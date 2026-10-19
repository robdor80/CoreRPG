#pragma once

#include <corerpg/world_state.hpp>
#include <corerpg/persistence.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <utility>

namespace corerpg {
class ResourceDefinition;

inline constexpr std::uint32_t inspector_max_page_size = 256;

/// All Inspector types are PRIVILEGED technical data, never observer projections.
enum class InspectionStatus : std::uint8_t {
    success,
    invalid_limit,
    stale_revision,
    entity_not_found,
};
struct InspectionRequest final {
    std::uint64_t offset{};
    std::uint32_t limit{64};
    std::optional<WorldState::revision_type> expected_revision{};
};
struct InspectionProgress final {
    InspectionStatus status{InspectionStatus::success};
    WorldState::revision_type revision{};
    std::optional<std::uint64_t> next_offset{};
    bool operator==(const InspectionProgress&) const = default;
};

/// Borrowed canonical page. Discard before ANY world mutation/replacement.
template <class T> struct InspectionPage final {
    InspectionProgress progress;
    std::span<const T> records;
};
/// Implicit contiguous IDs [first, first + count). Empty has invalid first.
struct InspectionEntities final {
    InspectionProgress progress;
    EntityId first;
    std::uint32_t count{};
    bool operator==(const InspectionEntities&) const = default;
};
struct InspectionMetadata final {
    WorldState::revision_type revision{};
    WorldState::size_type entities{};
    std::size_t containment_links{}, definitions{}, relations{}, populations{}, memberships{}, knowledge{};
    bool operator==(const InspectionMetadata&) const = default;
};
struct InspectedEntity final {
    EntityId id;
    ContainmentResult containment;
    const DefinitionId* definition{}; // borrowed; null means no binding
    std::optional<PopulationRecord> population;
    std::optional<EntityId> population_origin;
};

enum class InspectionDiagnosticStatus : std::uint8_t {
    valid,
    invariant_violation,
    limit_exceeded,
    allocation_failure,
    capacity_failure,
};
enum class InspectionSeverity : std::uint8_t { information, warning, error };
struct InspectionDiagnosticLimits final {
    std::uint64_t max_records{1'000'000}; // sum of sparse records, including snapshot streams
    std::uint64_t max_scratch_bytes{8ULL * 1024ULL * 1024ULL}; // payload, excluding allocator overhead
};
/// One first-failure diagnostic, fixed-size output; resource failures are INCONCLUSIVE.
/// code is present only when the shared validator ran to a decision (including
/// Persistence limit_exceeded). No stale-memory comparison or automatic repair.
struct InspectionDiagnostic final {
    InspectionDiagnosticStatus status{InspectionDiagnosticStatus::valid};
    InspectionSeverity severity{InspectionSeverity::information};
    std::optional<PersistenceStatus> code{};
    [[nodiscard]] bool valid() const noexcept { return status == InspectionDiagnosticStatus::valid; }
    bool operator==(const InspectionDiagnostic&) const = default;
};

/// Read-only PRIVILEGED access for trusted Host/developer code only.
/// ObserverView remains the ONLY information boundary for players/NPC/dialogue/AI.
/// No allocation, RNG or mutation in queries. Host excludes concurrent mutation
/// throughout use, keeps world/catalog alive and discards borrowed data before
/// mutation/move/assignment/restore. Revisions do not identify replacement worlds.
class Inspector final {
  public:
    explicit Inspector(const WorldState& world) noexcept : world_{world} {}
    Inspector(WorldState&&) = delete;
    Inspector(const WorldState&&) = delete;

    // first = entity, second = direct container; same sparse canonical table.
    using ContainmentLink = std::pair<EntityId, EntityId>;
    [[nodiscard]] InspectionMetadata metadata() const noexcept;
    [[nodiscard]] std::optional<InspectedEntity> entity(EntityId id) const noexcept;
    /// Null for absent entity/binding/catalog entry; inspect entity() to distinguish.
    /// Result borrows catalog; no payload is generated or catalog retained.
    [[nodiscard]] const ResourceDefinition* definition(EntityId id,
                                                       const ContentCatalog& catalog) const noexcept;
    [[nodiscard]] InspectionEntities entities(InspectionRequest request = {}) const noexcept;
    [[nodiscard]] InspectionPage<ContainmentLink> containment(InspectionRequest request = {}) const noexcept;
    [[nodiscard]] InspectionPage<EntityRelation> relations(InspectionRequest request = {}) const noexcept;
    /// Offsets apply to the selected source/kind, preserving directed edge order.
    [[nodiscard]] InspectionPage<EntityRelation> relations_from(
        EntityId source, InspectionRequest request = {},
        std::optional<RelationKind> kind = {}) const noexcept;
    [[nodiscard]] InspectionPage<PopulationRecord> populations(InspectionRequest request = {}) const noexcept;
    [[nodiscard]] InspectionPage<PopulationMembership> memberships(InspectionRequest request = {}) const noexcept;
    [[nodiscard]] InspectionPage<KnowledgeRecord> knowledge(
        EntityId observer, InspectionRequest request = {}) const noexcept;

    /// Live structural validation without capture/serialization, bounded sparse scratch.
    [[nodiscard]] InspectionDiagnostic diagnose(InspectionDiagnosticLimits limits = {}) const noexcept;
    /// Explains a detached candidate without restoring it. Includes format/RNG-policy
    /// and persistence resource checks. Does not verify catalog availability/content.
    [[nodiscard]] static InspectionDiagnostic diagnose_snapshot(
        const CampaignSnapshot& snapshot, InspectionDiagnosticLimits limits = {},
        PersistenceLimits persistence_limits = {}) noexcept;

  private:
    const WorldState& world_;
};
} // namespace corerpg
