#pragma once

#include <utility>
#include <vector>

#include "corerpg/definition_reference.hpp"
#include "corerpg/resource_definition.hpp"

namespace corerpg {

class ContentDefinitionInput final {
public:
    ContentDefinitionInput(ResourceDefinition definition,
                           std::vector<DefinitionReference> references = {})
        : definition_(std::move(definition)), references_(std::move(references)) {}

    [[nodiscard]] const ResourceDefinition& definition() const noexcept {
        return definition_;
    }

    [[nodiscard]] const std::vector<DefinitionReference>& references() const noexcept {
        return references_;
    }

private:
    ResourceDefinition definition_;
    std::vector<DefinitionReference> references_;
};

}  // namespace corerpg
