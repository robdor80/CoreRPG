#pragma once

#include <corerpg/pack_dependency.hpp>
#include <corerpg/pack_id.hpp>
#include <corerpg/semantic_version.hpp>

#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace corerpg {

/// Logical, in-memory manifest for one published Content Pack version.
class ContentManifest final {
public:
    ContentManifest(PackId pack_id, SemanticVersion pack_version,
                    SemanticVersion content_contract_version,
                    SemanticVersion minimum_core_version,
                    std::vector<PackDependency> dependencies = {},
                    std::string display_name = {}, std::string description = {})
        : pack_id_{std::move(pack_id)}, pack_version_{std::move(pack_version)},
          content_contract_version_{std::move(content_contract_version)},
          minimum_core_version_{std::move(minimum_core_version)},
          dependencies_{std::move(dependencies)}, display_name_{std::move(display_name)},
          description_{std::move(description)} {}

    [[nodiscard]] const PackId& pack_id() const noexcept { return pack_id_; }
    [[nodiscard]] const SemanticVersion& pack_version() const noexcept { return pack_version_; }

    [[nodiscard]] const SemanticVersion& content_contract_version() const noexcept {
        return content_contract_version_;
    }

    [[nodiscard]] const SemanticVersion& minimum_core_version() const noexcept {
        return minimum_core_version_;
    }

    [[nodiscard]] const std::vector<PackDependency>& dependencies() const noexcept {
        return dependencies_;
    }

    [[nodiscard]] std::string_view display_name() const noexcept { return display_name_; }
    [[nodiscard]] std::string_view description() const noexcept { return description_; }

private:
    // Normative fields.
    PackId pack_id_;
    SemanticVersion pack_version_;
    SemanticVersion content_contract_version_;
    SemanticVersion minimum_core_version_;
    std::vector<PackDependency> dependencies_;

    // Informative metadata.
    std::string display_name_;
    std::string description_;
};

} // namespace corerpg
