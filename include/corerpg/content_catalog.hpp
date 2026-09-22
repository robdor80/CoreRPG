#pragma once

#include <cstddef>
#include <optional>
#include <vector>

#include "corerpg/resource_definition.hpp"

namespace corerpg {

class ContentCatalog final {
public:
    [[nodiscard]] static std::optional<ContentCatalog> create(
        std::vector<ResourceDefinition> definitions);

    [[nodiscard]] std::size_t size() const noexcept;
    [[nodiscard]] bool empty() const noexcept;
    [[nodiscard]] bool contains(const DefinitionId& id) const noexcept;
    [[nodiscard]] const ResourceDefinition* find(const DefinitionId& id) const noexcept;
    [[nodiscard]] const std::vector<ResourceDefinition>& definitions() const noexcept;

private:
    explicit ContentCatalog(std::vector<ResourceDefinition> definitions);

    std::vector<ResourceDefinition> definitions_;
};

}  // namespace corerpg
