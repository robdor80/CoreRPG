#include <corerpg/context_builder.hpp>

#include <type_traits>

namespace corerpg {
namespace {
bool supported_kind(FactKind kind) noexcept {
    switch (kind) {
    case FactKind::existence:
    case FactKind::definition:
    case FactKind::containment:
    case FactKind::relation:
    case FactKind::population:
    case FactKind::population_origin: return true;
    }
    return false;
}
static_assert(std::is_nothrow_copy_assignable_v<VisibleFact>);
} // namespace

ContextResult ContextBuilder::build(const ObserverView& view, const ContextRequest& request,
                                     std::span<VisibleFact> output, ContextLimits limits) noexcept {
    ContextResult result;
    if (request.contract_version != context_contract_version) {
        result.status = ContextStatus::unsupported_version;
        return result;
    }
    if (request.kind && !supported_kind(*request.kind)) {
        result.status = ContextStatus::invalid_request;
        return result;
    }
    if (limits.max_examined > ContextLimits::examination_ceiling ||
        limits.max_selected > ContextLimits::selection_ceiling ||
        limits.max_content_bytes > ContextLimits::content_ceiling) {
        result.status = ContextStatus::invalid_limits;
        return result;
    }
    for (const auto& fact : view.facts()) {
        if (result.examined == limits.max_examined) {
            result.status = ContextStatus::examination_limit;
            return result;
        }
        ++result.examined;
        if ((request.subject && fact.key.subject != *request.subject) ||
            (request.kind && fact.key.kind != *request.kind)) continue;
        if (result.emitted == limits.max_selected) {
            result.status = ContextStatus::selection_limit;
            return result;
        }
        if (result.emitted == output.size()) {
            result.status = ContextStatus::output_capacity;
            return result;
        }
        const auto remaining = limits.max_content_bytes - result.content_bytes;
        const auto text_size = fact.definition_id().size();
        if (remaining < context_fact_fixed_bytes || text_size > remaining - context_fact_fixed_bytes) {
            result.status = ContextStatus::content_limit;
            return result;
        }
        output[result.emitted] = fact;
        ++result.emitted;
        result.content_bytes += context_fact_fixed_bytes + text_size;
    }
    return result;
}
} // namespace corerpg
