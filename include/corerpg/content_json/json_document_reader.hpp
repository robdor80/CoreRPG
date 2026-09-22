#pragma once

#include <cstddef>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <variant>

namespace corerpg::content_json {

enum class JsonDiagnosticCode {
    bom_not_allowed,
    document_too_large,
    invalid_json,
    duplicate_object_key,
};

class JsonReadResult;
struct JsonReaderOptions;

[[nodiscard]] JsonReadResult read_json_document(std::span<const std::byte> bytes,
                                                JsonReaderOptions options);

class JsonDiagnostic final {
public:
    JsonDiagnostic(JsonDiagnosticCode code, std::optional<std::size_t> line,
                   std::optional<std::size_t> column,
                   std::optional<std::string> duplicate_key);

    [[nodiscard]] JsonDiagnosticCode code() const noexcept;
    [[nodiscard]] const std::optional<std::size_t>& line() const noexcept;
    [[nodiscard]] const std::optional<std::size_t>& column() const noexcept;
    [[nodiscard]] const std::optional<std::string>& duplicate_key() const noexcept;

private:
    JsonDiagnosticCode code_;
    std::optional<std::size_t> line_;
    std::optional<std::size_t> column_;
    std::optional<std::string> duplicate_key_;
};

class JsonDocument final {
public:
    JsonDocument(JsonDocument&&) noexcept;
    JsonDocument& operator=(JsonDocument&&) noexcept;
    ~JsonDocument();

    JsonDocument(const JsonDocument&) = delete;
    JsonDocument& operator=(const JsonDocument&) = delete;

private:
    friend JsonReadResult read_json_document(std::span<const std::byte> bytes,
                                             JsonReaderOptions options);

    struct Impl;
    explicit JsonDocument(std::unique_ptr<Impl> implementation);

    std::unique_ptr<Impl> implementation_;
};

class JsonReadResult final {
public:
    [[nodiscard]] bool succeeded() const noexcept;
    [[nodiscard]] const JsonDocument* document() const noexcept;
    [[nodiscard]] const JsonDiagnostic* diagnostic() const noexcept;

private:
    friend JsonReadResult read_json_document(std::span<const std::byte> bytes,
                                             JsonReaderOptions options);

    explicit JsonReadResult(JsonDocument document);
    explicit JsonReadResult(JsonDiagnostic diagnostic);

    std::variant<JsonDocument, JsonDiagnostic> result_;
};

struct JsonReaderOptions final {
    std::size_t maximum_document_bytes;
};

[[nodiscard]] JsonReadResult read_json_document(std::string_view text,
                                                JsonReaderOptions options);

}  // namespace corerpg::content_json
