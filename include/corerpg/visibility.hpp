#pragma once

#include <corerpg/knowledge.hpp>

#include <optional>
#include <span>
#include <vector>

namespace corerpg {
class WorldState;

/// Provenance available at projection time, not an acquisition timestamp/source.
enum class FactProvenance : std::uint8_t { remembered, temporary_grant };

/// Public data only. Ownership and reference counts of remembered text stay private.
class VisibleFact final {
  public:
    VisibleFact() noexcept = default;
    VisibleFact(const VisibleFact&) noexcept = default;
    VisibleFact(VisibleFact&&) noexcept = default;
    VisibleFact& operator=(const VisibleFact&) noexcept = default;
    VisibleFact& operator=(VisibleFact&&) noexcept = default;
    FactKey key;
    bool present{};
    EntityId reference;
    std::uint64_t total{};
    std::uint64_t latent{};
    [[nodiscard]] std::string_view definition_id() const noexcept {
        return definition_ ? std::string_view{*definition_} : std::string_view{};
    }
    /// A grant was current when projected; detached copies do not refresh.
    [[nodiscard]] FactProvenance provenance() const noexcept { return provenance_; }
    bool operator==(const VisibleFact& other) const noexcept {
        return key == other.key && present == other.present && reference == other.reference &&
               total == other.total && latent == other.latent && definition_id() == other.definition_id() &&
               provenance_ == other.provenance_;
    }

  private:
    friend class Visibility;
    explicit VisibleFact(const KnownFact& value, FactProvenance provenance) noexcept
        : key{value.key}, present{value.present}, reference{value.reference}, total{value.total},
          latent{value.latent}, definition_{value.definition}, provenance_{provenance} {}
    std::shared_ptr<const std::string> definition_;
    FactProvenance provenance_{FactProvenance::remembered};
};

/// Detached read-only value for untrusted consumers. No world/revision/RNG access.
class ObserverView final {
  public:
    ObserverView() = default;
    ObserverView(const ObserverView&) = default;
    ObserverView& operator=(const ObserverView& other) {
        if (this != &other) { ObserverView copy{other}; facts_.swap(copy.facts_); }
        return *this;
    }
    ObserverView(ObserverView&& other) { facts_.swap(other.facts_); }
    ObserverView& operator=(ObserverView&& other) noexcept {
        facts_.swap(other.facts_);
        return *this;
    }
    [[nodiscard]] std::span<const VisibleFact> facts() const noexcept { return facts_; }
    /// Enumerates only explicit known existence records, in increasing opaque ID order.
    [[nodiscard]] std::optional<EntityId> next_entity_id(EntityId after = {}) const noexcept;
    [[nodiscard]] const VisibleFact* find(FactKey key) const noexcept;
    [[nodiscard]] bool knows(EntityId entity) const noexcept { return find({entity}) != nullptr; }
    bool operator==(const ObserverView& other) const noexcept { return facts_ == other.facts_; }

  private:
    friend class Visibility;
    // Count constructor propagates checked-iterator proxy allocation failures on MSVC.
    std::vector<VisibleFact> facts_ = std::vector<VisibleFact>(0);
};

class Visibility final {
  public:
    /// PRIVILEGED builder. Caller authenticates observer and supplies trusted temporary
    /// grants, never accepts grants from an untrusted client. Invalid/unresolvable grants
    /// are omitted uniformly. No refresh of ungranted memories, mutation or RNG.
    [[nodiscard]] static ObserverView project(const WorldState& world, EntityId observer,
                                             std::span<const FactKey> temporary_grants = {});
};
} // namespace corerpg
