#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

#include <gtest/gtest.h>

#include "corerpg/content_json/json_document_reader.hpp"
#include "corerpg/content_json/json_schema_validator.hpp"

namespace {

using corerpg::content_json::JsonReaderOptions;
using corerpg::content_json::JsonSchemaDiagnostic;
using corerpg::content_json::JsonSchemaDiagnosticCode;
using corerpg::content_json::JsonSchemaRole;
using corerpg::content_json::JsonSchemaValidationResult;
using corerpg::content_json::read_json_document;
using corerpg::content_json::validate_json_document;

constexpr std::size_t test_document_limit = 1024U * 1024U;

std::string read_fixture(std::string_view relative_path) {
    const auto path = std::filesystem::path(CORERPG_SOURCE_DIR) / relative_path;
    std::ifstream input(path, std::ios::binary);
    if (!input.is_open()) {
        throw std::runtime_error("Cannot open fixture: " + path.string());
    }
    return std::string(std::istreambuf_iterator<char>(input),
                       std::istreambuf_iterator<char>());
}

JsonSchemaValidationResult validate(std::string_view text, JsonSchemaRole role) {
    auto parsed = read_json_document(text, JsonReaderOptions{test_document_limit});
    if (!parsed.succeeded()) {
        throw std::runtime_error("Schema test input is not strict JSON");
    }
    return validate_json_document(*parsed.document(), role);
}

void expect_valid(const JsonSchemaValidationResult& result) {
    EXPECT_TRUE(result.succeeded());
    EXPECT_EQ(result.diagnostics(), nullptr);
}

const std::vector<JsonSchemaDiagnostic>& expect_invalid(
    const JsonSchemaValidationResult& result) {
    EXPECT_FALSE(result.succeeded());
    if (result.diagnostics() == nullptr) {
        ADD_FAILURE() << "Expected schema diagnostics";
        static const std::vector<JsonSchemaDiagnostic> missing;
        return missing;
    }
    EXPECT_FALSE(result.diagnostics()->empty());
    for (const auto& diagnostic : *result.diagnostics()) {
        EXPECT_EQ(diagnostic.code(), JsonSchemaDiagnosticCode::schema_violation);
        EXPECT_FALSE(diagnostic.schema_uri().empty());
        EXPECT_FALSE(diagnostic.schema_location().empty());
        EXPECT_FALSE(diagnostic.keyword().empty());
        EXPECT_FALSE(diagnostic.message().empty());
    }
    return *result.diagnostics();
}

bool has_keyword(const JsonSchemaValidationResult& result, std::string_view keyword) {
    if (result.diagnostics() == nullptr) {
        return false;
    }
    return std::ranges::any_of(*result.diagnostics(), [&](const auto& diagnostic) {
        return diagnostic.keyword() == keyword;
    });
}

constexpr std::string_view manifest_without_schema = R"({
  "pack_id": "example.test",
  "pack_version": "1.0.0",
  "content_contract_version": "0.1.0",
  "minimum_core_version": "0.1.0",
  "dependencies": [],
  "resources": [],
  "display_name": "Example Test",
  "description": ""
})";

constexpr std::string_view resource_without_schema = R"({
  "id": "example.test:things/a",
  "kind": "thing",
  "references": []
})";

TEST(JsonSchemaValidatorTest, ExampleManifestIsValidForManifestRole) {
    expect_valid(validate(read_fixture("content-packs/example.test/manifest.json"),
                          JsonSchemaRole::content_pack_manifest));
}

TEST(JsonSchemaValidatorTest, ExampleResourceAIsValidForResourceRole) {
    expect_valid(validate(read_fixture("content-packs/example.test/resources/things/a.json"),
                          JsonSchemaRole::resource_definition));
}

TEST(JsonSchemaValidatorTest, ExampleResourceBIsValidForResourceRole) {
    expect_valid(validate(read_fixture("content-packs/example.test/resources/things/b.json"),
                          JsonSchemaRole::resource_definition));
}

TEST(JsonSchemaValidatorTest, ManifestWithoutSchemaPropertyIsValid) {
    expect_valid(validate(manifest_without_schema, JsonSchemaRole::content_pack_manifest));
}

