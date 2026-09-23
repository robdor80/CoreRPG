#pragma once

#include <filesystem>
#include <span>
#include <variant>
#include <vector>

#include "corerpg/content_catalog.hpp"
#include "corerpg/content_diagnostic.hpp"
#include "corerpg/content_json/content_pack_loader.hpp"

namespace corerpg::content_json {

class ContentCatalogLoadResult;

class ContentCatalogPhysicalLoadFailure final {
public:
    [[nodiscard]] const std::filesystem::path& pack_root() const noexcept;
    [[nodiscard]] const ContentPackLoadResult& load_result() const noexcept;

private:
    friend class ContentCatalogLoadResult;
    friend ContentCatalogLoadResult load_content_catalog(
        std::span<const std::filesystem::path> pack_roots,
        ContentPackLoadOptions options);

    ContentCatalogPhysicalLoadFailure(std::filesystem::path pack_root,
                                      ContentPackLoadResult load_result);

    std::filesystem::path pack_root_;
    ContentPackLoadResult load_result_;
};

class ContentCatalogLoadResult final {
public:
    [[nodiscard]] bool succeeded() const noexcept;
    [[nodiscard]] const ContentCatalog* catalog() const noexcept;
    [[nodiscard]] const ContentCatalogPhysicalLoadFailure* physical_failure() const noexcept;
    [[nodiscard]] const std::vector<ContentDiagnostic>* semantic_diagnostics() const noexcept;

private:
    friend ContentCatalogLoadResult load_content_catalog(
        std::span<const std::filesystem::path> pack_roots,
        ContentPackLoadOptions options);

    explicit ContentCatalogLoadResult(ContentCatalog catalog);
    explicit ContentCatalogLoadResult(ContentCatalogPhysicalLoadFailure failure);
    explicit ContentCatalogLoadResult(std::vector<ContentDiagnostic> diagnostics);

    std::variant<ContentCatalog, ContentCatalogPhysicalLoadFailure,
                 std::vector<ContentDiagnostic>>
        result_;
};

[[nodiscard]] ContentCatalogLoadResult load_content_catalog(
    std::span<const std::filesystem::path> pack_roots,
    ContentPackLoadOptions options = {});

}  // namespace corerpg::content_json
