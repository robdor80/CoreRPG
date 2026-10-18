#include <corerpg/visibility.hpp>
#include <corerpg/world_state.hpp>

#include "knowledge_internal.hpp"

#include <algorithm>

namespace corerpg {

const KnownFact* ObserverView::find(FactKey key) const noexcept {
    const auto it = std::lower_bound(facts_.begin(), facts_.end(), key,
        [](const auto& fact, FactKey value) { return fact.key < value; });
    return it != facts_.end() && it->key == key ? &*it : nullptr;
}

std::optional<EntityId> ObserverView::next_entity_id(EntityId after) const noexcept {
    auto it = std::upper_bound(facts_.begin(), facts_.end(), after,
        [](EntityId id, const auto& fact) { return id < fact.key.subject; });
    for (; it != facts_.end(); ++it)
        if (it->key.kind == FactKind::existence) return it->key.subject;
    return std::nullopt;
}

ObserverView Visibility::project(const WorldState& world, EntityId observer,
                                 std::span<const FactKey> temporary_grants) {
    ObserverView view;
    if (!world.contains(observer)) return view;
    const auto records = world.knowledge_of(observer);
    view.facts_.reserve(records.size());
    for (const auto& record : records) view.facts_.push_back(record.fact);
    const auto put = [&](KnownFact fact) {
        const auto it = std::lower_bound(view.facts_.begin(), view.facts_.end(), fact.key,
            [](const auto& value, FactKey key) { return value.key < key; });
        if (it != view.facts_.end() && it->key == fact.key) *it = std::move(fact);
        else view.facts_.insert(it, std::move(fact));
    };
    // Authorize explicit existence first, independently of grant order.
    for (const auto key : temporary_grants) {
        if (key.kind == FactKind::existence && detail::valid_fact_key(key) && world.contains(key.subject))
            put(detail::current_fact(world, key));
    }
    for (const auto key : temporary_grants) {
        if (key.kind == FactKind::existence || !detail::valid_fact_key(key) || !view.knows(key.subject)) continue;
        if (key.kind == FactKind::relation && !view.knows(key.target)) continue;
        auto value = detail::current_fact(world, key);
        if (value.reference.is_valid() && !view.knows(value.reference)) continue;
        put(std::move(value));
    }
    return view;
}
} // namespace corerpg
