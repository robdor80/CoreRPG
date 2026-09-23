#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "corerpg/content_json/content_document_decoder.hpp"
#include "corerpg/content_json/json_document_reader.hpp"

namespace {

using corerpg::content_json::ContentPackManifestDecodeResult;
using corerpg::content_json::JsonMappingDiagnosticCode;
using corerpg::content_json::JsonReaderOptions;
using corerpg::content_json::ResourceDefinitionDecodeResult;
using corerpg::content_json::decode_content_pack_manifest;
using corerpg::content_json::decode_resource_definition;
using corerpg::content_json::read_json_document;

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

ContentPackManifestDecodeResult decode_manifest(std::string_view text) {
    auto parsed = read_json_document(text, JsonReaderOptions{test_document_limit});
    if (!parsed.succeeded()) {
        throw std::runtime_error("Decoder test input is not strict JSON");
    }
    return decode_content_pack_manifest(*parsed.document());
}

ResourceDefinitionDecodeResult decode_resource(std::string_view text) {
    auto parsed = read_json_document(text, JsonReaderOptions{test_document_limit});
    if (!parsed.succeeded()) {
        throw std::runtime_error("Decoder test input is not strict JSON");
    }
    return decode_resource_definition(*parsed.document());
}

std::string manifest_with_versions(std::string_view pack_version,
                                   std::string_view contract_version,
                                   std::string_view core_version) {
    return R"({"pack_id":"example.test","pack_version":")" +
           std::string(pack_version) + R"(","content_contract_version":")" +
           std::string(contract_version) + R"(","minimum_core_version":")" +
           std::string(core_version) +
           R"(","dependencies":[],"resources":[],"display_name":"","description":""})";
}

TEST(ContentJsonDocumentDecoderTest, DecodesExampleManifestExactly) {
    const auto result = decode_manifest(read_fixture("content-packs/example.test/manifest.json"));

    ASSERT_TRUE(result.succeeded());
    ASSERT_NE(result.decoded_manifest(), nullptr);
    EXPECT_EQ(result.schema_diagnostics(), nullptr);
    EXPECT_EQ(result.mapping_diagnostic(), nullptr);

    const auto& decoded = *result.decoded_manifest();
    const auto& manifest = decoded.manifest();
    EXPECT_EQ(manifest.pack_id().value(), "example.test");
    EXPECT_EQ(manifest.pack_version().value(), "1.0.0");
    EXPECT_EQ(manifest.content_contract_version().value(), "0.1.0");
    EXPECT_EQ(manifest.minimum_core_version().value(), "0.1.0");
    EXPECT_TRUE(manifest.dependencies().empty());
    EXPECT_EQ(manifest.display_name(), "Example Test");
    EXPECT_EQ(manifest.description(),
              "Neutral content pack used to validate Content Contract 0.1.0.");
    EXPECT_EQ(decoded.resource_locators(),
              (std::vector<std::string>{"resources/things/a.json",
                                        "resources/things/b.json"}));
}

TEST(ContentJsonDocumentDecoderTest, BuildsDependenciesThroughCoreTypes) {
    const auto result = decode_manifest(R"({
      "pack_id":"example.test","pack_version":"1.0.0",
      "content_contract_version":"0.1.0","minimum_core_version":"0.1.0",
      "dependencies":[{
        "pack_id":"example.base",
        "version_requirement":{
          "minimum_inclusive":"1.2.3","maximum_exclusive":"2.0.0"
        }
      }],"resources":[],"display_name":"","description":""
    })");

    ASSERT_TRUE(result.succeeded());
    const auto& dependencies = result.decoded_manifest()->manifest().dependencies();
    ASSERT_EQ(dependencies.size(), 1U);
    EXPECT_EQ(dependencies[0].pack_id().value(), "example.base");
    EXPECT_EQ(dependencies[0].requirement().minimum_inclusive().value(), "1.2.3");
    ASSERT_TRUE(dependencies[0].requirement().maximum_exclusive().has_value());
    EXPECT_EQ(dependencies[0].requirement().maximum_exclusive()->value(), "2.0.0");
}

TEST(ContentJsonDocumentDecoderTest, DecodesExampleResourceAExactly) {
    const auto result = decode_resource(
        read_fixture("content-packs/example.test/resources/things/a.json"));

    ASSERT_TRUE(result.succeeded());
    ASSERT_NE(result.definition(), nullptr);
    EXPECT_EQ(result.schema_diagnostics(), nullptr);
    EXPECT_EQ(result.mapping_diagnostic(), nullptr);
    EXPECT_EQ(result.definition()->definition().id().value(), "example.test:things/a");
    EXPECT_EQ(result.definition()->definition().kind().value(), "thing");
    EXPECT_TRUE(result.definition()->references().empty());
}

