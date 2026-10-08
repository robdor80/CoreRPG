#pragma once

#include <corerpg/world_state.hpp>

#include <algorithm>
#include <cstdint>
#include <limits>
#include <type_traits>

namespace corerpg::detail {

using ContainmentLinks = std::vector<std::pair<EntityId, EntityId>>;
static_assert(std::is_nothrow_move_constructible_v<ContainmentLinks::value_type>);
static_assert(std::is_nothrow_move_assignable_v<ContainmentLinks::value_type>);

[[nodiscard]] constexpr bool revision_available(std::uint64_t revision) noexcept {
    return revision != std::numeric_limits<std::uint64_t>::max();
}

// Internal production transition, also exercised directly at numeric boundaries.
[[nodiscard]] constexpr EntityId advance_entity_creation(EntityId::value_type& last_entity_id,
                                                        std::uint64_t& revision) noexcept {
    if (last_entity_id == std::numeric_limits<EntityId::value_type>::max() ||
        !revision_available(revision)) {
        return EntityId{};
    }

    ++last_entity_id;
    ++revision;
    return EntityId{last_entity_id};
}

[[nodiscard]] inline auto find_containment(const ContainmentLinks& links, EntityId entity) noexcept {
    return std::lower_bound(links.begin(), links.end(), entity,
                            [](const auto& link, EntityId id) { return link.first < id; });
}

// Both production operations and boundary tests use these complete transitions.
[[nodiscard]] inline ContainmentChange place_entity(ContainmentLinks& links,
                                                    EntityId::value_type last_entity_id,
                                                    std::uint64_t& revision, EntityId entity,
                                                    EntityId container) {
    if (!entity.is_valid() || entity.value() > last_entity_id) {
        return ContainmentChange::entity_not_found;
    }
    if (!container.is_valid() || container.value() > last_entity_id) {
        return ContainmentChange::container_not_found;
    }
    if (entity == container) {
        return ContainmentChange::self_containment;
    }

    // Existing links form a forest. Follow the proposed parent's chain iteratively.
    for (auto ancestor = container;;) {
        if (ancestor == entity) {
            return ContainmentChange::cycle_detected;
        }
        const auto link = find_containment(links, ancestor);
        if (link == links.end() || link->first != ancestor) {
            break;
        }
        ancestor = link->second;
    }

    const auto position = find_containment(links, entity);
    const bool has_link = position != links.end() && position->first == entity;
    if (has_link && position->second == container) {
        return ContainmentChange::no_change;
    }
    if (!revision_available(revision)) {
        return ContainmentChange::revision_exhausted;
    }

    if (has_link) {
        links[static_cast<ContainmentLinks::size_type>(position - links.cbegin())].second = container;
    } else {
        // vector insertion has the strong guarantee for these nonthrowing value types.
        links.insert(position, {entity, container});
    }
    ++revision;
    return ContainmentChange::applied;
}

[[nodiscard]] inline ContainmentChange remove_containment(ContainmentLinks& links,
                                                         EntityId::value_type last_entity_id,
                                                         std::uint64_t& revision,
                                                         EntityId entity) noexcept {
    if (!entity.is_valid() || entity.value() > last_entity_id) {
        return ContainmentChange::entity_not_found;
    }
    const auto position = find_containment(links, entity);
    if (position == links.end() || position->first != entity) {
        return ContainmentChange::no_change;
    }
    if (!revision_available(revision)) {
        return ContainmentChange::revision_exhausted;
    }
    links.erase(position);
    ++revision;
    return ContainmentChange::applied;
}

} // namespace corerpg::detail
