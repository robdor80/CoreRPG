#pragma once

#include <utility>

#include "corerpg/definition_id.hpp"
#include "corerpg/resource_kind.hpp"

namespace corerpg {

class DefinitionReference final {
public:
    DefinitionReference(DefinitionId target, ResourceKind expected_kind)
        : target_(std::move(target)), expected_kind_(std::move(expected_kind)) {}

    [[nodiscard]] const DefinitionId& target() const noexcept {
        return target_;
    }

    [[nodiscard]] const ResourceKind& expected_kind() const noexcept {
        return expected_kind_;
    }

private:
    DefinitionId target_;
    ResourceKind expected_kind_;
};

}  // namespace corerpg
