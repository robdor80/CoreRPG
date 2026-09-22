#include "corerpg/content_diagnostic.hpp"

#include <utility>

namespace corerpg {

ContentDiagnostic::ContentDiagnostic(
    ContentDiagnosticCode code, DefinitionId definition_id,
    std::optional<DefinitionId> related_definition_id,
    std::optional<ResourceKind> expected_kind, std::optional<ResourceKind> actual_kind)
    : code_(code), definition_id_(std::move(definition_id)),
      related_definition_id_(std::move(related_definition_id)),
      expected_kind_(std::move(expected_kind)), actual_kind_(std::move(actual_kind)) {}

ContentDiagnostic ContentDiagnostic::duplicate_definition(DefinitionId definition_id) {
    return ContentDiagnostic(ContentDiagnosticCode::duplicate_definition,
                             std::move(definition_id), std::nullopt, std::nullopt,
                             std::nullopt);
}

ContentDiagnostic ContentDiagnostic::reference_target_not_found(
    DefinitionId source, DefinitionId target, ResourceKind expected_kind) {
    return ContentDiagnostic(ContentDiagnosticCode::reference_target_not_found,
                             std::move(source), std::move(target),
                             std::move(expected_kind), std::nullopt);
}

ContentDiagnostic ContentDiagnostic::reference_kind_mismatch(
    DefinitionId source, DefinitionId target, ResourceKind expected_kind,
    ResourceKind actual_kind) {
    return ContentDiagnostic(ContentDiagnosticCode::reference_kind_mismatch,
                             std::move(source), std::move(target),
                             std::move(expected_kind), std::move(actual_kind));
}

ContentDiagnosticCode ContentDiagnostic::code() const noexcept {
    return code_;
}

const DefinitionId& ContentDiagnostic::definition_id() const noexcept {
    return definition_id_;
}

const std::optional<DefinitionId>& ContentDiagnostic::related_definition_id() const noexcept {
    return related_definition_id_;
}

const std::optional<ResourceKind>& ContentDiagnostic::expected_kind() const noexcept {
    return expected_kind_;
}

const std::optional<ResourceKind>& ContentDiagnostic::actual_kind() const noexcept {
    return actual_kind_;
}

}  // namespace corerpg
