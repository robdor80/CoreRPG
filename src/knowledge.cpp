#include <corerpg/world_state.hpp>

#include "knowledge_internal.hpp"
#include "world_state_internal.hpp"

#include <algorithm>
#include <type_traits>

namespace corerpg {

std::span<const KnowledgeRecord> WorldState::knowledge_of(EntityId observer) const noexcept {
    const auto begin = std::lower_bound(knowledge_.begin(), knowledge_.end(), observer,
        [](const auto& record, EntityId id) { return record.observer < id; });
    const auto end = std::upper_bound(begin, knowledge_.end(), observer,
        [](EntityId id, const auto& record) { return id < record.observer; });
    return std::span{knowledge_}.subspan(static_cast<std::size_t>(begin - knowledge_.begin()),
                                      static_cast<std::size_t>(end - begin));
}

KnowledgeChange WorldState::learn_fact(EntityId observer, FactKey key) {
    if (!contains(observer)) return KnowledgeChange::observer_not_found;
    if (!contains(key.subject)) return KnowledgeChange::subject_not_found;
    if (!detail::valid_fact_key(key) ||
        (key.kind == FactKind::relation && !contains(key.target))) return KnowledgeChange::invalid_fact;
    const auto records = knowledge_of(observer);
    if (key.kind != FactKind::existence && !detail::known_entity(records, key.subject))
        return KnowledgeChange::subject_unknown;
    // Detect unchanged definitions before copying their text (no-op never allocates).
    const auto* old = detail::find_fact(records, key);
    if (old && key.kind == FactKind::definition) {
        const auto* id = definition_of(key.subject);
        if ((!id && !old->present) || (id && old->present && id->value() == old->definition_id()))
            return KnowledgeChange::no_change;
    }
    // Validate references before allocating definition text or changing any table.
    const auto reference = key.kind == FactKind::containment ? container_of(key.subject).container
        : key.kind == FactKind::population_origin ? population_origin(key.subject).value_or(EntityId{})
        : key.kind == FactKind::relation ? key.target : EntityId{};
    if (reference.is_valid() && !detail::known_entity(records, reference))
        return KnowledgeChange::reference_unknown;
    // Non-definition values are scalar and can be compared before revision checking.
    if (key.kind != FactKind::definition) {
        const auto value = detail::current_fact(*this, key);
        if (old && *old == value) return KnowledgeChange::no_change;
    }
    if (!detail::revision_available(revision_)) return KnowledgeChange::revision_exhausted;
    auto value = detail::current_fact(*this, key);
    const auto position = std::lower_bound(knowledge_.begin(), knowledge_.end(), std::pair{observer, key},
        [](const auto& record, const auto& id) { return detail::knowledge_key(record) < id; });
    static_assert(std::is_nothrow_move_constructible_v<KnowledgeRecord>);
    static_assert(std::is_nothrow_move_assignable_v<KnowledgeRecord>);
    if (position != knowledge_.end() && detail::knowledge_key(*position) == std::pair{observer, key})
        position->fact = std::move(value);
    else knowledge_.insert(position, {observer, std::move(value)});
    ++revision_;
    return KnowledgeChange::applied;
}

KnowledgeChange WorldState::revoke_fact(EntityId observer, FactKey key) noexcept {
    if (!contains(observer)) return KnowledgeChange::observer_not_found;
    if (!contains(key.subject)) return KnowledgeChange::subject_not_found;
    if (!detail::valid_fact_key(key) ||
        (key.kind == FactKind::relation && !contains(key.target))) return KnowledgeChange::invalid_fact;
    const auto records = knowledge_of(observer);
    if (!detail::find_fact(records, key)) return KnowledgeChange::no_change;
    if (!detail::revision_available(revision_)) return KnowledgeChange::revision_exhausted;
    std::erase_if(knowledge_, [&](const auto& record) {
        if (record.observer != observer) return false;
        if (key.kind != FactKind::existence) return record.fact.key == key;
        return record.fact.key.subject == key.subject || record.fact.reference == key.subject ||
               record.fact.key.target == key.subject;
    });
    ++revision_;
    return KnowledgeChange::applied;
}

KnowledgeChange WorldState::revoke_knowledge(EntityId observer) noexcept {
    if (!contains(observer)) return KnowledgeChange::observer_not_found;
    if (knowledge_of(observer).empty()) return KnowledgeChange::no_change;
    if (!detail::revision_available(revision_)) return KnowledgeChange::revision_exhausted;
    std::erase_if(knowledge_, [observer](const auto& record) { return record.observer == observer; });
    ++revision_;
    return KnowledgeChange::applied;
}

} // namespace corerpg
