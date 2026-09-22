#pragma once

#include <corerpg/pack_id.hpp>
#include <corerpg/version_requirement.hpp>

#include <utility>

namespace corerpg {

/// Required Content Pack and the versions that satisfy the dependency.
class PackDependency final {
public:
    PackDependency(PackId pack_id, VersionRequirement requirement)
        : pack_id_{std::move(pack_id)}, requirement_{std::move(requirement)} {}

    [[nodiscard]] const PackId& pack_id() const noexcept { return pack_id_; }
    [[nodiscard]] const VersionRequirement& requirement() const noexcept { return requirement_; }

private:
    PackId pack_id_;
    VersionRequirement requirement_;
};

} // namespace corerpg
