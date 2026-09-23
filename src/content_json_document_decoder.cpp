#include "corerpg/content_json/content_document_decoder.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <jsoncons/json.hpp>

#include "content_json_document_internal.hpp"
#include "corerpg/definition_id.hpp"
#include "corerpg/definition_reference.hpp"
#include "corerpg/pack_dependency.hpp"
#include "corerpg/pack_id.hpp"
#include "corerpg/resource_definition.hpp"
#include "corerpg/resource_kind.hpp"
#include "corerpg/semantic_version.hpp"
#include "corerpg/version_requirement.hpp"

namespace corerpg::content_json {
namespace {

[[nodiscard]] std::string string_value(const jsoncons::json& object,
                                       std::string_view key) {
    return object.at(key).as<std::string>();
}

[[nodiscard]] JsonMappingDiagnostic invalid_mapping(JsonMappingDiagnosticCode code,
                                                    std::string instance_location,
                                                    std::string_view type_name) {
    return JsonMappingDiagnostic(code, std::move(instance_location),
                                 "Value cannot be converted to " + std::string(type_name));
}

[[nodiscard]] std::string indexed_location(std::string_view collection, std::size_t index,
                                           std::string_view field) {
    return std::string(collection) + "/" + std::to_string(index) + std::string(field);
}

}  // namespace

JsonMappingDiagnostic::JsonMappingDiagnostic(JsonMappingDiagnosticCode code,
                                             std::string instance_location,
                                             std::string message)
    : code_(code),
      instance_location_(std::move(instance_location)),
      message_(std::move(message)) {}

JsonMappingDiagnosticCode JsonMappingDiagnostic::code() const noexcept {
    return code_;
}

const std::string& JsonMappingDiagnostic::instance_location() const noexcept {
    return instance_location_;
}

const std::string& JsonMappingDiagnostic::message() const noexcept {
    return message_;
}

DecodedContentPackManifest::DecodedContentPackManifest(
    ContentManifest manifest, std::vector<std::string> resource_locators)
    : manifest_(std::move(manifest)),
      resource_locators_(std::move(resource_locators)) {}

const ContentManifest& DecodedContentPackManifest::manifest() const noexcept {
    return manifest_;
}

const std::vector<std::string>&
DecodedContentPackManifest::resource_locators() const noexcept {
    return resource_locators_;
}

ContentPackManifestDecodeResult::ContentPackManifestDecodeResult(
    DecodedContentPackManifest decoded_manifest)
    : result_(std::move(decoded_manifest)) {}

ContentPackManifestDecodeResult::ContentPackManifestDecodeResult(
    std::vector<JsonSchemaDiagnostic> schema_diagnostics)
    : result_(std::move(schema_diagnostics)) {}

ContentPackManifestDecodeResult::ContentPackManifestDecodeResult(
    JsonMappingDiagnostic mapping_diagnostic)
    : result_(std::move(mapping_diagnostic)) {}

bool ContentPackManifestDecodeResult::succeeded() const noexcept {
    return std::holds_alternative<DecodedContentPackManifest>(result_);
}

const DecodedContentPackManifest*
ContentPackManifestDecodeResult::decoded_manifest() const noexcept {
    return std::get_if<DecodedContentPackManifest>(&result_);
}

const std::vector<JsonSchemaDiagnostic>*
ContentPackManifestDecodeResult::schema_diagnostics() const noexcept {
    return std::get_if<std::vector<JsonSchemaDiagnostic>>(&result_);
}

const JsonMappingDiagnostic*
ContentPackManifestDecodeResult::mapping_diagnostic() const noexcept {
    return std::get_if<JsonMappingDiagnostic>(&result_);
}

ResourceDefinitionDecodeResult::ResourceDefinitionDecodeResult(
    ContentDefinitionInput definition)
    : result_(std::move(definition)) {}

ResourceDefinitionDecodeResult::ResourceDefinitionDecodeResult(
    std::vector<JsonSchemaDiagnostic> schema_diagnostics)
    : result_(std::move(schema_diagnostics)) {}

ResourceDefinitionDecodeResult::ResourceDefinitionDecodeResult(
    JsonMappingDiagnostic mapping_diagnostic)
    : result_(std::move(mapping_diagnostic)) {}

bool ResourceDefinitionDecodeResult::succeeded() const noexcept {
    return std::holds_alternative<ContentDefinitionInput>(result_);
}

const ContentDefinitionInput* ResourceDefinitionDecodeResult::definition() const noexcept {
    return std::get_if<ContentDefinitionInput>(&result_);
}

const std::vector<JsonSchemaDiagnostic>*
ResourceDefinitionDecodeResult::schema_diagnostics() const noexcept {
    return std::get_if<std::vector<JsonSchemaDiagnostic>>(&result_);
}

const JsonMappingDiagnostic*
ResourceDefinitionDecodeResult::mapping_diagnostic() const noexcept {
    return std::get_if<JsonMappingDiagnostic>(&result_);
}

ContentPackManifestDecodeResult decode_content_pack_manifest(const JsonDocument& document) {
    const auto validation =
        validate_json_document(document, JsonSchemaRole::content_pack_manifest);
    if (!validation.succeeded()) {
        return ContentPackManifestDecodeResult(*validation.diagnostics());
    }

    const auto& root = JsonDocumentAccess::value(document);

    auto pack_id = PackId::parse(string_value(root, "pack_id"));
    if (!pack_id.has_value()) {
        return ContentPackManifestDecodeResult(invalid_mapping(
            JsonMappingDiagnosticCode::invalid_pack_id, "/pack_id", "PackId"));
    }

    auto pack_version = SemanticVersion::parse(string_value(root, "pack_version"));
    if (!pack_version.has_value()) {
        return ContentPackManifestDecodeResult(invalid_mapping(
            JsonMappingDiagnosticCode::invalid_semantic_version, "/pack_version",
            "SemanticVersion"));
    }

    auto content_contract_version =
        SemanticVersion::parse(string_value(root, "content_contract_version"));
    if (!content_contract_version.has_value()) {
        return ContentPackManifestDecodeResult(invalid_mapping(
            JsonMappingDiagnosticCode::invalid_semantic_version,
            "/content_contract_version", "SemanticVersion"));
    }

    auto minimum_core_version =
        SemanticVersion::parse(string_value(root, "minimum_core_version"));
    if (!minimum_core_version.has_value()) {
        return ContentPackManifestDecodeResult(invalid_mapping(
            JsonMappingDiagnosticCode::invalid_semantic_version, "/minimum_core_version",
            "SemanticVersion"));
    }

    std::vector<PackDependency> dependencies;
    const auto& dependencies_json = root.at("dependencies");
    dependencies.reserve(dependencies_json.size());
    std::size_t dependency_index = 0;
    for (const auto& dependency_json : dependencies_json.array_range()) {
        auto dependency_pack_id =
            PackId::parse(string_value(dependency_json, "pack_id"));
        if (!dependency_pack_id.has_value()) {
            return ContentPackManifestDecodeResult(invalid_mapping(
                JsonMappingDiagnosticCode::invalid_pack_id,
                indexed_location("/dependencies", dependency_index, "/pack_id"),
                "PackId"));
        }

        const auto& requirement_json = dependency_json.at("version_requirement");
        auto minimum =
            SemanticVersion::parse(string_value(requirement_json, "minimum_inclusive"));
        if (!minimum.has_value()) {
            return ContentPackManifestDecodeResult(invalid_mapping(
                JsonMappingDiagnosticCode::invalid_semantic_version,
                indexed_location("/dependencies", dependency_index,
                                 "/version_requirement/minimum_inclusive"),
                "SemanticVersion"));
        }

        std::optional<SemanticVersion> maximum;
        if (requirement_json.contains("maximum_exclusive")) {
            maximum = SemanticVersion::parse(
                string_value(requirement_json, "maximum_exclusive"));
            if (!maximum.has_value()) {
                return ContentPackManifestDecodeResult(invalid_mapping(
                    JsonMappingDiagnosticCode::invalid_semantic_version,
                    indexed_location("/dependencies", dependency_index,
                                     "/version_requirement/maximum_exclusive"),
                    "SemanticVersion"));
            }
        }

        auto requirement =
            VersionRequirement::create(std::move(*minimum), std::move(maximum));
        if (!requirement.has_value()) {
            return ContentPackManifestDecodeResult(invalid_mapping(
                JsonMappingDiagnosticCode::invalid_version_requirement,
                indexed_location("/dependencies", dependency_index,
                                 "/version_requirement"),
                "VersionRequirement"));
        }

        dependencies.emplace_back(std::move(*dependency_pack_id),
                                  std::move(*requirement));
        ++dependency_index;
    }

    std::vector<std::string> resource_locators;
    const auto& resources_json = root.at("resources");
    resource_locators.reserve(resources_json.size());
    for (const auto& resource_json : resources_json.array_range()) {
        resource_locators.push_back(resource_json.as<std::string>());
    }

    ContentManifest manifest(
        std::move(*pack_id), std::move(*pack_version),
        std::move(*content_contract_version), std::move(*minimum_core_version),
        std::move(dependencies), string_value(root, "display_name"),
        string_value(root, "description"));
    return ContentPackManifestDecodeResult(DecodedContentPackManifest(
        std::move(manifest), std::move(resource_locators)));
}

ResourceDefinitionDecodeResult decode_resource_definition(const JsonDocument& document) {
    const auto validation =
        validate_json_document(document, JsonSchemaRole::resource_definition);
    if (!validation.succeeded()) {
        return ResourceDefinitionDecodeResult(*validation.diagnostics());
    }

    const auto& root = JsonDocumentAccess::value(document);
    auto id = DefinitionId::parse(string_value(root, "id"));
    if (!id.has_value()) {
        return ResourceDefinitionDecodeResult(invalid_mapping(
            JsonMappingDiagnosticCode::invalid_definition_id, "/id", "DefinitionId"));
    }

    auto kind = ResourceKind::parse(string_value(root, "kind"));
    if (!kind.has_value()) {
        return ResourceDefinitionDecodeResult(invalid_mapping(
            JsonMappingDiagnosticCode::invalid_resource_kind, "/kind", "ResourceKind"));
    }

    std::vector<DefinitionReference> references;
    const auto& references_json = root.at("references");
    references.reserve(references_json.size());
    std::size_t reference_index = 0;
    for (const auto& reference_json : references_json.array_range()) {
        auto target = DefinitionId::parse(string_value(reference_json, "target"));
        if (!target.has_value()) {
            return ResourceDefinitionDecodeResult(invalid_mapping(
                JsonMappingDiagnosticCode::invalid_definition_id,
                indexed_location("/references", reference_index, "/target"),
                "DefinitionId"));
        }

        auto expected_kind =
            ResourceKind::parse(string_value(reference_json, "expected_kind"));
        if (!expected_kind.has_value()) {
            return ResourceDefinitionDecodeResult(invalid_mapping(
                JsonMappingDiagnosticCode::invalid_resource_kind,
                indexed_location("/references", reference_index, "/expected_kind"),
                "ResourceKind"));
        }

        references.emplace_back(std::move(*target), std::move(*expected_kind));
        ++reference_index;
    }

    return ResourceDefinitionDecodeResult(ContentDefinitionInput(
        ResourceDefinition(std::move(*id), std::move(*kind)), std::move(references)));
}

}  // namespace corerpg::content_json
