#pragma once

#include <compare>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace corerpg {

/// Stable MAJOR.MINOR.PATCH subset of Semantic Versioning.
class SemanticVersion final {
public:
    using component_type = std::uint32_t;

    [[nodiscard]] static std::optional<SemanticVersion> parse(std::string_view text);

    [[nodiscard]] constexpr component_type major() const noexcept { return major_; }
    [[nodiscard]] constexpr component_type minor() const noexcept { return minor_; }
    [[nodiscard]] constexpr component_type patch() const noexcept { return patch_; }
    [[nodiscard]] std::string_view value() const noexcept { return value_; }

    auto operator<=>(const SemanticVersion&) const noexcept = default;

private:
    SemanticVersion(component_type major, component_type minor, component_type patch,
                    std::string value)
        : major_{major}, minor_{minor}, patch_{patch}, value_{std::move(value)} {}

    component_type major_;
    component_type minor_;
    component_type patch_;
    std::string value_;
};

} // namespace corerpg
