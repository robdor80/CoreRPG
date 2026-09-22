#include "corerpg/content_json/json_document_reader.hpp"

#include <cstddef>
#include <iterator>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <unordered_set>
#include <utility>
#include <vector>

#include <jsoncons/config/version.hpp>
#include <jsoncons/json_cursor.hpp>
#include <jsoncons/json.hpp>

static_assert(JSONCONS_VERSION_MAJOR == 1);
static_assert(JSONCONS_VERSION_MINOR == 9);
static_assert(JSONCONS_VERSION_PATCH == 0);

namespace corerpg::content_json {
namespace {

[[nodiscard]] JsonDiagnostic invalid_json(std::optional<std::size_t> line,
                                          std::optional<std::size_t> column) {
    return JsonDiagnostic(JsonDiagnosticCode::invalid_json, line, column, std::nullopt);
}

[[nodiscard]] jsoncons::json_options strict_options() {
    jsoncons::json_options options;
    options.allow_comments(false);
    options.allow_trailing_comma(false);
    return options;
}

[[nodiscard]] int hex_value(char value) noexcept {
    if (value >= '0' && value <= '9') {
        return value - '0';
    }
    if (value >= 'a' && value <= 'f') {
        return value - 'a' + 10;
    }
    if (value >= 'A' && value <= 'F') {
        return value - 'A' + 10;
    }
    return -1;
}

[[nodiscard]] std::optional<unsigned int> escaped_code_unit(std::string_view text,
                                                            std::size_t slash) noexcept {
    if (slash + 6U > text.size() || text[slash] != '\\' || text[slash + 1U] != 'u') {
        return std::nullopt;
    }

    unsigned int value = 0;
    for (std::size_t index = slash + 2U; index < slash + 6U; ++index) {
        const int digit = hex_value(text[index]);
        if (digit < 0) {
            return std::nullopt;
        }
        value = value * 16U + static_cast<unsigned int>(digit);
    }
    return value;
}

[[nodiscard]] std::pair<std::size_t, std::size_t> line_and_column(
    std::string_view text, std::size_t offset) noexcept {
    std::size_t line = 1U;
    std::size_t column = 1U;
    for (std::size_t index = 0; index < offset; ++index) {
        if (text[index] == '\n') {
            ++line;
            column = 1U;
        } else {
            ++column;
        }
    }
    return {line, column};
}

[[nodiscard]] std::optional<JsonDiagnostic> validate_surrogate_escapes(
    std::string_view text) {
    bool in_string = false;
    for (std::size_t index = 0; index < text.size(); ++index) {
        if (!in_string) {
            if (text[index] == '"') {
                in_string = true;
            }
            continue;
        }

        if (text[index] == '"') {
            in_string = false;
            continue;
        }
        if (text[index] != '\\') {
            continue;
        }
        if (index + 1U >= text.size()) {
            break;
        }
        if (text[index + 1U] != 'u') {
            ++index;
            continue;
        }

        const auto first = escaped_code_unit(text, index);
        if (!first.has_value()) {
            continue;
        }
        constexpr unsigned int high_begin = 0xD800U;
        constexpr unsigned int high_end = 0xDBFFU;
        constexpr unsigned int low_begin = 0xDC00U;
        constexpr unsigned int low_end = 0xDFFFU;

        if (*first >= low_begin && *first <= low_end) {
            const auto [line, column] = line_and_column(text, index);
            return invalid_json(line, column);
        }
        if (*first >= high_begin && *first <= high_end) {
            const auto second = escaped_code_unit(text, index + 6U);
            if (!second.has_value() || *second < low_begin || *second > low_end) {
                const auto [line, column] = line_and_column(text, index);
                return invalid_json(line, column);
            }
            index += 11U;
            continue;
        }
        index += 5U;
    }
    return std::nullopt;
}

[[nodiscard]] std::optional<JsonDiagnostic> validate_events(std::string_view text) {
    std::error_code error;
    jsoncons::json_string_cursor cursor(text, strict_options(), error);
    if (error) {
        return invalid_json(cursor.line(), cursor.column());
    }

    std::vector<std::unordered_set<std::string>> object_keys;
    while (!cursor.done()) {
        const auto& event = cursor.current();
        switch (event.event_type()) {
            case jsoncons::staj_event_type::begin_object:
                object_keys.emplace_back();
                break;
            case jsoncons::staj_event_type::key: {
                const auto key = event.get<std::string>();
                if (object_keys.empty()) {
                    return invalid_json(cursor.line(), cursor.column());
                }
                const auto [unused, inserted] = object_keys.back().insert(key);
                static_cast<void>(unused);
                if (!inserted) {
                    return JsonDiagnostic(JsonDiagnosticCode::duplicate_object_key,
                                          cursor.line(), cursor.column(), key);
                }
                break;
            }
            case jsoncons::staj_event_type::end_object:
                if (object_keys.empty()) {
                    return invalid_json(cursor.line(), cursor.column());
                }
                object_keys.pop_back();
                break;
            default:
                break;
        }

        cursor.next(error);
        if (error) {
            return invalid_json(cursor.line(), cursor.column());
        }
    }

    cursor.check_done(error);
    if (error) {
        return invalid_json(cursor.line(), cursor.column());
    }
    return std::nullopt;
}

}  // namespace

struct JsonDocument::Impl final {
    explicit Impl(jsoncons::json parsed_value) : value(std::move(parsed_value)) {}

