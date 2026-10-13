#pragma once

#include <corerpg/content_catalog.hpp>

namespace corerpg_test {

inline corerpg::CatalogBuildResult structural_catalog() {
    const auto pack = *corerpg::PackId::parse("example.structural");
    const auto version = *corerpg::SemanticVersion::parse("0.1.0");
    std::vector<corerpg::ContentDefinitionInput> definitions;
    for (const auto text : {"example.structural:unit", "example.structural:collective"}) {
        definitions.emplace_back(corerpg::ResourceDefinition{
            *corerpg::DefinitionId::parse(text), *corerpg::ResourceKind::parse("structural")});
    }
    return corerpg::ContentCatalog::create({corerpg::ContentPackInput{
        corerpg::ContentManifest{pack, version, version, version}, std::move(definitions)}});
}

} // namespace corerpg_test
