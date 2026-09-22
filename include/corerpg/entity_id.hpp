#pragma once

#include <compare>
#include <cstdint>

namespace corerpg {

/// Identifies a live entity in a WorldState, not a Content Pack definition.
class EntityId final {
public:
    using value_type = std::uint64_t;

    constexpr EntityId() noexcept = default;
    constexpr explicit EntityId(value_type value) noexcept : value_{value} {}

    [[nodiscard]] constexpr bool is_valid() const noexcept { return value_ != 0; }
    [[nodiscard]] constexpr value_type value() const noexcept { return value_; }

    constexpr auto operator<=>(const EntityId&) const noexcept = default;

private:
    value_type value_{0};
};

} // namespace corerpg
