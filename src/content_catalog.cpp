#include "corerpg/content_catalog.hpp"

#include <algorithm>
#include <utility>

namespace corerpg {

ContentCatalog::ContentCatalog(std::vector<ResourceDefinition> definitions)
    : definitions_(std::move(definitions)) {}

std::optional<ContentCatalog> ContentCatalog::create(
    std::vector<ResourceDefinition> definitions) {
    std::ranges::sort(definitions, {}, &ResourceDefinition::id);

    const auto duplicate = std::ranges::adjacent_find(
        definitions,
        [](const ResourceDefinition& left, const ResourceDefinition& right) {
            return left.id() == right.id();
        });
    if (duplicate != definitions.end()) {
        return std::nullopt;
    }

    return ContentCatalog(std::move(definitions));
}

std::size_t ContentCatalog::size() const noexcept {
    return definitions_.size();
}

bool ContentCatalog::empty() const noexcept {
    return definitions_.empty();
}

bool ContentCatalog::contains(const DefinitionId& id) const noexcept {
    return find(id) != nullptr;
}

const ResourceDefinition* ContentCatalog::find(const DefinitionId& id) const noexcept {
    const auto definition = std::ranges::lower_bound(definitions_, id, {}, &ResourceDefinition::id);
    if (definition == definitions_.end() || definition->id() != id) {
        return nullptr;
    }

    return &*definition;
}

const std::vector<ResourceDefinition>& ContentCatalog::definitions() const noexcept {
    return definitions_;
}

}  // namespace corerpg
