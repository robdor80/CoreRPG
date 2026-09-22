#pragma once

#include <utility>

#include "corerpg/definition_id.hpp"
#include "corerpg/resource_kind.hpp"

namespace corerpg {

class ResourceDefinition final {
public:
    ResourceDefinition(DefinitionId id, ResourceKind kind)
        : id_(std::move(id)), kind_(std::move(kind)) {}

    [[nodiscard]] const DefinitionId& id() const noexcept {
        return id_;
    }

    [[nodiscard]] const ResourceKind& kind() const noexcept {
        return kind_;
    }

private:
    DefinitionId id_;
    ResourceKind kind_;
};

}  // namespace corerpg
