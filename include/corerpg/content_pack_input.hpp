#pragma once

#include <utility>
#include <vector>

#include "corerpg/content_definition_input.hpp"
#include "corerpg/content_manifest.hpp"

namespace corerpg {

class ContentPackInput final {
public:
    ContentPackInput(ContentManifest manifest,
                     std::vector<ContentDefinitionInput> definitions = {})
        : manifest_(std::move(manifest)), definitions_(std::move(definitions)) {}

    [[nodiscard]] const ContentManifest& manifest() const noexcept {
        return manifest_;
    }

    [[nodiscard]] const std::vector<ContentDefinitionInput>& definitions() const noexcept {
        return definitions_;
    }

private:
    ContentManifest manifest_;
    std::vector<ContentDefinitionInput> definitions_;
};

}  // namespace corerpg
