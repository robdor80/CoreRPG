#include <corerpg/definition_id.hpp>
#include <corerpg/pack_id.hpp>

#include <cstddef>
#include <string>
#include <string_view>
#include <utility>

namespace corerpg {
namespace {

[[nodiscard]] constexpr bool is_ascii_lower(char character) noexcept {
    return character >= 'a' && character <= 'z';
}

[[nodiscard]] constexpr bool is_ascii_digit(char character) noexcept {
    return character >= '0' && character <= '9';
}

[[nodiscard]] bool is_valid_pack_id(std::string_view text) noexcept {
    std::size_t segment_count = 1;
    bool at_segment_start = true;

    for (const char character : text) {
        if (character == '.') {
            if (at_segment_start) {
                return false;
            }

            ++segment_count;
            at_segment_start = true;
            continue;
        }

        if (at_segment_start) {
            if (!is_ascii_lower(character)) {
                return false;
            }

            at_segment_start = false;
            continue;
        }

        if (!is_ascii_lower(character) && !is_ascii_digit(character) && character != '-') {
            return false;
        }
    }

    return !at_segment_start && segment_count >= 2;
}

[[nodiscard]] bool is_valid_local_path(std::string_view text) noexcept {
    bool at_segment_start = true;

    for (const char character : text) {
        if (character == '/') {
            if (at_segment_start) {
                return false;
            }

            at_segment_start = true;
            continue;
        }

        if (at_segment_start) {
            if (!is_ascii_lower(character) && !is_ascii_digit(character)) {
                return false;
            }

            at_segment_start = false;
            continue;
        }

        if (!is_ascii_lower(character) && !is_ascii_digit(character) && character != '.' &&
            character != '_' && character != '-') {
            return false;
        }
    }

    return !at_segment_start;
}

} // namespace

std::optional<PackId> PackId::parse(std::string_view text) {
    if (!is_valid_pack_id(text)) {
        return std::nullopt;
    }

    return PackId{std::string{text}};
}

std::optional<DefinitionId> DefinitionId::parse(std::string_view text) {
    const auto separator = text.find(':');
    if (separator == std::string_view::npos) {
        return std::nullopt;
    }

    auto pack_id = PackId::parse(text.substr(0, separator));
    const auto local_path = text.substr(separator + 1);
    if (!pack_id.has_value() || !is_valid_local_path(local_path)) {
        return std::nullopt;
    }

    return DefinitionId{std::move(*pack_id), std::string{local_path}, std::string{text}};
}

bool DefinitionId::is_valid_text(std::string_view text) noexcept {
    const auto separator = text.find(':');
    return separator != std::string_view::npos && is_valid_pack_id(text.substr(0, separator)) &&
           is_valid_local_path(text.substr(separator + 1));
}

} // namespace corerpg
