#pragma once

#include <string>
#include <variant>
#include <vector>

#include "corerpg/content_definition_input.hpp"
#include "corerpg/content_manifest.hpp"
#include "corerpg/content_json/json_document_reader.hpp"
#include "corerpg/content_json/json_schema_validator.hpp"

namespace corerpg::content_json {

class ContentPackManifestDecodeResult;

enum class JsonMappingDiagnosticCode {
    invalid_pack_id,
    invalid_definition_id,
    invalid_resource_kind,
    invalid_semantic_version,
    invalid_version_requirement,
};

class JsonMappingDiagnostic final {
public:
    JsonMappingDiagnostic(JsonMappingDiagnosticCode code, std::string instance_location,
                          std::string message);

    [[nodiscard]] JsonMappingDiagnosticCode code() const noexcept;
    [[nodiscard]] const std::string& instance_location() const noexcept;
    [[nodiscard]] const std::string& message() const noexcept;

private:
    JsonMappingDiagnosticCode code_;
    std::string instance_location_;
    std::string message_;
};

class DecodedContentPackManifest final {
public:
    [[nodiscard]] const ContentManifest& manifest() const noexcept;
    [[nodiscard]] const std::vector<std::string>& resource_locators() const noexcept;

private:
    friend ContentPackManifestDecodeResult decode_content_pack_manifest(
        const JsonDocument& document);

    DecodedContentPackManifest(ContentManifest manifest,
                               std::vector<std::string> resource_locators);

    ContentManifest manifest_;
    std::vector<std::string> resource_locators_;
};

class ContentPackManifestDecodeResult final {
public:
    [[nodiscard]] bool succeeded() const noexcept;
    [[nodiscard]] const DecodedContentPackManifest* decoded_manifest() const noexcept;
    [[nodiscard]] const std::vector<JsonSchemaDiagnostic>* schema_diagnostics() const noexcept;
    [[nodiscard]] const JsonMappingDiagnostic* mapping_diagnostic() const noexcept;

private:
    friend ContentPackManifestDecodeResult decode_content_pack_manifest(
        const JsonDocument& document);

    explicit ContentPackManifestDecodeResult(DecodedContentPackManifest decoded_manifest);
    explicit ContentPackManifestDecodeResult(
        std::vector<JsonSchemaDiagnostic> schema_diagnostics);
    explicit ContentPackManifestDecodeResult(JsonMappingDiagnostic mapping_diagnostic);

    std::variant<DecodedContentPackManifest, std::vector<JsonSchemaDiagnostic>,
                 JsonMappingDiagnostic>
        result_;
};

class ResourceDefinitionDecodeResult final {
public:
    [[nodiscard]] bool succeeded() const noexcept;
    [[nodiscard]] const ContentDefinitionInput* definition() const noexcept;
    [[nodiscard]] const std::vector<JsonSchemaDiagnostic>* schema_diagnostics() const noexcept;
    [[nodiscard]] const JsonMappingDiagnostic* mapping_diagnostic() const noexcept;

private:
    friend ResourceDefinitionDecodeResult decode_resource_definition(
        const JsonDocument& document);

    explicit ResourceDefinitionDecodeResult(ContentDefinitionInput definition);
    explicit ResourceDefinitionDecodeResult(
        std::vector<JsonSchemaDiagnostic> schema_diagnostics);
    explicit ResourceDefinitionDecodeResult(JsonMappingDiagnostic mapping_diagnostic);

    std::variant<ContentDefinitionInput, std::vector<JsonSchemaDiagnostic>,
                 JsonMappingDiagnostic>
        result_;
};

[[nodiscard]] ContentPackManifestDecodeResult decode_content_pack_manifest(
    const JsonDocument& document);

[[nodiscard]] ResourceDefinitionDecodeResult decode_resource_definition(
    const JsonDocument& document);

}  // namespace corerpg::content_json
