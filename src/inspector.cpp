#include <corerpg/inspector.hpp>
#include <corerpg/content_catalog.hpp>

#include "world_validation_internal.hpp"

#include <algorithm>
#include <new>
#include <stdexcept>

namespace corerpg {
namespace {
InspectionProgress progress_for(std::uint64_t revision, InspectionRequest request) noexcept {
    if (request.expected_revision && *request.expected_revision != revision)
        return {InspectionStatus::stale_revision, revision, {}};
    if (request.limit == 0 || request.limit > inspector_max_page_size)
        return {InspectionStatus::invalid_limit, revision, {}};
    return {InspectionStatus::success, revision, {}};
}
template <class T>
InspectionPage<T> page(std::span<const T> records, std::uint64_t revision,
                       InspectionRequest request, bool exists = true) noexcept {
    auto progress = progress_for(revision, request);
    if (progress.status != InspectionStatus::success) return {progress, {}};
    if (!exists) {
        progress.status = InspectionStatus::entity_not_found;
        return {progress, {}};
    }
    if (request.offset >= records.size()) return {progress, {}};
    const auto offset = static_cast<std::size_t>(request.offset);
    const auto count = std::min(records.size() - offset, static_cast<std::size_t>(request.limit));
    if (count < records.size() - offset) progress.next_offset = request.offset + count;
    return {progress, records.subspan(offset, count)};
}
} // namespace

InspectionMetadata Inspector::metadata() const noexcept {
    return {world_.revision_, world_.last_entity_id_, world_.containment_.size(),
            world_.definitions_.size(), world_.relations_.size(), world_.populations_.size(),
            world_.population_memberships_.size(), world_.knowledge_.size()};
}
std::optional<InspectedEntity> Inspector::entity(EntityId id) const noexcept {
    if (!world_.contains(id)) return std::nullopt;
    return InspectedEntity{id, world_.container_of(id), world_.definition_of(id),
                           world_.population_of(id), world_.population_origin(id)};
}
const ResourceDefinition* Inspector::definition(EntityId id, const ContentCatalog& catalog) const noexcept {
    const auto value = entity(id);
    return value && value->definition ? catalog.find(*value->definition) : nullptr;
}
InspectionEntities Inspector::entities(InspectionRequest request) const noexcept {
    auto progress = progress_for(world_.revision(), request);
    if (progress.status != InspectionStatus::success || request.offset >= world_.entity_count())
        return {progress, {}, 0};
    const auto count = static_cast<std::uint32_t>(std::min(
        world_.entity_count() - request.offset, static_cast<std::uint64_t>(request.limit)));
    if (count < world_.entity_count() - request.offset) progress.next_offset = request.offset + count;
    return {progress, EntityId{request.offset + 1}, count};
}
InspectionPage<Inspector::ContainmentLink> Inspector::containment(InspectionRequest request) const noexcept {
    return page<ContainmentLink>(world_.containment_, world_.revision(), request);
}
InspectionPage<EntityRelation> Inspector::relations(InspectionRequest request) const noexcept {
    return page<EntityRelation>(world_.relations(), world_.revision(), request);
}
InspectionPage<EntityRelation> Inspector::relations_from(EntityId source, InspectionRequest request,
                                                        std::optional<RelationKind> kind) const noexcept {
    auto records = world_.relations_from(source);
    if (kind) {
        const auto begin = std::lower_bound(records.begin(), records.end(), *kind,
            [](const auto& edge, RelationKind value) { return edge.kind < value; });
        const auto end = std::upper_bound(begin, records.end(), *kind,
            [](RelationKind value, const auto& edge) { return value < edge.kind; });
        // Do not subtract possibly-null pointers for an empty span.
        if (begin == end) records = {};
        else records = std::span<const EntityRelation>{begin, end};
    }
    return page<EntityRelation>(records, world_.revision(), request, world_.contains(source));
}
InspectionPage<PopulationRecord> Inspector::populations(InspectionRequest request) const noexcept {
    return page<PopulationRecord>(world_.populations(), world_.revision(), request);
}
InspectionPage<PopulationMembership> Inspector::memberships(InspectionRequest request) const noexcept {
    return page<PopulationMembership>(world_.population_memberships(), world_.revision(), request);
}
InspectionPage<KnowledgeRecord> Inspector::knowledge(EntityId observer, InspectionRequest request) const noexcept {
    return page<KnowledgeRecord>(world_.knowledge_of(observer), world_.revision(), request,
                                 world_.contains(observer));
}

namespace {
template <class State>
bool diagnostic_budget(const State& state, InspectionDiagnosticLimits limits) noexcept {
    auto remaining = limits.max_records;
    for (const auto count : {state.containment.size(), state.definitions.size(), state.relations.size(),
                            state.populations.size(), state.population_memberships.size(),
                            state.knowledge.size(), state.streams.size()}) {
        if (count > remaining) return false;
        remaining -= count;
    }
    // Division/subtraction before multiplication, also on 32-bit hosts.
    if (state.populations.size() > limits.max_scratch_bytes / sizeof(std::uint64_t)) return false;
    return state.containment.size() <= limits.max_scratch_bytes -
        state.populations.size() * sizeof(std::uint64_t);
}
InspectionDiagnostic diagnostic_code(PersistenceStatus status) noexcept {
    if (status == PersistenceStatus::success)
        return {InspectionDiagnosticStatus::valid, InspectionSeverity::information, status};
    if (status == PersistenceStatus::limit_exceeded)
        return {InspectionDiagnosticStatus::limit_exceeded, InspectionSeverity::warning, status};
    return {InspectionDiagnosticStatus::invariant_violation, InspectionSeverity::error, status};
}
template <class Validate>
InspectionDiagnostic run_diagnostic(bool budget, Validate validate) noexcept {
    if (!budget) return {InspectionDiagnosticStatus::limit_exceeded, InspectionSeverity::warning, {}};
    try {
        return diagnostic_code(validate());
    } catch (const std::bad_alloc&) {
        return {InspectionDiagnosticStatus::allocation_failure, InspectionSeverity::warning, {}};
    } catch (const std::length_error&) {
        return {InspectionDiagnosticStatus::capacity_failure, InspectionSeverity::warning, {}};
    }
}
} // namespace
InspectionDiagnostic Inspector::diagnose(InspectionDiagnosticLimits limits) const noexcept {
    const detail::WorldValidationView view{world_.last_entity_id_, world_.revision_, world_.containment_,
        world_.definitions_, world_.relations_, world_.populations_, world_.population_memberships_,
        world_.knowledge_};
    return run_diagnostic(diagnostic_budget(view, limits), [&] {
        return detail::validate_world_structure(view);
    });
}
InspectionDiagnostic Inspector::diagnose_snapshot(const CampaignSnapshot& snapshot,
    InspectionDiagnosticLimits limits, PersistenceLimits persistence_limits) noexcept {
    return run_diagnostic(diagnostic_budget(snapshot, limits), [&] {
        return Persistence::validate(snapshot, persistence_limits);
    });
}
} // namespace corerpg
