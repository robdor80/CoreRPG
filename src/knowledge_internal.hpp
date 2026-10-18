#pragma once

#include <corerpg/definition_id.hpp>
#include <corerpg/world_state.hpp>

#include <algorithm>
#include <tuple>

namespace corerpg::detail {

inline auto knowledge_key(const KnowledgeRecord& record) noexcept {
    return std::pair{record.observer, record.fact.key};
}

inline bool valid_fact_key(FactKey key) noexcept {
    if (!key.subject.is_valid()) return false;
    switch (key.kind) {
    case FactKind::relation: return key.relation_kind.is_valid() && key.target.is_valid();
    case FactKind::existence:
    case FactKind::definition:
    case FactKind::containment:
    case FactKind::population:
    case FactKind::population_origin:
        return !key.relation_kind.is_valid() && !key.target.is_valid();
    }
    return false;
}

inline const KnownFact* find_fact(std::span<const KnowledgeRecord> records, FactKey key) noexcept {
    const auto it = std::lower_bound(records.begin(), records.end(), key,
        [](const auto& record, FactKey value) { return record.fact.key < value; });
    return it != records.end() && it->fact.key == key ? &it->fact : nullptr;
}

inline bool known_entity(std::span<const KnowledgeRecord> records, EntityId entity) noexcept {
    return find_fact(records, {entity}) != nullptr;
}

/// Caller validates keys and existence. Captures only the requested structural value.
inline KnownFact current_fact(const WorldState& world, FactKey key) {
    KnownFact result;
    result.key = key;
    switch (key.kind) {
    case FactKind::existence: break;
    case FactKind::definition:
        if (const auto* id = world.definition_of(key.subject)) {
            const auto text = id->value();
            result.definition = std::make_shared<const std::string>(text.data(), text.size());
        } else result.present = false;
        break;
    case FactKind::containment: {
        const auto link = world.container_of(key.subject);
        result.present = link.status == ContainmentStatus::contained;
        result.reference = link.container;
        break;
    }
    case FactKind::relation:
        result.present = world.has_relation({key.subject, key.relation_kind, key.target});
        break;
    case FactKind::population:
        if (const auto value = world.population_of(key.subject)) {
            result.total = value->total;
            result.latent = value->latent;
        } else result.present = false;
        break;
    case FactKind::population_origin:
        if (const auto value = world.population_origin(key.subject)) result.reference = *value;
        else result.present = false;
        break;
    }
    return result;
}

inline bool canonical_fact(const KnownFact& fact, std::uint64_t last) noexcept {
    if (!valid_fact_key(fact.key) || fact.key.subject.value() > last ||
        fact.key.target.value() > last || fact.reference.value() > last) return false;
    if (fact.key.kind != FactKind::definition && fact.definition) return false;
    if (fact.key.kind != FactKind::population && (fact.total != 0 || fact.latent != 0)) return false;
    if (fact.key.kind != FactKind::containment && fact.key.kind != FactKind::population_origin &&
        fact.reference.is_valid()) return false;
    switch (fact.key.kind) {
    case FactKind::existence: return fact.present;
    case FactKind::definition:
        return fact.present ? fact.definition && DefinitionId::is_valid_text(fact.definition_id())
                            : !fact.definition;
    case FactKind::containment:
        return fact.present ? fact.reference.is_valid() && fact.reference != fact.key.subject
                            : !fact.reference.is_valid();
    case FactKind::population_origin:
        return fact.present ? fact.reference.is_valid() && fact.reference < fact.key.subject
                            : !fact.reference.is_valid();
    case FactKind::relation: return true;
    case FactKind::population:
        return fact.present ? fact.latent <= fact.total : fact.total == 0 && fact.latent == 0;
    }
    return false;
}

} // namespace corerpg::detail