TEST(JsonSchemaValidatorTest, ResourceWithoutSchemaPropertyIsValid) {
    expect_valid(validate(resource_without_schema, JsonSchemaRole::resource_definition));
}

TEST(JsonSchemaValidatorManifestTest, RejectsMissingRequiredField) {
    const auto result = validate(R"({
      "pack_version":"1.0.0","content_contract_version":"0.1.0",
      "minimum_core_version":"0.1.0","dependencies":[],"resources":[],
      "display_name":"","description":""
    })", JsonSchemaRole::content_pack_manifest);
    expect_invalid(result);
    EXPECT_TRUE(has_keyword(result, "required"));
}

TEST(JsonSchemaValidatorManifestTest, RejectsUnknownField) {
    const auto result = validate(R"({
      "pack_id":"example.test","pack_version":"1.0.0",
      "content_contract_version":"0.1.0","minimum_core_version":"0.1.0",
      "dependencies":[],"resources":[],"display_name":"","description":"",
      "unknown":true
    })", JsonSchemaRole::content_pack_manifest);
    expect_invalid(result);
    EXPECT_TRUE(has_keyword(result, "additionalProperties"));
}

TEST(JsonSchemaValidatorManifestTest, RejectsIncorrectSchemaProperty) {
    const auto result = validate(R"({
      "$schema":"https://evil.example/schema.json","pack_id":"example.test",
      "pack_version":"1.0.0","content_contract_version":"0.1.0",
      "minimum_core_version":"0.1.0","dependencies":[],"resources":[],
      "display_name":"","description":""
    })", JsonSchemaRole::content_pack_manifest);
    expect_invalid(result);
    EXPECT_TRUE(has_keyword(result, "const"));
}

TEST(JsonSchemaValidatorManifestTest, RejectsUppercasePackId) {
    const auto result = validate(R"({
      "pack_id":"Example.test","pack_version":"1.0.0",
      "content_contract_version":"0.1.0","minimum_core_version":"0.1.0",
      "dependencies":[],"resources":[],"display_name":"","description":""
    })", JsonSchemaRole::content_pack_manifest);
    expect_invalid(result);
    EXPECT_TRUE(has_keyword(result, "pattern"));
}

TEST(JsonSchemaValidatorManifestTest, RejectsInvalidSemanticVersion) {
    const auto result = validate(R"({
      "pack_id":"example.test","pack_version":"1.0",
      "content_contract_version":"0.1.0","minimum_core_version":"0.1.0",
      "dependencies":[],"resources":[],"display_name":"","description":""
    })", JsonSchemaRole::content_pack_manifest);
    expect_invalid(result);
    EXPECT_TRUE(has_keyword(result, "pattern"));
}

TEST(JsonSchemaValidatorManifestTest, RejectsInvalidResourceLocator) {
    const auto result = validate(R"({
      "pack_id":"example.test","pack_version":"1.0.0",
      "content_contract_version":"0.1.0","minimum_core_version":"0.1.0",
      "dependencies":[],"resources":["../escape.json"],
      "display_name":"","description":""
    })", JsonSchemaRole::content_pack_manifest);
    expect_invalid(result);
    EXPECT_TRUE(has_keyword(result, "pattern"));
}

TEST(JsonSchemaValidatorManifestTest, RejectsDuplicateResources) {
    const auto result = validate(R"({
      "pack_id":"example.test","pack_version":"1.0.0",
      "content_contract_version":"0.1.0","minimum_core_version":"0.1.0",
      "dependencies":[],
      "resources":["resources/a.json","resources/a.json"],
      "display_name":"","description":""
    })", JsonSchemaRole::content_pack_manifest);
    expect_invalid(result);
    EXPECT_TRUE(has_keyword(result, "uniqueItems"));
}

