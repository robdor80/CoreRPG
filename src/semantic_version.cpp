#include <corerpg/semantic_version.hpp>

#include <limits>
#include <string>
#include <string_view>

namespace corerpg {
namespace {

[[nodiscard]] std::optional<SemanticVersion::component_type>
parse_component(std::string_view text) noexcept {
    if (text.empty() || (text.size() > 1 && text.front() == '0')) {
        return std::nullopt;
    }

    SemanticVersion::component_type value = 0;
    constexpr auto maximum = std::numeric_limits<SemanticVersion::component_type>::max();

    for (const char character : text) {
        if (character < '0' || character > '9') {
            return std::nullopt;
        }

        const auto digit = static_cast<SemanticVersion::component_type>(character - '0');
        if (value > (maximum - digit) / 10U) {
            return std::nullopt;
        }

        value = value * 10U + digit;
    }

    return value;
}

} // namespace

std::optional<SemanticVersion> SemanticVersion::parse(std::string_view text) {
    const auto first_separator = text.find('.');
    if (first_separator == std::string_view::npos) {
        return std::nullopt;
    }

    const auto second_separator = text.find('.', first_separator + 1);
    if (second_separator == std::string_view::npos ||
        text.find('.', second_separator + 1) != std::string_view::npos) {
        return std::nullopt;
    }

    const auto major = parse_component(text.substr(0, first_separator));
    const auto minor =
        parse_component(text.substr(first_separator + 1, second_separator - first_separator - 1));
    const auto patch = parse_component(text.substr(second_separator + 1));
    if (!major.has_value() || !minor.has_value() || !patch.has_value()) {
        return std::nullopt;
    }

    return SemanticVersion{*major, *minor, *patch, std::string{text}};
}

} // namespace corerpg
