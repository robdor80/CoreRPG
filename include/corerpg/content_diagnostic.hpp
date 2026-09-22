#pragma once

#include <optional>

#include "corerpg/definition_id.hpp"
#include "corerpg/pack_id.hpp"
#include "corerpg/resource_kind.hpp"
#include "corerpg/semantic_version.hpp"
#include "corerpg/version_requirement.hpp"

namespace corerpg {

enum class ContentDiagnosticCode {
    duplicate_pack,
    definition_pack_mismatch,
    duplicate_dependency,
    self_dependency,
    dependency_not_found,
    dependency_version_mismatch,
    dependency_cycle,
    unsupported_content_contract_version,
    minimum_core_version_not_met,
    duplicate_definition,
    reference_target_not_found,
    reference_kind_mismatch,
    cross_pack_reference_not_declared,
};

class ContentDiagnostic final {
public:
    [[nodiscard]] static ContentDiagnostic duplicate_pack(PackId pack_id);

    [[nodiscard]] static ContentDiagnostic definition_pack_mismatch(
        PackId pack_id, DefinitionId definition_id);

    [[nodiscard]] static ContentDiagnostic duplicate_dependency(
        PackId pack_id, PackId dependency_pack_id);

    [[nodiscard]] static ContentDiagnostic self_dependency(PackId pack_id);

    [[nodiscard]] static ContentDiagnostic dependency_not_found(
        PackId pack_id, PackId dependency_pack_id);

    [[nodiscard]] static ContentDiagnostic dependency_version_mismatch(
        PackId pack_id, PackId dependency_pack_id, VersionRequirement requirement,
        SemanticVersion actual_version);

    [[nodiscard]] static ContentDiagnostic dependency_cycle(
        PackId pack_id, PackId dependency_pack_id);

    [[nodiscard]] static ContentDiagnostic unsupported_content_contract_version(
        PackId pack_id, SemanticVersion declared_version,
        SemanticVersion supported_version);

    [[nodiscard]] static ContentDiagnostic minimum_core_version_not_met(
        PackId pack_id, SemanticVersion minimum_core_version,
        SemanticVersion current_core_version);

    [[nodiscard]] static ContentDiagnostic duplicate_definition(DefinitionId definition_id);

    [[nodiscard]] static ContentDiagnostic reference_target_not_found(
        DefinitionId source, DefinitionId target, ResourceKind expected_kind);

    [[nodiscard]] static ContentDiagnostic reference_kind_mismatch(
        DefinitionId source, DefinitionId target, ResourceKind expected_kind,
        ResourceKind actual_kind);

    [[nodiscard]] static ContentDiagnostic cross_pack_reference_not_declared(
        DefinitionId source, DefinitionId target);

    [[nodiscard]] ContentDiagnosticCode code() const noexcept;
    [[nodiscard]] const PackId& pack_id() const noexcept;

    [[nodiscard]] const std::optional<PackId>& related_pack_id() const noexcept;
    [[nodiscard]] const std::optional<DefinitionId>& definition_id() const noexcept;
    [[nodiscard]] const std::optional<DefinitionId>& related_definition_id() const noexcept;
    [[nodiscard]] const std::optional<ResourceKind>& expected_kind() const noexcept;
    [[nodiscard]] const std::optional<ResourceKind>& actual_kind() const noexcept;
    [[nodiscard]] const std::optional<VersionRequirement>& version_requirement() const noexcept;
    [[nodiscard]] const std::optional<SemanticVersion>& actual_version() const noexcept;
    [[nodiscard]] const std::optional<SemanticVersion>& supported_version() const noexcept;

private:
    ContentDiagnostic(ContentDiagnosticCode code, PackId pack_id,
                      std::optional<PackId> related_pack_id,
                      std::optional<DefinitionId> definition_id,
                      std::optional<DefinitionId> related_definition_id,
                      std::optional<ResourceKind> expected_kind,
                      std::optional<ResourceKind> actual_kind,
                      std::optional<VersionRequirement> version_requirement,
                      std::optional<SemanticVersion> actual_version,
                      std::optional<SemanticVersion> supported_version = std::nullopt);

    ContentDiagnosticCode code_;
    PackId pack_id_;
    std::optional<PackId> related_pack_id_;
    std::optional<DefinitionId> definition_id_;
    std::optional<DefinitionId> related_definition_id_;
    std::optional<ResourceKind> expected_kind_;
    std::optional<ResourceKind> actual_kind_;
    std::optional<VersionRequirement> version_requirement_;
    std::optional<SemanticVersion> actual_version_;
    std::optional<SemanticVersion> supported_version_;
};

}  // namespace corerpg
