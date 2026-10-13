#include <corerpg/content_catalog.hpp>
#include <corerpg/world_state.hpp>

#include "world_state_internal.hpp"

#include <algorithm>
#include <memory>
#include <type_traits>

namespace corerpg {

DefinitionBindingChange WorldState::bind_definition(EntityId entity, const ContentCatalog& catalog,
                                                    const DefinitionId& definition) {
    if (!contains(entity)) {
        return DefinitionBindingChange::entity_not_found;
    }
    if (!catalog.contains(definition)) {
        return DefinitionBindingChange::definition_not_found;
    }
    const auto position =
        std::lower_bound(definitions_.begin(), definitions_.end(), entity,
                         [](const auto& binding, EntityId id) { return binding.first < id; });
    if (position != definitions_.end() && position->first == entity) {
        return *position->second == definition ? DefinitionBindingChange::no_change
                                               : DefinitionBindingChange::definition_conflict;
    }
    if (!detail::revision_available(revision_)) {
        return DefinitionBindingChange::revision_exhausted;
    }
    // Copy inside the immutable value, so allocation exceptions never modify inputs.
    const auto value = std::make_shared<const DefinitionId>(definition);
    definitions_.insert(position, {entity, value});
    ++revision_;
    return DefinitionBindingChange::applied;
}

const DefinitionId* WorldState::definition_of(EntityId entity) const noexcept {
    const auto position =
        std::lower_bound(definitions_.begin(), definitions_.end(), entity,
                         [](const auto& binding, EntityId id) { return binding.first < id; });
    return position != definitions_.end() && position->first == entity ? position->second.get()
                                                                       : nullptr;
}

RelationChange WorldState::add_relation(EntityRelation relation) {
    if (!contains(relation.source)) {
        return RelationChange::source_not_found;
    }
    if (!contains(relation.target)) {
        return RelationChange::target_not_found;
    }
    if (!relation.kind.is_valid()) {
        return RelationChange::invalid_kind;
    }
    const auto position = std::lower_bound(relations_.begin(), relations_.end(), relation);
    if (position != relations_.end() && *position == relation) {
        return RelationChange::no_change;
    }
    if (!detail::revision_available(revision_)) {
        return RelationChange::revision_exhausted;
    }
    static_assert(std::is_nothrow_move_constructible_v<EntityRelation>);
    static_assert(std::is_nothrow_move_assignable_v<EntityRelation>);
    relations_.insert(position, relation);
    ++revision_;
    return RelationChange::applied;
}

RelationChange WorldState::remove_relation(EntityRelation relation) noexcept {
    if (!contains(relation.source)) {
        return RelationChange::source_not_found;
    }
    if (!contains(relation.target)) {
        return RelationChange::target_not_found;
    }
    if (!relation.kind.is_valid()) {
        return RelationChange::invalid_kind;
    }
    const auto position = std::lower_bound(relations_.begin(), relations_.end(), relation);
    if (position == relations_.end() || *position != relation) {
        return RelationChange::no_change;
    }
    if (!detail::revision_available(revision_)) {
        return RelationChange::revision_exhausted;
    }
    relations_.erase(position);
    ++revision_;
    return RelationChange::applied;
}

bool WorldState::has_relation(EntityRelation relation) const noexcept {
    return std::binary_search(relations_.begin(), relations_.end(), relation);
}

std::span<const EntityRelation> WorldState::relations_from(EntityId source) const noexcept {
    const auto begin =
        std::lower_bound(relations_.begin(), relations_.end(), source,
                         [](const auto& relation, EntityId id) { return relation.source < id; });
    const auto end =
        std::upper_bound(begin, relations_.end(), source,
                         [](EntityId id, const auto& relation) { return id < relation.source; });
    return std::span{relations_}.subspan(static_cast<std::size_t>(begin - relations_.begin()),
                                         static_cast<std::size_t>(end - begin));
}

} // namespace corerpg