    jsoncons::json value;
};

JsonDiagnostic::JsonDiagnostic(JsonDiagnosticCode code, std::optional<std::size_t> line,
                               std::optional<std::size_t> column,
                               std::optional<std::string> duplicate_key)
    : code_(code),
      line_(line),
      column_(column),
      duplicate_key_(std::move(duplicate_key)) {}

JsonDiagnosticCode JsonDiagnostic::code() const noexcept {
    return code_;
}

const std::optional<std::size_t>& JsonDiagnostic::line() const noexcept {
    return line_;
}

const std::optional<std::size_t>& JsonDiagnostic::column() const noexcept {
    return column_;
}

const std::optional<std::string>& JsonDiagnostic::duplicate_key() const noexcept {
    return duplicate_key_;
}

JsonDocument::JsonDocument(std::unique_ptr<Impl> implementation)
    : implementation_(std::move(implementation)) {}

JsonDocument::JsonDocument(JsonDocument&&) noexcept = default;
JsonDocument& JsonDocument::operator=(JsonDocument&&) noexcept = default;
JsonDocument::~JsonDocument() = default;

JsonReadResult::JsonReadResult(JsonDocument document) : result_(std::move(document)) {}

JsonReadResult::JsonReadResult(JsonDiagnostic diagnostic)
    : result_(std::move(diagnostic)) {}

bool JsonReadResult::succeeded() const noexcept {
    return std::holds_alternative<JsonDocument>(result_);
}

const JsonDocument* JsonReadResult::document() const noexcept {
    return std::get_if<JsonDocument>(&result_);
}

const JsonDiagnostic* JsonReadResult::diagnostic() const noexcept {
    return std::get_if<JsonDiagnostic>(&result_);
}

JsonReadResult read_json_document(std::span<const std::byte> bytes,
                                  JsonReaderOptions options) {
    if (bytes.size() > options.maximum_document_bytes) {
        return JsonReadResult(JsonDiagnostic(JsonDiagnosticCode::document_too_large,
                                             std::nullopt, std::nullopt, std::nullopt));
    }

    constexpr std::byte bom[] = {std::byte{0xEF}, std::byte{0xBB}, std::byte{0xBF}};
    if (bytes.size() >= std::size(bom) && bytes[0] == bom[0] && bytes[1] == bom[1] &&
        bytes[2] == bom[2]) {
        return JsonReadResult(JsonDiagnostic(JsonDiagnosticCode::bom_not_allowed, 1U, 1U,
                                             std::nullopt));
    }

    const char* text_data =
        bytes.empty() ? "" : reinterpret_cast<const char*>(bytes.data());
    const auto text = std::string_view(text_data, bytes.size());
    if (auto diagnostic = validate_surrogate_escapes(text); diagnostic.has_value()) {
        return JsonReadResult(std::move(*diagnostic));
    }
    if (auto diagnostic = validate_events(text); diagnostic.has_value()) {
        return JsonReadResult(std::move(*diagnostic));
    }

    try {
        auto value = jsoncons::json::parse(text, strict_options());
        return JsonReadResult(
            JsonDocument(std::make_unique<JsonDocument::Impl>(std::move(value))));
    } catch (const jsoncons::ser_error& error) {
        return JsonReadResult(invalid_json(error.line(), error.column()));
    }
}

JsonReadResult read_json_document(std::string_view text, JsonReaderOptions options) {
    const auto* data = reinterpret_cast<const std::byte*>(text.data());
    return read_json_document(std::span<const std::byte>(data, text.size()), options);
}

}  // namespace corerpg::content_json
