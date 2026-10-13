#include <corerpg/persistence.hpp>

#include "persistence_internal.hpp"

#include <algorithm>

namespace corerpg {

CampaignSnapshot Persistence::capture(const WorldState& world, const RandomStreams& random) {
    CampaignSnapshot snapshot;
    snapshot.last_entity_id = world.last_entity_id_;
    snapshot.revision = world.revision_;
    snapshot.master_seed = random.master_seed_;
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

PersistenceStatus Persistence::validate(const CampaignSnapshot& snapshot, PersistenceLimits limits) {
    if (snapshot.format_version != save_format_version) {
        return PersistenceStatus::unsupported_version;
    }
    if (snapshot.rng_policy != save_rng_policy) {
        return PersistenceStatus::unsupported_rng_policy;
    }
    if (!detail::save_size(snapshot.containment.size(), snapshot.streams.size(), limits)) {
        return PersistenceStatus::limit_exceeded;
    }
    const auto last = snapshot.last_entity_id;
    if (snapshot.revision < last || (last == 0 && snapshot.revision != 0) ||
        snapshot.containment.size() > snapshot.revision - last) {
        return PersistenceStatus::invalid_world;
    }
    EntityId previous;
    for (const auto& link : snapshot.containment) {
        if (!link.entity.is_valid() || !link.container.is_valid() ||
            link.entity.value() > last || link.container.value() > last ||
            link.entity == link.container) {
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

    // Iterative coloring: every link is visited and finalized once. Only sparse
    // records consume memory, never the implicit entity interval or a call stack.
    const auto& links = snapshot.containment;
    const auto parent_index = [&](std::size_t index) {
        const auto parent = links[index].container;
        const auto it = std::lower_bound(links.begin(), links.end(), parent,
            [](const auto& link, EntityId id) { return link.entity < id; });
        return it == links.end() || it->entity != parent
            ? links.size() : static_cast<std::size_t>(it - links.begin());
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
        return {status, {}};
    }
    std::vector<std::uint8_t> bytes;
    bytes.reserve(*detail::save_size(snapshot.containment.size(), snapshot.streams.size(), limits));
    bytes.insert(bytes.end(), detail::save_magic.begin(), detail::save_magic.end());
    detail::append_integer(bytes, snapshot.format_version, 4);
    detail::append_integer(bytes, snapshot.rng_policy, 4);
    detail::append_integer(bytes, snapshot.last_entity_id, 8);
    detail::append_integer(bytes, snapshot.revision, 8);
    detail::append_integer(bytes, snapshot.master_seed, 8);
    detail::append_integer(bytes, snapshot.containment.size(), 8);
    detail::append_integer(bytes, snapshot.streams.size(), 8);
    for (const auto& link : snapshot.containment) {
        detail::append_integer(bytes, link.entity.value(), 8);
        detail::append_integer(bytes, link.container.value(), 8);
    }
    for (const auto& stream : snapshot.streams) {
        detail::append_integer(bytes, stream.id, 8);
        detail::append_integer(bytes, stream.state, 8);
    }
    const auto checksum = detail::save_checksum(bytes);
    detail::append_integer(bytes, checksum, 4);
    return {PersistenceStatus::success, std::move(bytes)};
}

} // namespace corerpg
