#include "corerpg/content_json/content_catalog_loader.hpp"

#include <filesystem>
#include <span>
#include <utility>
#include <variant>
#include <vector>

#include "corerpg/content_catalog.hpp"
#include "corerpg/content_pack_input.hpp"

namespace corerpg::content_json {

ContentCatalogPhysicalLoadFailure::ContentCatalogPhysicalLoadFailure(
    std::filesystem::path pack_root, ContentPackLoadResult load_result)
    : pack_root_(std::move(pack_root)), load_result_(std::move(load_result)) {}

const std::filesystem::path&
ContentCatalogPhysicalLoadFailure::pack_root() const noexcept {
    return pack_root_;
}

const ContentPackLoadResult&
ContentCatalogPhysicalLoadFailure::load_result() const noexcept {
    return load_result_;
}

ContentCatalogLoadResult::ContentCatalogLoadResult(ContentCatalog catalog)
    : result_(std::move(catalog)) {}

ContentCatalogLoadResult::ContentCatalogLoadResult(
    ContentCatalogPhysicalLoadFailure failure)
    : result_(std::move(failure)) {}

ContentCatalogLoadResult::ContentCatalogLoadResult(
    std::vector<ContentDiagnostic> diagnostics)
    : result_(std::move(diagnostics)) {}

bool ContentCatalogLoadResult::succeeded() const noexcept {
    return std::holds_alternative<ContentCatalog>(result_);
}

const ContentCatalog* ContentCatalogLoadResult::catalog() const noexcept {
    return std::get_if<ContentCatalog>(&result_);
}

const ContentCatalogPhysicalLoadFailure*
ContentCatalogLoadResult::physical_failure() const noexcept {
    return std::get_if<ContentCatalogPhysicalLoadFailure>(&result_);
}

const std::vector<ContentDiagnostic>*
ContentCatalogLoadResult::semantic_diagnostics() const noexcept {
    return std::get_if<std::vector<ContentDiagnostic>>(&result_);
}

ContentCatalogLoadResult load_content_catalog(
    std::span<const std::filesystem::path> pack_roots,
    ContentPackLoadOptions options) {
    std::vector<ContentPackInput> packs;
    packs.reserve(pack_roots.size());
    for (const auto& pack_root : pack_roots) {
        auto load_result = load_content_pack(pack_root, options);
        if (!load_result.succeeded()) {
            return ContentCatalogLoadResult(ContentCatalogPhysicalLoadFailure(
                pack_root, std::move(load_result)));
        }
        packs.push_back(*load_result.pack());
    }

    const auto build_result = ContentCatalog::create(std::move(packs));
    if (!build_result.succeeded()) {
        return ContentCatalogLoadResult(*build_result.diagnostics());
    }
    return ContentCatalogLoadResult(*build_result.catalog());
}

}  // namespace corerpg::content_json
