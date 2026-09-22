#pragma once

#include <optional>

#include "corerpg/definition_id.hpp"
#include "corerpg/resource_kind.hpp"

namespace corerpg {

enum class ContentDiagnosticCode {
    duplicate_definition,
    reference_target_not_found,
    reference_kind_mismatch,
};

class ContentDiagnostic final {
public:
    [[nodiscard]] static ContentDiagnostic duplicate_definition(DefinitionId definition_id);

    [[nodiscard]] static ContentDiagnostic reference_target_not_found(
        DefinitionId source, DefinitionId target, ResourceKind expected_kind);

    [[nodiscard]] static ContentDiagnostic reference_kind_mismatch(
        DefinitionId source, DefinitionId target, ResourceKind expected_kind,
        ResourceKind actual_kind);

    [[nodiscard]] ContentDiagnosticCode code() const noexcept;
    [[nodiscard]] const DefinitionId& definition_id() const noexcept;

    [[nodiscard]] const std::optional<DefinitionId>& related_definition_id() const noexcept;
    [[nodiscard]] const std::optional<ResourceKind>& expected_kind() const noexcept;
    [[nodiscard]] const std::optional<ResourceKind>& actual_kind() const noexcept;

private:
    ContentDiagnostic(ContentDiagnosticCode code, DefinitionId definition_id,
                      std::optional<DefinitionId> related_definition_id,
                      std::optional<ResourceKind> expected_kind,
                      std::optional<ResourceKind> actual_kind);

    ContentDiagnosticCode code_;
    DefinitionId definition_id_;
    std::optional<DefinitionId> related_definition_id_;
    std::optional<ResourceKind> expected_kind_;
    std::optional<ResourceKind> actual_kind_;
};

}  // namespace corerpg
