#pragma once

#include <cstddef>
#include <variant>
#include <vector>
#include <string_view>

#include "corerpg/content_diagnostic.hpp"
#include "corerpg/content_pack_input.hpp"
#include "corerpg/resource_definition.hpp"

namespace corerpg {

class CatalogBuildResult;

class ContentCatalog final {
public:
    [[nodiscard]] static CatalogBuildResult create(
        std::vector<ContentPackInput> packs);

    [[nodiscard]] std::size_t size() const noexcept;
    [[nodiscard]] bool empty() const noexcept;
    [[nodiscard]] bool contains(const DefinitionId& id) const noexcept;
    [[nodiscard]] const ResourceDefinition* find(const DefinitionId& id) const noexcept;
    /// Non-allocating resolution of a persisted canonical identifier.
    [[nodiscard]] const ResourceDefinition* find_by_value(std::string_view id) const noexcept;
    [[nodiscard]] const std::vector<ResourceDefinition>& definitions() const noexcept;

private:
    explicit ContentCatalog(std::vector<ResourceDefinition> definitions);

    std::vector<ResourceDefinition> definitions_;
};

class CatalogBuildResult final {
public:
    [[nodiscard]] bool succeeded() const noexcept;
    [[nodiscard]] const ContentCatalog* catalog() const noexcept;
    [[nodiscard]] const std::vector<ContentDiagnostic>* diagnostics() const noexcept;

private:
    friend class ContentCatalog;

    explicit CatalogBuildResult(ContentCatalog catalog);
    explicit CatalogBuildResult(std::vector<ContentDiagnostic> diagnostics);

    std::variant<ContentCatalog, std::vector<ContentDiagnostic>> result_;
};

}  // namespace corerpg
