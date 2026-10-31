#pragma once

#include <corerpg/campaign_persistence.hpp>
#include <array>
#include <filesystem>
#include <functional>
#include <map>

namespace corerpg {

using HistoryHash = std::array<std::uint8_t,32>;
using HistoryCampaignId = std::array<std::uint8_t,16>;
enum class HistoryStatus : std::uint8_t {
    success, invalid_request, unsupported_version, integrity_error, limit_exceeded,
    io_error, conflict, invalid_projection, dependency_pinned, injected_failure
};
enum class HistoryLocation : std::uint8_t { nonexistent, resident, archived };
enum class HistoryFault : std::uint8_t {
    none, before_segment, after_segment, after_checkpoint, before_manifest,
    before_publish, after_publish, read
};
struct HistoryLimits final {
    PersistenceLimits records{};
    std::uint64_t max_manifest_bytes{16*1024*1024};
    std::uint64_t max_segments{100'000};
    std::uint64_t max_selected{65'536};
    std::uint64_t max_page{256};
};
/// Inclusive ID ranges, independent of residency and never reusable.
struct HistoryIdRange final {
    std::uint64_t first{},last{};
    bool operator==(const HistoryIdRange&) const = default;
};
struct HistorySelection final {
    EntityId entity;
    std::uint64_t begin{},end{}; // Declared closed fact, [begin,end), end <= frontier.
    bool operator==(const HistorySelection&) const = default;
};
/// Generic predicate on self-relations whose high 16 bits identify a scalar field.
/// No field number or definition has an engine-specific meaning.
enum class HistoryPredicateOp : std::uint8_t { present, absent, equal, less_field };
struct HistoryPredicate final {
    std::uint16_t field{};
    HistoryPredicateOp op{};
    std::uint64_t operand{};
    bool operator==(const HistoryPredicate&) const = default;
};
struct HistoryCountRule final {
    std::uint64_t key{};
    std::string definition;
    RelationKind group_relation;
    // Disjunction of conjunctions. Empty alternatives match every record of definition.
    std::vector<std::vector<HistoryPredicate>> alternatives;
    bool operator==(const HistoryCountRule&) const = default;
};
struct HistoryDescriptor final {
    std::uint64_t sequence{},bytes{},begin{},end{},issued{},revision{};
    HistoryHash hash{},previous{};
    std::vector<HistoryIdRange> ids;
    bool operator==(const HistoryDescriptor&) const = default;
};
struct HistoryFact final {
    HistorySelection identity;
    std::string definition;
    std::optional<EntityId> container;
    std::vector<EntityRelation> relations;
    std::vector<KnowledgeRecord> knowledge;
};
struct HistoryPage final {
    HistoryStatus status{HistoryStatus::success};
    std::vector<HistoryFact> facts;
    EntityId next; // Exclusive ID cursor, valid only with the same generation.
    bool more{};
};

/// Opt-in Core-owned manifest. Existing WorldState ABI, ID semantics and SAVE readers
/// are unchanged. A WorldState used with this API is explicitly an active projection;
/// contains/entity_count retain their issued-ID meaning. Use locate/resident_count here.
/// Privileged callers must serialize writers and stop using a projection on conflict.
class HistoryArchive final {
public:
    [[nodiscard]] std::uint64_t generation() const noexcept { return generation_; }
    [[nodiscard]] std::uint64_t high_water_mark() const noexcept { return issued_; }
    [[nodiscard]] std::uint64_t frontier() const noexcept { return frontier_; }
    [[nodiscard]] HistoryCampaignId campaign_id() const noexcept { return campaign_; }
    [[nodiscard]] const std::vector<HistoryDescriptor>& segments() const noexcept { return segments_; }
    [[nodiscard]] const std::vector<HistoryCountRule>& count_rules() const noexcept { return rules_; }
    [[nodiscard]] const std::filesystem::path& directory() const noexcept { return directory_; }
    [[nodiscard]] HistoryLocation locate(EntityId, std::uint64_t issued) const noexcept;
    [[nodiscard]] std::uint64_t archived_count() const noexcept;
    [[nodiscard]] std::uint64_t resident_count(const WorldState& world) const noexcept;
    [[nodiscard]] std::uint64_t count(std::uint64_t rule, EntityId group) const noexcept;
private:
    friend class HistoryPersistence;
    std::filesystem::path directory_;
    HistoryCampaignId campaign_{};
    std::uint64_t generation_{},issued_{},revision_{},frontier_{},now_{};
    HistoryHash root_{},checkpoint_{};
    std::uint64_t checkpoint_bytes_{};
    std::vector<HistoryDescriptor> segments_;
    std::vector<HistoryCountRule> rules_;
    std::map<std::pair<std::uint64_t,std::uint64_t>,std::uint64_t> counts_;
};

/// Storage v1: CRPGHST1 manifests, CRPGSEG1 immutable original facts, SHA-256,
/// CRPGTIME active checkpoints. Publication uses an atomic durable CURRENT pointer;
/// no fallback to a stale generation when a confirmed root is corrupt.
class HistoryPersistence final {
public:
    [[nodiscard]] static HistoryHash hash(std::span<const std::uint8_t>);
    /// Sparse traversal of the active definition table; does not scan issued IDs.
    [[nodiscard]] static std::vector<EntityId> bound_entities(const WorldState&);
    /// Explicit non-destructive migration: caller loads the legacy source through
    /// unchanged readers, then creates a NEW directory. Never rewrites the source.
    [[nodiscard]] static HistoryStatus migrate(const std::filesystem::path&, HistoryCampaignId,
        const WorldState&,const RandomStreams&,const Timeline&,HistoryArchive&,
        std::vector<HistoryCountRule> = {},HistoryLimits = {},HistoryFault = {});
    /// Verifies projection, historical ownership and revision/high-water monotonicity.
    [[nodiscard]] static HistoryStatus validate_active(const HistoryArchive&,
        const CampaignArchiveSnapshot&,HistoryLimits = {});
    /// Prepares copies and verifies all new objects before publishing. On pre-publication
    /// failure, world/RNG/time/archive remain unchanged. after_publish simulates process
    /// loss: returns injected_failure with old memory; recover before any further write.
    [[nodiscard]] static HistoryStatus consolidate(HistoryArchive&,WorldState&,RandomStreams&,
        Timeline&,std::span<const HistorySelection>,std::uint64_t frontier,
        const ContentCatalog* = nullptr,HistoryLimits = {},HistoryFault = {},
        const std::function<bool(const CampaignArchiveSnapshot&)>& validate_projection = {});
    [[nodiscard]] static HistoryStatus publish(HistoryArchive&,const WorldState&,
        const RandomStreams&,const Timeline&,HistoryLimits = {},HistoryFault = {});
    /// Complete validation streams segments one at a time and verifies reductions.
    /// All destinations are unchanged on any error, including a missing/corrupt segment.
    [[nodiscard]] static HistoryStatus recover(const std::filesystem::path&,HistoryArchive&,
        WorldState&,RandomStreams&,Timeline&,const ContentCatalog* = nullptr,
        HistoryLimits = {},HistoryFault = {});
    [[nodiscard]] static HistoryPage query(const HistoryArchive&,EntityId after,std::uint64_t limit,
        HistoryLimits = {},HistoryFault = {});
private:
    struct Segment;
    static HistoryStatus commit(HistoryArchive&,const CampaignArchiveSnapshot&,
        const Segment*,HistoryLimits,HistoryFault);
    static HistoryStatus read_segment(const HistoryArchive&,const HistoryDescriptor&,Segment&,HistoryLimits,HistoryFault);
    static std::vector<std::uint8_t> encode_manifest(const HistoryArchive&);
    static HistoryStatus decode_manifest(std::span<const std::uint8_t>,HistoryArchive&,HistoryLimits);
    static void reduce(HistoryArchive&,const Segment&);
};
} // namespace corerpg
