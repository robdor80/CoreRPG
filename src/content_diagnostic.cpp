#include "corerpg/content_diagnostic.hpp"

#include <utility>

namespace corerpg {

ContentDiagnostic::ContentDiagnostic(
    ContentDiagnosticCode code, PackId pack_id, std::optional<PackId> related_pack_id,
    std::optional<DefinitionId> definition_id,
    std::optional<DefinitionId> related_definition_id,
    std::optional<ResourceKind> expected_kind, std::optional<ResourceKind> actual_kind,
    std::optional<VersionRequirement> version_requirement,
    std::optional<SemanticVersion> actual_version,
    std::optional<SemanticVersion> supported_version)
    : code_(code), pack_id_(std::move(pack_id)),
      related_pack_id_(std::move(related_pack_id)), definition_id_(std::move(definition_id)),
      related_definition_id_(std::move(related_definition_id)),
      expected_kind_(std::move(expected_kind)), actual_kind_(std::move(actual_kind)),
      version_requirement_(std::move(version_requirement)),
      actual_version_(std::move(actual_version)),
      supported_version_(std::move(supported_version)) {}

ContentDiagnostic ContentDiagnostic::duplicate_pack(PackId pack_id) {
    return ContentDiagnostic(ContentDiagnosticCode::duplicate_pack, std::move(pack_id),
                             std::nullopt, std::nullopt, std::nullopt, std::nullopt,
                             std::nullopt, std::nullopt, std::nullopt);
}

ContentDiagnostic ContentDiagnostic::definition_pack_mismatch(
    PackId pack_id, DefinitionId definition_id) {
    return ContentDiagnostic(ContentDiagnosticCode::definition_pack_mismatch,
                             std::move(pack_id), std::nullopt, std::move(definition_id),
                             std::nullopt, std::nullopt, std::nullopt, std::nullopt,
                             std::nullopt);
}

ContentDiagnostic ContentDiagnostic::duplicate_dependency(
    PackId pack_id, PackId dependency_pack_id) {
    return ContentDiagnostic(ContentDiagnosticCode::duplicate_dependency,
                             std::move(pack_id), std::move(dependency_pack_id),
                             std::nullopt, std::nullopt, std::nullopt, std::nullopt,
                             std::nullopt, std::nullopt);
}

ContentDiagnostic ContentDiagnostic::self_dependency(PackId pack_id) {
    return ContentDiagnostic(ContentDiagnosticCode::self_dependency, std::move(pack_id),
                             std::nullopt, std::nullopt, std::nullopt, std::nullopt,
                             std::nullopt, std::nullopt, std::nullopt);
}

ContentDiagnostic ContentDiagnostic::dependency_not_found(
    PackId pack_id, PackId dependency_pack_id) {
    return ContentDiagnostic(ContentDiagnosticCode::dependency_not_found,
                             std::move(pack_id), std::move(dependency_pack_id),
                             std::nullopt, std::nullopt, std::nullopt, std::nullopt,
                             std::nullopt, std::nullopt);
}

ContentDiagnostic ContentDiagnostic::dependency_version_mismatch(
    PackId pack_id, PackId dependency_pack_id, VersionRequirement requirement,
    SemanticVersion actual_version) {
    return ContentDiagnostic(ContentDiagnosticCode::dependency_version_mismatch,
                             std::move(pack_id), std::move(dependency_pack_id),
                             std::nullopt, std::nullopt, std::nullopt, std::nullopt,
                             std::move(requirement), std::move(actual_version));
}

ContentDiagnostic ContentDiagnostic::dependency_cycle(
    PackId pack_id, PackId dependency_pack_id) {
    return ContentDiagnostic(ContentDiagnosticCode::dependency_cycle, std::move(pack_id),
                             std::move(dependency_pack_id), std::nullopt, std::nullopt,
                             std::nullopt, std::nullopt, std::nullopt, std::nullopt);
}

ContentDiagnostic ContentDiagnostic::unsupported_content_contract_version(
    PackId pack_id, SemanticVersion declared_version,
    SemanticVersion supported_version) {
    return ContentDiagnostic(
        ContentDiagnosticCode::unsupported_content_contract_version,
        std::move(pack_id), std::nullopt, std::nullopt, std::nullopt,
        std::nullopt, std::nullopt, std::nullopt, std::move(declared_version),
        std::move(supported_version));
}

ContentDiagnostic ContentDiagnostic::minimum_core_version_not_met(
    PackId pack_id, SemanticVersion minimum_core_version,
    SemanticVersion current_core_version) {
    auto requirement =
        VersionRequirement::create(std::move(minimum_core_version)).value();
    return ContentDiagnostic(
        ContentDiagnosticCode::minimum_core_version_not_met, std::move(pack_id),
        std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt,
        std::move(requirement), std::move(current_core_version));
}

ContentDiagnostic ContentDiagnostic::duplicate_definition(DefinitionId definition_id) {
    auto pack_id = definition_id.pack_id();
    return ContentDiagnostic(ContentDiagnosticCode::duplicate_definition,
                             std::move(pack_id), std::nullopt,
                             std::move(definition_id), std::nullopt, std::nullopt,
                             std::nullopt, std::nullopt, std::nullopt);
}

ContentDiagnostic ContentDiagnostic::reference_target_not_found(
    DefinitionId source, DefinitionId target, ResourceKind expected_kind) {
    auto pack_id = source.pack_id();
    return ContentDiagnostic(ContentDiagnosticCode::reference_target_not_found,
                             std::move(pack_id), std::nullopt, std::move(source),
                             std::move(target), std::move(expected_kind), std::nullopt,
                             std::nullopt, std::nullopt);
}

ContentDiagnostic ContentDiagnostic::reference_kind_mismatch(
    DefinitionId source, DefinitionId target, ResourceKind expected_kind,
    ResourceKind actual_kind) {
    auto pack_id = source.pack_id();
    return ContentDiagnostic(ContentDiagnosticCode::reference_kind_mismatch,
                             std::move(pack_id), std::nullopt, std::move(source),
                             std::move(target), std::move(expected_kind),
                             std::move(actual_kind), std::nullopt, std::nullopt);
}

ContentDiagnostic ContentDiagnostic::cross_pack_reference_not_declared(
    DefinitionId source, DefinitionId target) {
    auto pack_id = source.pack_id();
    auto related_pack_id = target.pack_id();
    return ContentDiagnostic(ContentDiagnosticCode::cross_pack_reference_not_declared,
                             std::move(pack_id), std::move(related_pack_id),
                             std::move(source), std::move(target), std::nullopt,
                             std::nullopt, std::nullopt, std::nullopt);
}

ContentDiagnosticCode ContentDiagnostic::code() const noexcept {
    return code_;
}

const PackId& ContentDiagnostic::pack_id() const noexcept {
    return pack_id_;
}

const std::optional<PackId>& ContentDiagnostic::related_pack_id() const noexcept {
    return related_pack_id_;
}

const std::optional<DefinitionId>& ContentDiagnostic::definition_id() const noexcept {
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

const std::optional<VersionRequirement>& ContentDiagnostic::version_requirement() const noexcept {
    return version_requirement_;
}

const std::optional<SemanticVersion>& ContentDiagnostic::actual_version() const noexcept {
    return actual_version_;
}

const std::optional<SemanticVersion>& ContentDiagnostic::supported_version() const noexcept {
    return supported_version_;
}

}  // namespace corerpg
