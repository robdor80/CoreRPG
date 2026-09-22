#pragma once

#include <corerpg/semantic_version.hpp>

#include <string_view>

namespace corerpg {

/// Returns the Semantic Versioning string embedded in this CoreRPG build.
[[nodiscard]] std::string_view library_version() noexcept;

/// Returns the semantic form of the version embedded in this CoreRPG build.
[[nodiscard]] SemanticVersion current_core_version();

} // namespace corerpg
