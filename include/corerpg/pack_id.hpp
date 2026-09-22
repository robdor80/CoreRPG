#pragma once

#include <compare>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace corerpg {

/// Stable identity of a Content Pack lineage, without a version component.
class PackId final {
public:
    [[nodiscard]] static std::optional<PackId> parse(std::string_view text);

    [[nodiscard]] std::string_view value() const noexcept { return value_; }

    auto operator<=>(const PackId&) const noexcept = default;

private:
    explicit PackId(std::string value) : value_{std::move(value)} {}

    std::string value_;
};

} // namespace corerpg
