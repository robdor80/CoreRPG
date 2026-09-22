#pragma once

#include <vector>

#include "corerpg/content_diagnostic.hpp"
#include "corerpg/content_manifest.hpp"
#include "corerpg/semantic_version.hpp"

namespace corerpg {

/// Immutable policy describing the content contract supported by this Core build.
class ContentCompatibility final {
public:
    [[nodiscard]] static ContentCompatibility current();

    [[nodiscard]] const SemanticVersion& core_version() const noexcept;
    [[nodiscard]] const SemanticVersion& supported_content_contract_version() const noexcept;

    [[nodiscard]] std::vector<ContentDiagnostic>
    validate(const ContentManifest& manifest) const;

private:
    ContentCompatibility(SemanticVersion core_version,
                         SemanticVersion supported_content_contract_version);

    SemanticVersion core_version_;
    SemanticVersion supported_content_contract_version_;
};

}  // namespace corerpg