TEST(JsonSchemaValidatorManifestTest, RejectsIncorrectDependenciesType) {
    const auto result = validate(R"({
      "pack_id":"example.test","pack_version":"1.0.0",
      "content_contract_version":"0.1.0","minimum_core_version":"0.1.0",
      "dependencies":{},"resources":[],"display_name":"","description":""
    })", JsonSchemaRole::content_pack_manifest);
    expect_invalid(result);
    EXPECT_TRUE(has_keyword(result, "type"));
}

TEST(JsonSchemaValidatorManifestTest, RejectsInvalidDependencyStructure) {
    const auto result = validate(R"({
      "pack_id":"example.test","pack_version":"1.0.0",
      "content_contract_version":"0.1.0","minimum_core_version":"0.1.0",
      "dependencies":[{"pack_id":"example.base"}],"resources":[],
      "display_name":"","description":""
    })", JsonSchemaRole::content_pack_manifest);
    expect_invalid(result);
    EXPECT_TRUE(has_keyword(result, "required"));
}

TEST(JsonSchemaValidatorResourceTest, RejectsInvalidDefinitionId) {
    const auto result = validate(
        R"({"id":"Example.test:things/a","kind":"thing","references":[]})",
        JsonSchemaRole::resource_definition);
    expect_invalid(result);
    EXPECT_TRUE(has_keyword(result, "pattern"));
}

TEST(JsonSchemaValidatorResourceTest, RejectsInvalidKind) {
    const auto result = validate(
        R"({"id":"example.test:things/a","kind":"Thing","references":[]})",
        JsonSchemaRole::resource_definition);
    expect_invalid(result);
    EXPECT_TRUE(has_keyword(result, "pattern"));
}

TEST(JsonSchemaValidatorResourceTest, RejectsMissingRequiredField) {
    const auto result = validate(R"({"id":"example.test:things/a","kind":"thing"})",
                                 JsonSchemaRole::resource_definition);
    expect_invalid(result);
    EXPECT_TRUE(has_keyword(result, "required"));
}

TEST(JsonSchemaValidatorResourceTest, RejectsUnknownField) {
    const auto result = validate(R"({
      "id":"example.test:things/a","kind":"thing","references":[],"data":{}
    })", JsonSchemaRole::resource_definition);
    expect_invalid(result);
    EXPECT_TRUE(has_keyword(result, "additionalProperties"));
}

TEST(JsonSchemaValidatorResourceTest, RejectsIncorrectSchemaProperty) {
    const auto result = validate(R"({
      "$schema":"https://evil.example/schema.json",
      "id":"example.test:things/a","kind":"thing","references":[]
    })", JsonSchemaRole::resource_definition);
    expect_invalid(result);
    EXPECT_TRUE(has_keyword(result, "const"));
}

TEST(JsonSchemaValidatorResourceTest, RejectsIncorrectReferencesType) {
    const auto result = validate(R"({
      "id":"example.test:things/a","kind":"thing","references":{}
    })", JsonSchemaRole::resource_definition);
    expect_invalid(result);
    EXPECT_TRUE(has_keyword(result, "type"));
}

TEST(JsonSchemaValidatorResourceTest, RejectsInvalidReferenceTarget) {
    const auto result = validate(R"({
      "id":"example.test:things/a","kind":"thing",
      "references":[{"target":"invalid","expected_kind":"thing"}]
    })", JsonSchemaRole::resource_definition);
    expect_invalid(result);
    EXPECT_TRUE(has_keyword(result, "pattern"));
}

TEST(JsonSchemaValidatorResourceTest, RejectsInvalidExpectedKind) {
    const auto result = validate(R"({
      "id":"example.test:things/a","kind":"thing",
      "references":[{"target":"example.test:things/b","expected_kind":"Thing"}]
    })", JsonSchemaRole::resource_definition);
    expect_invalid(result);
    EXPECT_TRUE(has_keyword(result, "pattern"));
}

TEST(JsonSchemaValidatorResourceTest, RejectsDuplicateReferences) {
    const auto result = validate(R"({
      "id":"example.test:things/a","kind":"thing",
      "references":[
        {"target":"example.test:things/b","expected_kind":"thing"},
        {"target":"example.test:things/b","expected_kind":"thing"}
      ]
    })", JsonSchemaRole::resource_definition);
    expect_invalid(result);
    EXPECT_TRUE(has_keyword(result, "uniqueItems"));
}

