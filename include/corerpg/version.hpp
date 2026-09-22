#pragma once

#include <string_view>

namespace corerpg {

/// Returns the Semantic Versioning string embedded in this CoreRPG build.
[[nodiscard]] std::string_view library_version() noexcept;

} // namespace corerpg