TEST(ContentJsonDocumentDecoderTest, DecodesExampleResourceBExactly) {
    const auto result = decode_resource(
        read_fixture("content-packs/example.test/resources/things/b.json"));

    ASSERT_TRUE(result.succeeded());
    ASSERT_NE(result.definition(), nullptr);
    EXPECT_EQ(result.definition()->definition().id().value(), "example.test:things/b");
    EXPECT_EQ(result.definition()->definition().kind().value(), "thing");
    ASSERT_EQ(result.definition()->references().size(), 1U);
    EXPECT_EQ(result.definition()->references()[0].target().value(),
              "example.test:things/a");
    EXPECT_EQ(result.definition()->references()[0].expected_kind().value(), "thing");
}

TEST(ContentJsonDocumentDecoderTest, ManifestDecoderRejectsResourceAtSchemaLayer) {
    const auto result = decode_manifest(
        read_fixture("content-packs/example.test/resources/things/a.json"));

    EXPECT_FALSE(result.succeeded());
    EXPECT_EQ(result.decoded_manifest(), nullptr);
    ASSERT_NE(result.schema_diagnostics(), nullptr);
    EXPECT_FALSE(result.schema_diagnostics()->empty());
    EXPECT_EQ(result.mapping_diagnostic(), nullptr);
}

TEST(ContentJsonDocumentDecoderTest, ResourceDecoderRejectsManifestAtSchemaLayer) {
    const auto result = decode_resource(read_fixture("content-packs/example.test/manifest.json"));

    EXPECT_FALSE(result.succeeded());
    EXPECT_EQ(result.definition(), nullptr);
    ASSERT_NE(result.schema_diagnostics(), nullptr);
    EXPECT_FALSE(result.schema_diagnostics()->empty());
    EXPECT_EQ(result.mapping_diagnostic(), nullptr);
}

class SemanticVersionOverflowTest
    : public testing::TestWithParam<std::pair<std::string, std::string>> {};

TEST_P(SemanticVersionOverflowTest, ReportsConversionFailureAtVersionField) {
    constexpr std::string_view overflow = "4294967296.0.0";
    const auto& [field, expected_location] = GetParam();
    const auto result = decode_manifest(manifest_with_versions(
        field == "pack_version" ? overflow : "1.0.0",
        field == "content_contract_version" ? overflow : "0.1.0",
        field == "minimum_core_version" ? overflow : "0.1.0"));

    EXPECT_FALSE(result.succeeded());
    EXPECT_EQ(result.decoded_manifest(), nullptr);
    EXPECT_EQ(result.schema_diagnostics(), nullptr);
    ASSERT_NE(result.mapping_diagnostic(), nullptr);
    EXPECT_EQ(result.mapping_diagnostic()->code(),
              JsonMappingDiagnosticCode::invalid_semantic_version);
    EXPECT_EQ(result.mapping_diagnostic()->instance_location(), expected_location);
    EXPECT_FALSE(result.mapping_diagnostic()->message().empty());
}

INSTANTIATE_TEST_SUITE_P(
    ManifestVersions, SemanticVersionOverflowTest,
    testing::Values(std::pair<std::string, std::string>{"pack_version", "/pack_version"},
                    std::pair<std::string, std::string>{"content_contract_version",
                                                       "/content_contract_version"},
                    std::pair<std::string, std::string>{"minimum_core_version",
                                                       "/minimum_core_version"}));

TEST(ContentJsonDocumentDecoderTest, RejectsLogicallyInvalidVersionRequirement) {
    const auto result = decode_manifest(R"({
      "pack_id":"example.test","pack_version":"1.0.0",
      "content_contract_version":"0.1.0","minimum_core_version":"0.1.0",
      "dependencies":[{
        "pack_id":"example.base",
        "version_requirement":{
          "minimum_inclusive":"2.0.0","maximum_exclusive":"1.0.0"
        }
      }],"resources":[],"display_name":"","description":""
    })");

    EXPECT_FALSE(result.succeeded());
    EXPECT_EQ(result.schema_diagnostics(), nullptr);
    ASSERT_NE(result.mapping_diagnostic(), nullptr);
    EXPECT_EQ(result.mapping_diagnostic()->code(),
              JsonMappingDiagnosticCode::invalid_version_requirement);
    EXPECT_EQ(result.mapping_diagnostic()->instance_location(),
              "/dependencies/0/version_requirement");
}

TEST(ContentJsonDocumentDecoderTest, PublicDecoderHeaderDoesNotExposeJsoncons) {
    const auto header =
        read_fixture("include/corerpg/content_json/content_document_decoder.hpp");
    EXPECT_EQ(header.find("jsoncons"), std::string::npos);
}

}  // namespace
