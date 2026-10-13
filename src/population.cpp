#include <corerpg/world_state.hpp>

#include "world_state_internal.hpp"

#include <algorithm>
#include <limits>
#include <type_traits>

namespace corerpg {

PopulationChange WorldState::declare_population(EntityId group, std::uint64_t total) {
    if (!contains(group)) {
        return PopulationChange::group_not_found;
    }
    const auto position =
        std::lower_bound(populations_.begin(), populations_.end(), group,
                         [](const auto& record, EntityId id) { return record.group < id; });
    if (position != populations_.end() && position->group == group) {
        return position->total == total ? PopulationChange::no_change
                                        : PopulationChange::population_conflict;
    }
    if (!detail::revision_available(revision_)) {
        return PopulationChange::revision_exhausted;
    }
    static_assert(std::is_nothrow_move_constructible_v<PopulationRecord>);
    static_assert(std::is_nothrow_move_assignable_v<PopulationRecord>);
    populations_.insert(position, {group, total, total});
    ++revision_;
    return PopulationChange::applied;
}

std::optional<PopulationRecord> WorldState::population_of(EntityId group) const noexcept {
    const auto position =
        std::lower_bound(populations_.begin(), populations_.end(), group,
                         [](const auto& record, EntityId id) { return record.group < id; });
    return position != populations_.end() && position->group == group ? std::optional{*position}
                                                                      : std::nullopt;
}

std::optional<EntityId> WorldState::population_origin(EntityId entity) const noexcept {
    const auto position = std::lower_bound(
        population_memberships_.begin(), population_memberships_.end(), entity,
        [](const auto& membership, EntityId id) { return membership.entity < id; });
    return position != population_memberships_.end() && position->entity == entity
               ? std::optional{position->population}
               : std::nullopt;
}

MaterializationResult WorldState::materialize_population(EntityId group, std::uint64_t count,
                                                         std::uint64_t max_batch) {
    if (!contains(group)) {
        return {MaterializationStatus::group_not_found, {}, 0};
    }
    const auto position =
        std::lower_bound(populations_.begin(), populations_.end(), group,
                         [](const auto& record, EntityId id) { return record.group < id; });
    if (position == populations_.end() || position->group != group) {
        return {MaterializationStatus::population_not_found, {}, 0};
    }
    if (count == 0) {
        return {MaterializationStatus::no_change, {}, 0};
    }
    if (count > max_batch ||
        count > population_memberships_.max_size() - population_memberships_.size()) {
        return {MaterializationStatus::batch_limit_exceeded, {}, 0};
    }
    if (count > position->latent) {
        return {MaterializationStatus::insufficient_latent, {}, 0};
    }
    constexpr auto maximum = std::numeric_limits<std::uint64_t>::max();
    if (count > maximum - last_entity_id_) {
        return {MaterializationStatus::identity_exhausted, {}, 0};
    }
    if (count > maximum - revision_) {
        return {MaterializationStatus::revision_exhausted, {}, 0};
    }

    // This is the only potentially throwing publication prerequisite. A failed
    // reserve preserves all records; scalar-only appends below cannot allocate.
    static_assert(std::is_nothrow_move_constructible_v<PopulationMembership>);
    static_assert(std::is_nothrow_move_assignable_v<PopulationMembership>);
    static_assert(noexcept(PopulationMembership{EntityId{}, EntityId{}}));
    population_memberships_.reserve(population_memberships_.size() +
                                    static_cast<std::size_t>(count));
    const EntityId first{last_entity_id_ + 1U};
    for (std::uint64_t i = 0; i < count; ++i) {
        population_memberships_.push_back({EntityId{first.value() + i}, group});
    }
    last_entity_id_ += count;
    revision_ += count;
    position->latent -= count;
    return {MaterializationStatus::applied, first, count};
}

} // namespace corerpg
