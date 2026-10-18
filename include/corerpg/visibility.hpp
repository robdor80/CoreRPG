#pragma once

#include <corerpg/knowledge.hpp>

#include <optional>
#include <span>
#include <vector>

namespace corerpg {
class WorldState;

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
    [[nodiscard]] std::span<const KnownFact> facts() const noexcept { return facts_; }
    /// Enumerates only explicit known existence records, in increasing opaque ID order.
    [[nodiscard]] std::optional<EntityId> next_entity_id(EntityId after = {}) const noexcept;
    [[nodiscard]] const KnownFact* find(FactKey key) const noexcept;
    [[nodiscard]] bool knows(EntityId entity) const noexcept { return find({entity}) != nullptr; }
    bool operator==(const ObserverView& other) const noexcept { return facts_ == other.facts_; }

  private:
    friend class Visibility;
    // Count constructor propagates checked-iterator proxy allocation failures on MSVC.
    std::vector<KnownFact> facts_ = std::vector<KnownFact>(0);
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
