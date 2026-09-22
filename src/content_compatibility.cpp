#include "corerpg/content_compatibility.hpp"

#include <string_view>
#include <utility>
#include <vector>

#include "corerpg/version.hpp"

namespace corerpg {
namespace {

constexpr std::string_view supported_content_contract_version_text{"0.1.0"};

}  // namespace

ContentCompatibility::ContentCompatibility(
    SemanticVersion core_version, SemanticVersion supported_content_contract_version)
    : core_version_{std::move(core_version)},
      supported_content_contract_version_{
          std::move(supported_content_contract_version)} {}

ContentCompatibility ContentCompatibility::current() {
    return ContentCompatibility(
        current_core_version(),
        SemanticVersion::parse(supported_content_contract_version_text).value());
}

const SemanticVersion& ContentCompatibility::core_version() const noexcept {
    return core_version_;
}

const SemanticVersion&
ContentCompatibility::supported_content_contract_version() const noexcept {
    return supported_content_contract_version_;
}

std::vector<ContentDiagnostic>
ContentCompatibility::validate(const ContentManifest& manifest) const {
    std::vector<ContentDiagnostic> diagnostics;

    if (manifest.content_contract_version() != supported_content_contract_version_) {
        diagnostics.push_back(ContentDiagnostic::unsupported_content_contract_version(
            manifest.pack_id(), manifest.content_contract_version(),
            supported_content_contract_version_));
    }

    if (manifest.minimum_core_version() > core_version_) {
        diagnostics.push_back(ContentDiagnostic::minimum_core_version_not_met(
            manifest.pack_id(), manifest.minimum_core_version(), core_version_));
    }

    return diagnostics;
}

}  // namespace corerpg