TEST(JsonSchemaValidatorBoundaryTest, AllowsSemanticallyDuplicateDependencies) {
    const auto result = validate(R"({
      "pack_id":"example.test","pack_version":"1.0.0",
      "content_contract_version":"0.1.0","minimum_core_version":"0.1.0",
      "dependencies":[
        {"pack_id":"example.base","version_requirement":{"minimum_inclusive":"1.0.0"}},
        {"pack_id":"example.base","version_requirement":{"minimum_inclusive":"2.0.0"}}
      ],"resources":[],"display_name":"","description":""
    })", JsonSchemaRole::content_pack_manifest);
    expect_valid(result);
}

TEST(JsonSchemaValidatorBoundaryTest, DoesNotResolveLogicalReferences) {
    const auto result = validate(R"({
      "id":"example.test:things/a","kind":"thing",
      "references":[{"target":"missing.pack:things/b","expected_kind":"other"}]
    })", JsonSchemaRole::resource_definition);
    expect_valid(result);
}

TEST(JsonSchemaValidatorRoleTest, ResourceFailsWhenValidatedAsManifest) {
    const auto result = validate(resource_without_schema,
                                 JsonSchemaRole::content_pack_manifest);
    expect_invalid(result);
    EXPECT_TRUE(has_keyword(result, "required"));
}

TEST(JsonSchemaValidatorRoleTest, ManifestFailsWhenValidatedAsResource) {
    const auto result = validate(manifest_without_schema,
                                 JsonSchemaRole::resource_definition);
    expect_invalid(result);
    EXPECT_TRUE(has_keyword(result, "required"));
}

TEST(JsonSchemaValidatorSecurityTest, ExternalSchemaPropertyIsDataNotAResolverInput) {
    const auto result = validate(R"({
      "$schema":"https://evil.example/schema.json",
      "id":"example.test:things/a","kind":"thing","references":[]
    })", JsonSchemaRole::resource_definition);
    const auto& diagnostics = expect_invalid(result);
    EXPECT_TRUE(has_keyword(result, "const"));
    EXPECT_TRUE(std::ranges::all_of(diagnostics, [](const auto& diagnostic) {
        return diagnostic.schema_uri().find("evil.example") == std::string::npos;
    }));
}

TEST(JsonSchemaValidatorResultTest, SuccessContainsNoDiagnostics) {
    const auto result = validate(resource_without_schema,
                                 JsonSchemaRole::resource_definition);
    EXPECT_TRUE(result.succeeded());
    EXPECT_EQ(result.diagnostics(), nullptr);
}

TEST(JsonSchemaValidatorResultTest, FailureContainsDiagnosticsOnly) {
    const auto result = validate("{}", JsonSchemaRole::resource_definition);
    EXPECT_FALSE(result.succeeded());
    EXPECT_NE(result.diagnostics(), nullptr);
    EXPECT_FALSE(result.diagnostics()->empty());
}

TEST(JsonSchemaValidatorResultTest, CollectsMultipleViolationsDeterministically) {
    const auto result = validate(R"({
      "id":"INVALID","kind":"INVALID","references":[],"unknown":true
    })", JsonSchemaRole::resource_definition);
    const auto& diagnostics = expect_invalid(result);
    EXPECT_GE(diagnostics.size(), 3U);
    EXPECT_TRUE(std::is_sorted(diagnostics.begin(), diagnostics.end(),
                               [](const auto& left, const auto& right) {
        return std::tie(left.instance_location(), left.schema_uri(),
                        left.schema_location(), left.keyword(), left.message()) <
               std::tie(right.instance_location(), right.schema_uri(),
                        right.schema_location(), right.keyword(), right.message());
    }));
}

TEST(JsonSchemaValidatorTest, PublicSchemaHeaderDoesNotExposeJsoncons) {
    const auto header =
        read_fixture("include/corerpg/content_json/json_schema_validator.hpp");
    EXPECT_EQ(header.find("jsoncons"), std::string::npos);
}

}  // namespace
