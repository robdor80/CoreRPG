#pragma once

#include <corerpg/entity_structure.hpp>

#include <memory>
#include <string>
#include <string_view>

namespace corerpg {

/// Closed set of supported structural facts. No domain properties or inference.
enum class FactKind : std::uint8_t {
    existence = 1,
    definition = 2,
    containment = 3,
    relation = 4,
    population = 5,
    population_origin = 6,
};

/// relation_kind/target are nonzero only for an exact relation query.
struct FactKey final {
    EntityId subject;
    FactKind kind{FactKind::existence};
    RelationKind relation_kind;
    EntityId target;
    auto operator<=>(const FactKey&) const noexcept = default;
};

/// Detached remembered value, never a live lookup. No record means unknown;
/// present=false means known absence. Unused fields are canonically zero/null.
struct KnownFact final {
    FactKey key;
    bool present{true};
    EntityId reference; // direct container or population origin
    std::uint64_t total{};
    std::uint64_t latent{};
    std::shared_ptr<const std::string> definition;

    [[nodiscard]] std::string_view definition_id() const noexcept {
        return definition ? std::string_view{*definition} : std::string_view{};
    }
    bool operator==(const KnownFact& other) const noexcept {
        return key == other.key && present == other.present && reference == other.reference &&
               total == other.total && latent == other.latent &&
               static_cast<bool>(definition) == static_cast<bool>(other.definition) &&
               definition_id() == other.definition_id();
    }
};

struct KnowledgeRecord final {
    EntityId observer;
    KnownFact fact;
    bool operator==(const KnowledgeRecord&) const noexcept = default;
};

enum class KnowledgeChange : std::uint8_t {
    applied,
    no_change,
    observer_not_found,
    subject_not_found,
    invalid_fact,
    subject_unknown,
    reference_unknown,
    revision_exhausted,
};

} // namespace corerpg
