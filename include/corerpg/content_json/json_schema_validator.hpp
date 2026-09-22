#pragma once

#include <string>
#include <variant>
#include <vector>

#include "corerpg/content_json/json_document_reader.hpp"

namespace corerpg::content_json {

enum class JsonSchemaRole {
    content_pack_manifest,
    resource_definition,
};

enum class JsonSchemaDiagnosticCode {
    schema_violation,
    schema_configuration_error,
};

class JsonSchemaDiagnostic final {
public:
    JsonSchemaDiagnostic(JsonSchemaDiagnosticCode code, std::string instance_location,
                         std::string schema_uri, std::string schema_location,
                         std::string keyword, std::string message);

    [[nodiscard]] JsonSchemaDiagnosticCode code() const noexcept;
    [[nodiscard]] const std::string& instance_location() const noexcept;
    [[nodiscard]] const std::string& schema_uri() const noexcept;
    [[nodiscard]] const std::string& schema_location() const noexcept;
    [[nodiscard]] const std::string& keyword() const noexcept;
    [[nodiscard]] const std::string& message() const noexcept;

private:
    JsonSchemaDiagnosticCode code_;
    std::string instance_location_;
    std::string schema_uri_;
    std::string schema_location_;
    std::string keyword_;
    std::string message_;
};

class JsonSchemaValidationResult final {
public:
    [[nodiscard]] bool succeeded() const noexcept;
    [[nodiscard]] const std::vector<JsonSchemaDiagnostic>* diagnostics() const noexcept;

private:
    friend class SchemaRegistry;
    friend JsonSchemaValidationResult validate_json_document(const JsonDocument& document,
                                                              JsonSchemaRole role);

    JsonSchemaValidationResult();
    explicit JsonSchemaValidationResult(std::vector<JsonSchemaDiagnostic> diagnostics);

    std::variant<std::monostate, std::vector<JsonSchemaDiagnostic>> result_;
};

[[nodiscard]] JsonSchemaValidationResult validate_json_document(
    const JsonDocument& document, JsonSchemaRole role);

}  // namespace corerpg::content_json
