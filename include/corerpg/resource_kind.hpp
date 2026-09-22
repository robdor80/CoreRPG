#pragma once

#include <compare>
#include <optional>
#include <string>
#include <string_view>

namespace corerpg {

class ResourceKind final {
public:
    [[nodiscard]] static std::optional<ResourceKind> parse(std::string_view value);

    [[nodiscard]] std::string_view value() const noexcept;

    auto operator<=>(const ResourceKind&) const noexcept = default;

private:
    explicit ResourceKind(std::string value);

    std::string value_;
};

}  // namespace corerpg
