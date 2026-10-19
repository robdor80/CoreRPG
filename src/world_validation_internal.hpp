#pragma once

#include "knowledge_internal.hpp"
#include <corerpg/persistence.hpp>

#include <algorithm>
#include <memory>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

namespace corerpg::detail {

// Same validation, borrowed adapters: never clone/serialize a live world.
struct WorldValidationView final {
    std::uint64_t last_entity_id, revision;
    std::span<const std::pair<EntityId, EntityId>> containment;
    std::span<const std::pair<EntityId, std::shared_ptr<const DefinitionId>>> definitions;
    std::span<const EntityRelation> relations;
    std::span<const PopulationRecord> populations;
    std::span<const PopulationMembership> population_memberships;
    std::span<const KnowledgeRecord> knowledge;
    std::uint32_t format_version{save_format_version_v3};
    std::span<const RandomStreamSnapshot> streams{};
};
inline EntityId validation_entity(const ContainmentSnapshot& v) noexcept { return v.entity; }
inline EntityId validation_entity(const DefinitionBindingSnapshot& v) noexcept { return v.entity; }
template <class T>
EntityId validation_entity(const std::pair<EntityId, T>& v) noexcept { return v.first; }
inline EntityId validation_container(const ContainmentSnapshot& v) noexcept { return v.container; }
inline EntityId validation_container(const std::pair<EntityId, EntityId>& v) noexcept { return v.second; }
inline std::string_view validation_definition(const DefinitionBindingSnapshot& v) noexcept {
    return v.definition_id;
}
inline std::string_view validation_definition(
    const std::pair<EntityId, std::shared_ptr<const DefinitionId>>& v) noexcept {
    return v.second ? v.second->value() : std::string_view{};
}

// Validates scalar structure, ordering and references before binary searches.
// Scratch allocations are O(populations + links), never O(last_entity_id).
// Caller controls budgets and handles exceptions; first failure order is stable.
template <class State>
PersistenceStatus validate_world_structure(const State& snapshot) {
    const auto last = snapshot.last_entity_id;
    if (snapshot.revision < last || (last == 0 && snapshot.revision != 0) ||
        (snapshot.format_version == save_format_version && last <= 1 &&
         snapshot.revision != last) ||
        snapshot.containment.size() > snapshot.revision - last) {
        return PersistenceStatus::invalid_world;
    }
    auto remaining_revision = snapshot.revision - last - snapshot.containment.size();
    for (const auto count :
         {snapshot.definitions.size(), snapshot.relations.size(), snapshot.populations.size(),
          snapshot.knowledge.size()}) {
        if (count > remaining_revision) {
            return PersistenceStatus::invalid_world;
        }
        remaining_revision -= count;
    }
    EntityId previous;
    for (const auto& link : snapshot.containment) {
        const auto entity = validation_entity(link), container = validation_container(link);
        if (!entity.is_valid() || !container.is_valid() || entity.value() > last ||
            container.value() > last || entity == container) {
            return PersistenceStatus::invalid_link;
        }
        if (validation_entity(link) <= previous) {
            return PersistenceStatus::noncanonical_links;
        }
        previous = validation_entity(link);
    }
    for (std::size_t i = 1; i < snapshot.streams.size(); ++i) {
        if (snapshot.streams[i - 1].id >= snapshot.streams[i].id) {
            return PersistenceStatus::noncanonical_streams;
        }
    }

    previous = {};
    for (const auto& binding : snapshot.definitions) {
        if (!validation_entity(binding).is_valid() || validation_entity(binding).value() > last ||
            !DefinitionId::is_valid_text(validation_definition(binding))) {
            return PersistenceStatus::invalid_definition;
        }
        if (validation_entity(binding) <= previous) {
            return PersistenceStatus::noncanonical_definitions;
        }
        previous = validation_entity(binding);
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

    for (std::size_t i = 0; i < snapshot.knowledge.size(); ++i) {
        const auto& record = snapshot.knowledge[i];
        if (!record.observer.is_valid() || record.observer.value() > last ||
            !detail::canonical_fact(record.fact, last)) return PersistenceStatus::invalid_knowledge;
        if (i != 0 && detail::knowledge_key(snapshot.knowledge[i - 1]) >= detail::knowledge_key(record))
            return PersistenceStatus::noncanonical_knowledge;
    }
    const auto knows = [&](EntityId observer, EntityId subject) {
        const auto key = std::pair{observer, FactKey{subject, FactKind::existence, {}, {}}};
        const auto it = std::lower_bound(snapshot.knowledge.begin(), snapshot.knowledge.end(), key,
            [](const auto& record, const auto& value) { return detail::knowledge_key(record) < value; });
        return it != snapshot.knowledge.end() && detail::knowledge_key(*it) == key;
    };
    for (const auto& record : snapshot.knowledge) {
        const auto& fact = record.fact;
        if (!knows(record.observer, fact.key.subject) ||
            (fact.key.target.is_valid() && !knows(record.observer, fact.key.target)) ||
            (fact.reference.is_valid() && !knows(record.observer, fact.reference)))
            return PersistenceStatus::invalid_knowledge;
    }

    // Iterative coloring: every link is visited and finalized once. Only sparse
    // records consume memory, never the implicit entity interval or a call stack.
    const auto& links = snapshot.containment;
    const auto parent_index = [&](std::size_t index) {
        const auto parent = validation_container(links[index]);
        const auto it =
            std::lower_bound(links.begin(), links.end(), parent,
                             [](const auto& link, EntityId id) { return validation_entity(link) < id; });
        return it == links.end() || validation_entity(*it) != parent
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

} // namespace corerpg::detail
