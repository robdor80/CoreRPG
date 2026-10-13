#include <corerpg/content_catalog.hpp>
#include <corerpg/persistence.hpp>

#include "persistence_internal.hpp"

#include <algorithm>
#include <type_traits>
#include <utility>

namespace corerpg {

WorldState::WorldState(const CampaignSnapshot& snapshot, const ContentCatalog* catalog)
    : last_entity_id_{snapshot.last_entity_id}, revision_{snapshot.revision},
      containment_(snapshot.containment.size()), definitions_(0), relations_(snapshot.relations),
      populations_(snapshot.populations), population_memberships_(snapshot.population_memberships) {
    for (std::size_t i = 0; i < snapshot.containment.size(); ++i) {
        containment_[i] = {snapshot.containment[i].entity, snapshot.containment[i].container};
    }
    definitions_.reserve(snapshot.definitions.size());
    for (const auto& binding : snapshot.definitions) {
        const auto& id = catalog->find_by_value(binding.definition_id)->id();
        definitions_.emplace_back(binding.entity, std::make_shared<const DefinitionId>(id));
    }
}

RandomStreams::RandomStreams(const CampaignSnapshot& snapshot)
    : master_seed_{snapshot.master_seed}, streams_{std::less<stream_id_type>{}} {
    for (const auto& stream : snapshot.streams) {
        streams_.emplace(stream.id, DeterministicRandom{stream.state});
    }
}

CampaignSnapshot Persistence::capture(const WorldState& world, const RandomStreams& random) {
    CampaignSnapshot snapshot;
    snapshot.last_entity_id = world.last_entity_id_;
    snapshot.revision = world.revision_;
    snapshot.master_seed = random.master_seed_;
    if (!world.definitions_.empty() || !world.relations_.empty() || !world.populations_.empty() ||
        !world.population_memberships_.empty() ||
        (world.last_entity_id_ <= 1 && world.revision_ != world.last_entity_id_)) {
        snapshot.format_version = save_format_version_v2;
    }
    snapshot.definitions.reserve(world.definitions_.size());
    for (const auto& [entity, definition] : world.definitions_) {
        snapshot.definitions.emplace_back(entity, definition->value());
    }
    snapshot.relations = world.relations_;
    snapshot.populations = world.populations_;
    snapshot.population_memberships = world.population_memberships_;
    snapshot.containment.reserve(world.containment_.size());
    for (const auto& [entity, container] : world.containment_) {
        snapshot.containment.push_back({entity, container});
    }
    snapshot.streams.reserve(random.streams_.size());
    for (const auto& [id, stream] : random.streams_) {
        snapshot.streams.push_back({id, stream.state()});
    }
    return snapshot;
}

PersistenceStatus Persistence::validate(const CampaignSnapshot& snapshot,
                                        PersistenceLimits limits) {
    if (snapshot.format_version != save_format_version &&
        snapshot.format_version != save_format_version_v2) {
        return PersistenceStatus::unsupported_version;
    }
    if (snapshot.format_version == save_format_version &&
        (!snapshot.definitions.empty() || !snapshot.relations.empty() ||
         !snapshot.populations.empty() || !snapshot.population_memberships.empty())) {
        return PersistenceStatus::unexpected_fields;
    }
    if (snapshot.rng_policy != save_rng_policy) {
        return PersistenceStatus::unsupported_rng_policy;
    }
    if (!detail::encoded_save_size(snapshot, limits)) {
        return PersistenceStatus::limit_exceeded;
    }
    const auto last = snapshot.last_entity_id;
    if (snapshot.revision < last || (last == 0 && snapshot.revision != 0) ||
        (snapshot.format_version == save_format_version && last <= 1 &&
         snapshot.revision != last) ||
        snapshot.containment.size() > snapshot.revision - last) {
        return PersistenceStatus::invalid_world;
    }
    auto remaining_revision = snapshot.revision - last - snapshot.containment.size();
    for (const auto count :
         {snapshot.definitions.size(), snapshot.relations.size(), snapshot.populations.size()}) {
        if (count > remaining_revision) {
            return PersistenceStatus::invalid_world;
        }
        remaining_revision -= count;
    }
    EntityId previous;
    for (const auto& link : snapshot.containment) {
        if (!link.entity.is_valid() || !link.container.is_valid() || link.entity.value() > last ||
            link.container.value() > last || link.entity == link.container) {
            return PersistenceStatus::invalid_link;
        }
        if (link.entity <= previous) {
            return PersistenceStatus::noncanonical_links;
        }
        previous = link.entity;
    }
    for (std::size_t i = 1; i < snapshot.streams.size(); ++i) {
        if (snapshot.streams[i - 1].id >= snapshot.streams[i].id) {
            return PersistenceStatus::noncanonical_streams;
        }
    }

    previous = {};
    for (const auto& binding : snapshot.definitions) {
        if (!binding.entity.is_valid() || binding.entity.value() > last ||
            !DefinitionId::is_valid_text(binding.definition_id)) {
            return PersistenceStatus::invalid_definition;
        }
        if (binding.entity <= previous) {
            return PersistenceStatus::noncanonical_definitions;
        }
        previous = binding.entity;
    }
    for (std::size_t i = 0; i < snapshot.relations.size(); ++i) {
        const auto& edge = snapshot.relations[i];
        if (!edge.source.is_valid() || !edge.target.is_valid() || !edge.kind.is_valid() ||
            edge.source.value() > last || edge.target.value() > last) {
            return PersistenceStatus::invalid_relation;
        }
        if (i != 0 && snapshot.relations[i - 1] >= edge) {
            return PersistenceStatus::noncanonical_relations;
        }
    }
    previous = {};
    for (const auto& population : snapshot.populations) {
        if (!population.group.is_valid() || population.group.value() > last ||
            population.latent > population.total) {
            return PersistenceStatus::invalid_population;
        }
        if (population.group <= previous) {
            return PersistenceStatus::noncanonical_populations;
        }
        previous = population.group;
    }
    std::vector<std::uint64_t> counts(snapshot.populations.size(), 0);
    previous = {};
    for (const auto& member : snapshot.population_memberships) {
        if (!member.entity.is_valid() || !member.population.is_valid() ||
            member.entity.value() > last || member.population >= member.entity) {
            return PersistenceStatus::invalid_membership;
        }
        if (member.entity <= previous) {
            return PersistenceStatus::noncanonical_memberships;
        }
        previous = member.entity;
        const auto position = std::lower_bound(
            snapshot.populations.begin(), snapshot.populations.end(), member.population,
            [](const auto& population, EntityId id) { return population.group < id; });
        if (position == snapshot.populations.end() || position->group != member.population) {
            return PersistenceStatus::invalid_membership;
        }
        ++counts[static_cast<std::size_t>(position - snapshot.populations.begin())];
    }
    for (std::size_t i = 0; i < snapshot.populations.size(); ++i) {
        if (counts[i] != snapshot.populations[i].total - snapshot.populations[i].latent) {
            return PersistenceStatus::invalid_population;
        }
    }

    // Iterative coloring: every link is visited and finalized once. Only sparse
    // records consume memory, never the implicit entity interval or a call stack.
    const auto& links = snapshot.containment;
    const auto parent_index = [&](std::size_t index) {
        const auto parent = links[index].container;
        const auto it =
            std::lower_bound(links.begin(), links.end(), parent,
                             [](const auto& link, EntityId id) { return link.entity < id; });
        return it == links.end() || it->entity != parent
                   ? links.size()
                   : static_cast<std::size_t>(it - links.begin());
    };
    std::vector<std::uint8_t> colors(links.size(), 0);
    for (std::size_t start = 0; start < links.size(); ++start) {
        if (colors[start] == 2) {
            continue;
        }
        auto index = start;
        while (index < links.size() && colors[index] == 0) {
            colors[index] = 1;
            index = parent_index(index);
        }
        if (index < links.size() && colors[index] == 1) {
            return PersistenceStatus::cycle_detected;
        }
        index = start;
        while (index < links.size() && colors[index] == 1) {
            colors[index] = 2;
            index = parent_index(index);
        }
    }
    return PersistenceStatus::success;
}

SerializedSnapshot Persistence::serialize(const CampaignSnapshot& snapshot,
                                          PersistenceLimits limits) {
    const auto status = validate(snapshot, limits);
    if (status != PersistenceStatus::success) {
        return {status};
    }
    std::vector<std::uint8_t> bytes(0);
    bytes.reserve(*detail::encoded_save_size(snapshot, limits));
    bytes.insert(bytes.end(), detail::save_magic.begin(), detail::save_magic.end());
    detail::append_integer(bytes, snapshot.format_version, 4);
    detail::append_integer(bytes, snapshot.rng_policy, 4);
    detail::append_integer(bytes, snapshot.last_entity_id, 8);
    detail::append_integer(bytes, snapshot.revision, 8);
    detail::append_integer(bytes, snapshot.master_seed, 8);
    detail::append_integer(bytes, snapshot.containment.size(), 8);
    detail::append_integer(bytes, snapshot.streams.size(), 8);
    if (snapshot.format_version == save_format_version_v2) {
        detail::append_integer(bytes, snapshot.definitions.size(), 8);
        detail::append_integer(bytes, snapshot.relations.size(), 8);
        detail::append_integer(bytes, snapshot.populations.size(), 8);
        detail::append_integer(bytes, snapshot.population_memberships.size(), 8);
    }
    for (const auto& link : snapshot.containment) {
        detail::append_integer(bytes, link.entity.value(), 8);
        detail::append_integer(bytes, link.container.value(), 8);
    }
    for (const auto& stream : snapshot.streams) {
        detail::append_integer(bytes, stream.id, 8);
        detail::append_integer(bytes, stream.state, 8);
    }
    if (snapshot.format_version == save_format_version_v2) {
        for (const auto& binding : snapshot.definitions) {
            detail::append_integer(bytes, binding.entity.value(), 8);
            detail::append_integer(bytes, binding.definition_id.size(), 4);
            for (const auto character : binding.definition_id) {
                bytes.push_back(static_cast<std::uint8_t>(character));
            }
        }
        for (const auto& edge : snapshot.relations) {
            detail::append_integer(bytes, edge.source.value(), 8);
            detail::append_integer(bytes, edge.kind.value(), 8);
            detail::append_integer(bytes, edge.target.value(), 8);
        }
        for (const auto& population : snapshot.populations) {
            detail::append_integer(bytes, population.group.value(), 8);
            detail::append_integer(bytes, population.total, 8);
            detail::append_integer(bytes, population.latent, 8);
        }
        for (const auto& member : snapshot.population_memberships) {
            detail::append_integer(bytes, member.entity.value(), 8);
            detail::append_integer(bytes, member.population.value(), 8);
        }
    }
    const auto checksum = detail::save_checksum(bytes);
    detail::append_integer(bytes, checksum, 4);
    return {PersistenceStatus::success, std::move(bytes)};
}

DecodedSnapshot Persistence::deserialize(std::span<const std::uint8_t> bytes,
                                         PersistenceLimits limits) {
    if (bytes.size() > limits.max_bytes) {
        return {PersistenceStatus::limit_exceeded, std::nullopt};
    }
    if (bytes.size() < detail::save_minimum_size) {
        return {PersistenceStatus::invalid_size, std::nullopt};
    }
    if (!std::equal(detail::save_magic.begin(), detail::save_magic.end(), bytes.begin())) {
        return {PersistenceStatus::invalid_magic, std::nullopt};
    }
    // Reads are bounded by the header check and then by the exact record counts.
    std::size_t position = detail::save_magic.size();
    const auto read = [&](unsigned width) {
        std::uint64_t value = 0;
        for (unsigned i = 0; i < width; ++i) {
            value |= static_cast<std::uint64_t>(bytes[position++]) << (8U * i);
        }
        return value;
    };
    const auto version = static_cast<std::uint32_t>(read(4));
    if (version != save_format_version && version != save_format_version_v2) {
        return {PersistenceStatus::unsupported_version, std::nullopt};
    }
    const bool extended = version == save_format_version_v2;
    if (extended && bytes.size() < detail::save_v2_minimum_size) {
        return {PersistenceStatus::invalid_size, std::nullopt};
    }
    const auto rng_policy = static_cast<std::uint32_t>(read(4));
    if (rng_policy != save_rng_policy) {
        return {PersistenceStatus::unsupported_rng_policy, std::nullopt};
    }
    const auto last = read(8);
    const auto revision = read(8);
    const auto master_seed = read(8);
    const auto link_count = read(8);
    const auto stream_count = read(8);
    const auto definition_count = extended ? read(8) : 0;
    const auto relation_count = extended ? read(8) : 0;
    const auto population_count = extended ? read(8) : 0;
    const auto membership_count = extended ? read(8) : 0;
    const auto expected_size =
        extended ? detail::save_v2_minimum_size_for(link_count, stream_count, definition_count,
                                                    relation_count, population_count,
                                                    membership_count, limits)
                 : detail::save_size(link_count, stream_count, limits);
    if (!expected_size) {
        return {PersistenceStatus::limit_exceeded, std::nullopt};
    }
    if ((!extended && *expected_size != bytes.size()) || *expected_size > bytes.size()) {
        return {PersistenceStatus::invalid_size, std::nullopt};
    }
    position = bytes.size() - 4U;
    const auto checksum = static_cast<std::uint32_t>(read(4));
    if (checksum != detail::save_checksum(bytes.first(bytes.size() - 4U))) {
        return {PersistenceStatus::checksum_mismatch, std::nullopt};
    }

    // Preflight variable-length records and their exact suffix without allocation.
    if (extended) {
        const auto body_end = bytes.size() - 4U;
        const auto tail = static_cast<std::size_t>(24U * relation_count + 24U * population_count +
                                                   16U * membership_count);
        position = detail::save_v2_header_size +
                   static_cast<std::size_t>(16U * (link_count + stream_count));
        for (std::uint64_t i = 0; i < definition_count; ++i) {
            if (position > body_end || body_end - position < 12U) {
                return {PersistenceStatus::invalid_size, std::nullopt};
            }
            static_cast<void>(read(8));
            const auto length = read(4);
            if (length > limits.max_definition_bytes) {
                return {PersistenceStatus::limit_exceeded, std::nullopt};
            }
            if (length > body_end - position) {
                return {PersistenceStatus::invalid_size, std::nullopt};
            }
            const std::string_view text{reinterpret_cast<const char*>(bytes.data() + position),
                                        static_cast<std::size_t>(length)};
            if (!DefinitionId::is_valid_text(text)) {
                return {PersistenceStatus::invalid_definition, std::nullopt};
            }
            position += static_cast<std::size_t>(length);
        }
        if (position > body_end || body_end - position != tail) {
            return {PersistenceStatus::invalid_size, std::nullopt};
        }
    }

    CampaignSnapshot snapshot;
    snapshot.format_version = version;
    snapshot.last_entity_id = last;
    snapshot.revision = revision;
    snapshot.master_seed = master_seed;
    if (link_count > snapshot.containment.max_size() ||
        stream_count > snapshot.streams.max_size() ||
        definition_count > snapshot.definitions.max_size() ||
        relation_count > snapshot.relations.max_size() ||
        population_count > snapshot.populations.max_size() ||
        membership_count > snapshot.population_memberships.max_size()) {
        return {PersistenceStatus::limit_exceeded, std::nullopt};
    }
    snapshot.containment.reserve(static_cast<std::size_t>(link_count));
    snapshot.streams.reserve(static_cast<std::size_t>(stream_count));
    snapshot.definitions.reserve(static_cast<std::size_t>(definition_count));
    snapshot.relations.reserve(static_cast<std::size_t>(relation_count));
    snapshot.populations.reserve(static_cast<std::size_t>(population_count));
    snapshot.population_memberships.reserve(static_cast<std::size_t>(membership_count));
    position = extended ? detail::save_v2_header_size : detail::save_header_size;
    for (std::uint64_t i = 0; i < link_count; ++i) {
        const auto entity = read(8);
        const auto container = read(8);
        snapshot.containment.push_back({EntityId{entity}, EntityId{container}});
    }
    for (std::uint64_t i = 0; i < stream_count; ++i) {
        const auto id = read(8);
        const auto state = read(8);
        snapshot.streams.push_back({id, state});
    }
    for (std::uint64_t i = 0; i < definition_count; ++i) {
        const EntityId entity{read(8)};
        const auto length = static_cast<std::size_t>(read(4));
        snapshot.definitions.emplace_back(
            entity,
            std::string_view{reinterpret_cast<const char*>(bytes.data() + position), length});
        position += length;
    }
    for (std::uint64_t i = 0; i < relation_count; ++i) {
        const EntityId source{read(8)};
        const RelationKind kind{read(8)};
        const EntityId target{read(8)};
        snapshot.relations.push_back({source, kind, target});
    }
    for (std::uint64_t i = 0; i < population_count; ++i) {
        const EntityId group{read(8)};
        const auto total = read(8);
        const auto latent = read(8);
        snapshot.populations.push_back({group, total, latent});
    }
    for (std::uint64_t i = 0; i < membership_count; ++i) {
        const EntityId entity{read(8)};
        const EntityId population{read(8)};
        snapshot.population_memberships.push_back({entity, population});
    }
    const auto status = validate(snapshot, limits);
    if (status != PersistenceStatus::success) {
        return {status, std::nullopt};
    }
    return {PersistenceStatus::success, std::move(snapshot)};
}

PersistenceStatus Persistence::restore(const CampaignSnapshot& snapshot, WorldState& world,
                                       RandomStreams& random, PersistenceLimits limits,
                                       const ContentCatalog* catalog) {
    const auto status = validate(snapshot, limits);
    if (status != PersistenceStatus::success) {
        return status;
    }

    if (!snapshot.definitions.empty() && catalog == nullptr) {
        return PersistenceStatus::catalog_required;
    }
    for (const auto& binding : snapshot.definitions) {
        if (catalog->find_by_value(binding.definition_id) == nullptr) {
            return PersistenceStatus::definition_not_found;
        }
    }
    WorldState replacement_world{snapshot, catalog};
    RandomStreams replacement_random{snapshot};

    // Nothing below can allocate or throw, including MSVC checked-iterator builds.
    static_assert(noexcept(world.containment_.swap(replacement_world.containment_)));
    static_assert(noexcept(random.streams_.swap(replacement_random.streams_)));
    static_assert(std::is_nothrow_swappable_v<std::uint64_t>);
    world.swap_state(replacement_world);
    std::swap(random.master_seed_, replacement_random.master_seed_);
    random.streams_.swap(replacement_random.streams_);
    return PersistenceStatus::success;
}

PersistenceStatus Persistence::load(std::span<const std::uint8_t> bytes, WorldState& world,
                                    RandomStreams& random, PersistenceLimits limits,
                                    const ContentCatalog* catalog) {
    const auto decoded = deserialize(bytes, limits);
    return decoded.status == PersistenceStatus::success
               ? restore(*decoded.snapshot, world, random, limits, catalog)
               : decoded.status;
}

} // namespace corerpg
