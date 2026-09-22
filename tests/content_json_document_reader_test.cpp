#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

#include <gtest/gtest.h>

#include "corerpg/content_json/json_document_reader.hpp"

namespace {

using corerpg::content_json::JsonDiagnosticCode;
using corerpg::content_json::JsonDocument;
using corerpg::content_json::JsonReadResult;
using corerpg::content_json::JsonReaderOptions;
using corerpg::content_json::read_json_document;

constexpr std::size_t test_document_limit = 1024U * 1024U;

JsonReaderOptions options() {
    return JsonReaderOptions{test_document_limit};
}

JsonReadResult read(std::string_view text) {
    return read_json_document(text, options());
}

JsonReadResult read_bytes(const std::string& bytes) {
    const auto* data = reinterpret_cast<const std::byte*>(bytes.data());
    return read_json_document(std::span<const std::byte>(data, bytes.size()), options());
}

std::string read_fixture(std::string_view relative_path) {
    const auto path = std::filesystem::path(CORERPG_SOURCE_DIR) / relative_path;
    std::ifstream input(path, std::ios::binary);
    EXPECT_TRUE(input.is_open()) << path;
    return std::string(std::istreambuf_iterator<char>(input),
                       std::istreambuf_iterator<char>());
}

void expect_success(const JsonReadResult& result) {
    EXPECT_TRUE(result.succeeded());
    EXPECT_NE(result.document(), nullptr);
    EXPECT_EQ(result.diagnostic(), nullptr);
}

const corerpg::content_json::JsonDiagnostic& expect_failure(
    const JsonReadResult& result, JsonDiagnosticCode expected_code) {
    EXPECT_FALSE(result.succeeded());
    EXPECT_EQ(result.document(), nullptr);
    if (result.diagnostic() == nullptr) {
        ADD_FAILURE() << "Expected a structured JSON diagnostic";
        static const corerpg::content_json::JsonDiagnostic missing_diagnostic(
            JsonDiagnosticCode::invalid_json, std::nullopt, std::nullopt, std::nullopt);
        return missing_diagnostic;
    }
    EXPECT_EQ(result.diagnostic()->code(), expected_code);
    return *result.diagnostic();
}

static_assert(!std::is_copy_constructible_v<JsonDocument>);
static_assert(std::is_nothrow_move_constructible_v<JsonDocument>);

TEST(JsonDocumentReaderTest, AcceptsEmptyObject) {
    expect_success(read("{}"));
}

TEST(JsonDocumentReaderTest, AcceptsArray) {
    expect_success(read(R"([1,"two",false,null,{"value":3}])"));
}

TEST(JsonDocumentReaderTest, AcceptsEveryJsonPrimitiveAtDocumentRoot) {
    for (const std::string_view document : {R"("text")", "42", "true", "false", "null"}) {
        expect_success(read(document));
    }
}

TEST(JsonDocumentReaderTest, ParsesExampleManifestPhysically) {
    expect_success(read_bytes(read_fixture("content-packs/example.test/manifest.json")));
}

TEST(JsonDocumentReaderTest, ParsesExampleResourceAPhysically) {
    expect_success(
        read_bytes(read_fixture("content-packs/example.test/resources/things/a.json")));
}

TEST(JsonDocumentReaderTest, ParsesExampleResourceBPhysically) {
    expect_success(
        read_bytes(read_fixture("content-packs/example.test/resources/things/b.json")));
}

TEST(JsonDocumentReaderTest, RejectsMalformedJson) {
    const auto result = read(R"({"value":])");
    expect_failure(result, JsonDiagnosticCode::invalid_json);
}

TEST(JsonDocumentReaderTest, RejectsTrailingGarbage) {
    const auto result = read("{} trailing");
    expect_failure(result, JsonDiagnosticCode::invalid_json);
}

TEST(JsonDocumentReaderTest, RejectsUtf8BomBeforeParsing) {
    const std::string document = "\xEF\xBB\xBF{}";
    const auto result = read_bytes(document);
    const auto& diagnostic = expect_failure(result, JsonDiagnosticCode::bom_not_allowed);
    EXPECT_EQ(diagnostic.line(), 1U);
    EXPECT_EQ(diagnostic.column(), 1U);
}

TEST(JsonDocumentReaderTest, RejectsLineComment) {
    const auto result = read("{// comment\n}");
    expect_failure(result, JsonDiagnosticCode::invalid_json);
}

TEST(JsonDocumentReaderTest, RejectsBlockComment) {
    const auto result = read("{/* comment */}");
    expect_failure(result, JsonDiagnosticCode::invalid_json);
}

TEST(JsonDocumentReaderTest, RejectsTrailingComma) {
    const auto result = read(R"({"value":1,})");
    expect_failure(result, JsonDiagnosticCode::invalid_json);
}

TEST(JsonDocumentReaderTest, RejectsDuplicateKeyAtRoot) {
    const auto result = read(R"({"id":"first","id":"second"})");
    const auto& diagnostic =
        expect_failure(result, JsonDiagnosticCode::duplicate_object_key);
    ASSERT_TRUE(diagnostic.duplicate_key().has_value());
    EXPECT_EQ(*diagnostic.duplicate_key(), "id");
    EXPECT_TRUE(diagnostic.line().has_value());
    EXPECT_TRUE(diagnostic.column().has_value());
}

TEST(JsonDocumentReaderTest, RejectsDuplicateKeyInNestedObject) {
    const auto result = read(R"({"nested":{"id":1,"id":2}})");
    const auto& diagnostic =
        expect_failure(result, JsonDiagnosticCode::duplicate_object_key);
    EXPECT_EQ(diagnostic.duplicate_key(), "id");
}

TEST(JsonDocumentReaderTest, RejectsDuplicateKeyInObjectInsideArray) {
    const auto result = read(R"([{"id":1,"id":2}])");
    const auto& diagnostic =
        expect_failure(result, JsonDiagnosticCode::duplicate_object_key);
    EXPECT_EQ(diagnostic.duplicate_key(), "id");
}

TEST(JsonDocumentReaderTest, AllowsSameKeyInSiblingObjects) {
    expect_success(read(R"([{"id":1},{"id":2}])"));
}

TEST(JsonDocumentReaderTest, TreatsEscapedAndLiteralEquivalentKeysAsDuplicates) {
    const auto result = read(R"({"id":1,"\u0069d":2})");
    const auto& diagnostic =
        expect_failure(result, JsonDiagnosticCode::duplicate_object_key);
    EXPECT_EQ(diagnostic.duplicate_key(), "id");
}

TEST(JsonDocumentReaderTest, RejectsInvalidUtf8ContinuationByte) {
    const std::string document = {'"', static_cast<char>(0x80), '"'};
    const auto result = read_bytes(document);
    expect_failure(result, JsonDiagnosticCode::invalid_json);
}

TEST(JsonDocumentReaderTest, RejectsTruncatedUtf8Sequence) {
    const std::string document = {'"', static_cast<char>(0xE2), static_cast<char>(0x82)};
    const auto result = read_bytes(document);
    expect_failure(result, JsonDiagnosticCode::invalid_json);
}

TEST(JsonDocumentReaderTest, RejectsUnpairedHighSurrogate) {
    const auto result = read(R"("\uD800")");
    expect_failure(result, JsonDiagnosticCode::invalid_json);
}

TEST(JsonDocumentReaderTest, RejectsUnpairedLowSurrogate) {
    const auto result = read(R"("\uDC00")");
    expect_failure(result, JsonDiagnosticCode::invalid_json);
}

TEST(JsonDocumentReaderTest, RejectsHighSurrogateFollowedByNonLowSurrogate) {
    const auto result = read(R"("\uD800\u0041")");
    expect_failure(result, JsonDiagnosticCode::invalid_json);
}

TEST(JsonDocumentReaderTest, AllowsEscapedTextThatLooksLikeLowSurrogate) {
    expect_success(read(R"("\\uDC00")"));
}

TEST(JsonDocumentReaderTest, AcceptsValidSurrogatePair) {
    expect_success(read(R"("\uD83C\uDF0D")"));
}

TEST(JsonDocumentReaderTest, AcceptsValidUtf8UnicodeInString) {
    const std::string document =
        {'"', 'c', 'a', 'f', static_cast<char>(0xC3), static_cast<char>(0xA9), ' ',
         static_cast<char>(0xF0), static_cast<char>(0x9F), static_cast<char>(0x8C),
         static_cast<char>(0x8D), '"'};
    expect_success(read_bytes(document));
}

TEST(JsonDocumentReaderTest, SuccessContainsOnlyDocument) {
    const auto result = read("{}");
    EXPECT_TRUE(result.succeeded());
    EXPECT_NE(result.document(), nullptr);
    EXPECT_EQ(result.diagnostic(), nullptr);
}

TEST(JsonDocumentReaderTest, FailureContainsOnlyDiagnostic) {
    const auto result = read("{");
    EXPECT_FALSE(result.succeeded());
    EXPECT_EQ(result.document(), nullptr);
    EXPECT_NE(result.diagnostic(), nullptr);
}

TEST(JsonDocumentReaderTest, EnforcesCallerProvidedMaximumSize) {
    const auto result = read_json_document("{}", JsonReaderOptions{1U});
    const auto& diagnostic =
        expect_failure(result, JsonDiagnosticCode::document_too_large);
    EXPECT_FALSE(diagnostic.line().has_value());
    EXPECT_FALSE(diagnostic.column().has_value());
}

TEST(JsonDocumentReaderTest, AcceptsDocumentAtCallerProvidedMaximumSize) {
    expect_success(read_json_document("{}", JsonReaderOptions{2U}));
}

TEST(JsonDocumentReaderTest, PublicHeaderDoesNotExposeJsoncons) {
    const auto header = read_fixture("include/corerpg/content_json/json_document_reader.hpp");
    EXPECT_EQ(header.find("jsoncons"), std::string::npos);
}

}  // namespace
