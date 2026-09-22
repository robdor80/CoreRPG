#include "corerpg/resource_kind.hpp"

#include <utility>

namespace corerpg {
namespace {

[[nodiscard]] constexpr bool is_ascii_lower(char value) noexcept {
    return value >= 'a' && value <= 'z';
}

[[nodiscard]] constexpr bool is_ascii_digit(char value) noexcept {
    return value >= '0' && value <= '9';
}

[[nodiscard]] bool is_valid_resource_kind(std::string_view value) noexcept {
    if (value.empty() || !is_ascii_lower(value.front())) {
        return false;
    }

    for (const char character : value.substr(1)) {
        if (!is_ascii_lower(character) && !is_ascii_digit(character) && character != '_' &&
            character != '-') {
            return false;
        }
    }

    return true;
}

}  // namespace

ResourceKind::ResourceKind(std::string value) : value_(std::move(value)) {}

std::optional<ResourceKind> ResourceKind::parse(std::string_view value) {
    if (!is_valid_resource_kind(value)) {
        return std::nullopt;
    }

    return ResourceKind(std::string(value));
}

std::string_view ResourceKind::value() const noexcept {
    return value_;
}

}  // namespace corerpg
