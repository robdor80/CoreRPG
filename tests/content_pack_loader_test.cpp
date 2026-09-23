#include <algorithm>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "corerpg/content_catalog.hpp"
#include "corerpg/content_diagnostic.hpp"
#include "corerpg/content_json/content_pack_loader.hpp"

namespace {

namespace fs = std::filesystem;

using corerpg::ContentCatalog;
using corerpg::ContentDiagnosticCode;
using corerpg::ContentPackInput;
using corerpg::content_json::ContentPackLoadDiagnosticCode;
using corerpg::content_json::ContentPackLoadOptions;
using corerpg::content_json::ContentPackLoadResult;
using corerpg::content_json::ContentPackDocumentDiagnostic;
using corerpg::content_json::JsonDiagnosticCode;
using corerpg::content_json::JsonMappingDiagnostic;
using corerpg::content_json::JsonMappingDiagnosticCode;
using corerpg::content_json::load_content_pack;

class TemporaryDirectory final {
public:
    TemporaryDirectory() {
        static std::atomic<unsigned long long> sequence{0U};
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        path_ = fs::temp_directory_path() /
                ("corerpg-content-pack-loader-" + std::to_string(stamp) + "-" +
                 std::to_string(sequence.fetch_add(1U)));
        if (!fs::create_directory(path_)) {
            throw std::runtime_error("Cannot create temporary test directory");
        }
    }

    ~TemporaryDirectory() {
        std::error_code error;
        fs::remove_all(path_, error);
    }

    TemporaryDirectory(const TemporaryDirectory&) = delete;
    TemporaryDirectory& operator=(const TemporaryDirectory&) = delete;

    [[nodiscard]] const fs::path& path() const noexcept { return path_; }

private:
    fs::path path_;
};

void write_file(const fs::path& path, std::string_view contents) {
    if (!path.parent_path().empty()) {
        fs::create_directories(path.parent_path());
    }
    std::ofstream output(path, std::ios::binary);
    if (!output.is_open()) {
        throw std::runtime_error("Cannot create test file: " + path.string());
    }
    output.write(contents.data(), static_cast<std::streamsize>(contents.size()));
    if (!output) {
        throw std::runtime_error("Cannot write test file: " + path.string());
    }
}

std::string manifest(std::vector<std::string> resources = {},
                     std::string_view pack_id = "example.test",
                     std::string_view pack_version = "1.0.0") {
    std::string resource_array;
    for (const auto& resource : resources) {
        if (!resource_array.empty()) {
            resource_array += ',';
        }
        resource_array += '"' + resource + '"';
    }
    return "{\"pack_id\":\"" + std::string(pack_id) +
           "\",\"pack_version\":\"" + std::string(pack_version) +
           "\",\"content_contract_version\":\"0.1.0\","
           "\"minimum_core_version\":\"0.1.0\",\"dependencies\":[],"
           "\"resources\":[" +
           resource_array + "],\"display_name\":\"\",\"description\":\"\"}";
}

std::string resource(std::string_view id = "example.test:things/a",
                     std::string_view kind = "thing") {
    return "{\"id\":\"" + std::string(id) + "\",\"kind\":\"" +
           std::string(kind) + "\",\"references\":[]}";
}

const corerpg::ContentDefinitionInput* find_definition(const ContentPackInput& pack,
                                                       std::string_view id) {
    const auto found = std::ranges::find_if(pack.definitions(), [&](const auto& input) {
        return input.definition().id().value() == id;
    });
    return found == pack.definitions().end() ? nullptr : &*found;
}

TEST(ContentPackLoaderTest, LoadsExamplePackExactly) {
    const auto root = fs::path(CORERPG_SOURCE_DIR) / "content-packs/example.test";
    const auto result = load_content_pack(root);

    ASSERT_TRUE(result.succeeded());
    ASSERT_NE(result.pack(), nullptr);
    EXPECT_EQ(result.load_diagnostic(), nullptr);
    EXPECT_EQ(result.document_diagnostic(), nullptr);
    const auto& pack = *result.pack();
    EXPECT_EQ(pack.manifest().pack_id().value(), "example.test");
    EXPECT_EQ(pack.manifest().pack_version().value(), "1.0.0");
    ASSERT_EQ(pack.definitions().size(), 2U);

    const auto* definition_a = find_definition(pack, "example.test:things/a");
    ASSERT_NE(definition_a, nullptr);
    EXPECT_EQ(definition_a->definition().kind().value(), "thing");
    EXPECT_TRUE(definition_a->references().empty());

    const auto* definition_b = find_definition(pack, "example.test:things/b");
    ASSERT_NE(definition_b, nullptr);
    EXPECT_EQ(definition_b->definition().kind().value(), "thing");
    ASSERT_EQ(definition_b->references().size(), 1U);
    EXPECT_EQ(definition_b->references()[0].target().value(),
              "example.test:things/a");
    EXPECT_EQ(definition_b->references()[0].expected_kind().value(), "thing");
}

TEST(ContentPackLoaderFilesystemTest, RejectsMissingPackRoot) {
    TemporaryDirectory temporary;
    const auto missing = temporary.path() / "missing";
    const auto result = load_content_pack(missing);

    ASSERT_FALSE(result.succeeded());
    ASSERT_NE(result.load_diagnostic(), nullptr);
    EXPECT_EQ(result.load_diagnostic()->code(),
              ContentPackLoadDiagnosticCode::pack_root_not_found);
    EXPECT_EQ(result.load_diagnostic()->path(), missing);
}

TEST(ContentPackLoaderFilesystemTest, RejectsPackRootThatIsAFile) {
    TemporaryDirectory temporary;
    const auto file = temporary.path() / "pack";
    write_file(file, "not a directory");
    const auto result = load_content_pack(file);

    ASSERT_NE(result.load_diagnostic(), nullptr);
    EXPECT_EQ(result.load_diagnostic()->code(),
              ContentPackLoadDiagnosticCode::pack_root_not_directory);
}

TEST(ContentPackLoaderFilesystemTest, RejectsMissingManifest) {
    TemporaryDirectory temporary;
    const auto result = load_content_pack(temporary.path());

    ASSERT_NE(result.load_diagnostic(), nullptr);
    EXPECT_EQ(result.load_diagnostic()->code(),
              ContentPackLoadDiagnosticCode::manifest_not_found);
}

TEST(ContentPackLoaderFilesystemTest, RejectsManifestThatIsADirectory) {
    TemporaryDirectory temporary;
    fs::create_directory(temporary.path() / "manifest.json");
    const auto result = load_content_pack(temporary.path());

    ASSERT_NE(result.load_diagnostic(), nullptr);
    EXPECT_EQ(result.load_diagnostic()->code(),
              ContentPackLoadDiagnosticCode::manifest_not_regular_file);
}

TEST(ContentPackLoaderFilesystemTest, ReportsUnreadableManifestWhenPlatformEnforcesIt) {
    TemporaryDirectory temporary;
    const auto manifest_path = temporary.path() / "manifest.json";
    write_file(manifest_path, manifest());
    std::error_code error;
    const auto original_permissions = fs::status(manifest_path, error).permissions();
    ASSERT_FALSE(error);
    fs::permissions(manifest_path, fs::perms::none, fs::perm_options::replace, error);
    if (error) {
        GTEST_SKIP() << "Platform cannot change file permissions";
    }

    std::ifstream probe(manifest_path, std::ios::binary);
    if (probe.is_open()) {
        probe.close();
        fs::permissions(manifest_path, original_permissions, fs::perm_options::replace,
                        error);
        GTEST_SKIP() << "Platform/user can still read a permissionless file";
    }

    const auto result = load_content_pack(temporary.path());
    fs::permissions(manifest_path, original_permissions, fs::perm_options::replace, error);
    ASSERT_FALSE(error);
    ASSERT_NE(result.load_diagnostic(), nullptr);
    EXPECT_EQ(result.load_diagnostic()->code(),
              ContentPackLoadDiagnosticCode::file_read_error);
}

TEST(ContentPackLoaderManifestTest, PreservesStrictJsonFailure) {
    TemporaryDirectory temporary;
    write_file(temporary.path() / "manifest.json", "{");
    const auto result = load_content_pack(temporary.path());

    ASSERT_NE(result.document_diagnostic(), nullptr);
    ASSERT_NE(result.document_diagnostic()->json_diagnostic(), nullptr);
    EXPECT_EQ(result.document_diagnostic()->json_diagnostic()->code(),
              JsonDiagnosticCode::invalid_json);
    EXPECT_FALSE(result.document_diagnostic()->resource_locator().has_value());
}

TEST(ContentPackLoaderManifestTest, PreservesSchemaFailure) {
    TemporaryDirectory temporary;
    write_file(temporary.path() / "manifest.json", "{}");
    const auto result = load_content_pack(temporary.path());

    ASSERT_NE(result.document_diagnostic(), nullptr);
    ASSERT_NE(result.document_diagnostic()->schema_diagnostics(), nullptr);
    EXPECT_FALSE(result.document_diagnostic()->schema_diagnostics()->empty());
    EXPECT_EQ(result.document_diagnostic()->mapping_diagnostic(), nullptr);
}

TEST(ContentPackLoaderManifestTest, PreservesMappingFailure) {
    TemporaryDirectory temporary;
    write_file(temporary.path() / "manifest.json",
               manifest({}, "example.test", "4294967296.0.0"));
    const auto result = load_content_pack(temporary.path());

    ASSERT_NE(result.document_diagnostic(), nullptr);
    ASSERT_NE(result.document_diagnostic()->mapping_diagnostic(), nullptr);
    EXPECT_EQ(result.document_diagnostic()->mapping_diagnostic()->code(),
              JsonMappingDiagnosticCode::invalid_semantic_version);
    EXPECT_EQ(result.document_diagnostic()->mapping_diagnostic()->instance_location(),
              "/pack_version");
}

TEST(ContentPackLoaderManifestTest, EnforcesManifestSizeLimitWithoutUnboundedRead) {
    TemporaryDirectory temporary;
    write_file(temporary.path() / "manifest.json", manifest());
    const auto result = load_content_pack(
        temporary.path(), ContentPackLoadOptions{8U, 64U * 1024U * 1024U});

    ASSERT_NE(result.document_diagnostic(), nullptr);
    ASSERT_NE(result.document_diagnostic()->json_diagnostic(), nullptr);
    EXPECT_EQ(result.document_diagnostic()->json_diagnostic()->code(),
              JsonDiagnosticCode::document_too_large);
}

TEST(ContentPackLoaderResourceTest, RejectsMissingDeclaredResource) {
    TemporaryDirectory temporary;
    write_file(temporary.path() / "manifest.json",
               manifest({"resources/things/a.json"}));
    const auto result = load_content_pack(temporary.path());

    ASSERT_NE(result.load_diagnostic(), nullptr);
    EXPECT_EQ(result.load_diagnostic()->code(),
              ContentPackLoadDiagnosticCode::resource_not_found);
    ASSERT_TRUE(result.load_diagnostic()->resource_locator().has_value());
    EXPECT_EQ(*result.load_diagnostic()->resource_locator(),
              "resources/things/a.json");
}

TEST(ContentPackLoaderResourceTest, RejectsDeclaredResourceThatIsADirectory) {
    TemporaryDirectory temporary;
    write_file(temporary.path() / "manifest.json",
               manifest({"resources/things/a.json"}));
    fs::create_directories(temporary.path() / "resources/things/a.json");
    const auto result = load_content_pack(temporary.path());

    ASSERT_NE(result.load_diagnostic(), nullptr);
    EXPECT_EQ(result.load_diagnostic()->code(),
              ContentPackLoadDiagnosticCode::resource_not_regular_file);
}

TEST(ContentPackLoaderResourceTest, PreservesStrictJsonFailureAndLocator) {
    TemporaryDirectory temporary;
    write_file(temporary.path() / "manifest.json",
               manifest({"resources/things/a.json"}));
    write_file(temporary.path() / "resources/things/a.json", "{");
    const auto result = load_content_pack(temporary.path());

    ASSERT_NE(result.document_diagnostic(), nullptr);
    ASSERT_NE(result.document_diagnostic()->json_diagnostic(), nullptr);
    EXPECT_EQ(result.document_diagnostic()->json_diagnostic()->code(),
              JsonDiagnosticCode::invalid_json);
    ASSERT_TRUE(result.document_diagnostic()->resource_locator().has_value());
    EXPECT_EQ(*result.document_diagnostic()->resource_locator(),
              "resources/things/a.json");
}

TEST(ContentPackLoaderResourceTest, PreservesSchemaFailure) {
    TemporaryDirectory temporary;
    write_file(temporary.path() / "manifest.json",
               manifest({"resources/things/a.json"}));
    write_file(temporary.path() / "resources/things/a.json",
               R"({"id":"example.test:things/a","kind":"thing"})");
    const auto result = load_content_pack(temporary.path());

    ASSERT_NE(result.document_diagnostic(), nullptr);
    ASSERT_NE(result.document_diagnostic()->schema_diagnostics(), nullptr);
    EXPECT_FALSE(result.document_diagnostic()->schema_diagnostics()->empty());
}

TEST(ContentPackLoaderResourceTest, DocumentFailurePreservesMappingDiagnosticAndLocator) {
    ContentPackDocumentDiagnostic diagnostic(
        fs::path("resources/things/a.json"),
        std::optional<std::string>("resources/things/a.json"),
        JsonMappingDiagnostic(JsonMappingDiagnosticCode::invalid_definition_id, "/id",
                              "Value cannot be converted to DefinitionId"));

    ASSERT_NE(diagnostic.mapping_diagnostic(), nullptr);
    EXPECT_EQ(diagnostic.mapping_diagnostic()->code(),
              JsonMappingDiagnosticCode::invalid_definition_id);
    EXPECT_EQ(diagnostic.mapping_diagnostic()->instance_location(), "/id");
    EXPECT_EQ(diagnostic.resource_locator(),
              std::optional<std::string>("resources/things/a.json"));
    EXPECT_EQ(diagnostic.json_diagnostic(), nullptr);
    EXPECT_EQ(diagnostic.schema_diagnostics(), nullptr);
}

TEST(ContentPackLoaderResourceTest, EnforcesResourceSizeLimit) {
    TemporaryDirectory temporary;
    write_file(temporary.path() / "manifest.json",
               manifest({"resources/things/a.json"}));
    write_file(temporary.path() / "resources/things/a.json", resource());
    const auto result = load_content_pack(
        temporary.path(), ContentPackLoadOptions{16U * 1024U * 1024U, 8U});

    ASSERT_NE(result.document_diagnostic(), nullptr);
    ASSERT_NE(result.document_diagnostic()->json_diagnostic(), nullptr);
    EXPECT_EQ(result.document_diagnostic()->json_diagnostic()->code(),
              JsonDiagnosticCode::document_too_large);
}

TEST(ContentPackLoaderAtomicityTest, SecondResourceFailureReturnsNoPartialPack) {
    TemporaryDirectory temporary;
    write_file(temporary.path() / "manifest.json",
               manifest({"resources/things/a.json", "resources/things/b.json"}));
    write_file(temporary.path() / "resources/things/a.json", resource());
    write_file(temporary.path() / "resources/things/b.json", "{");
    const auto result = load_content_pack(temporary.path());

    EXPECT_FALSE(result.succeeded());
    EXPECT_EQ(result.pack(), nullptr);
    ASSERT_NE(result.document_diagnostic(), nullptr);
    EXPECT_EQ(result.document_diagnostic()->resource_locator(),
              std::optional<std::string>("resources/things/b.json"));
}

TEST(ContentPackLoaderIndexTest, IgnoresUnlistedJsonAndLoadsExactlyDeclaredFiles) {
    TemporaryDirectory temporary;
    write_file(temporary.path() / "manifest.json",
               manifest({"resources/things/a.json"}));
    write_file(temporary.path() / "resources/things/a.json", resource());
    write_file(temporary.path() / "resources/things/unlisted.json", "not json");
    const auto result = load_content_pack(temporary.path());

    ASSERT_TRUE(result.succeeded());
    ASSERT_NE(result.pack(), nullptr);
    ASSERT_EQ(result.pack()->definitions().size(), 1U);
    EXPECT_EQ(result.pack()->definitions()[0].definition().id().value(),
              "example.test:things/a");
}

TEST(ContentPackLoaderSchemaBoundaryTest, DuplicateLocatorFailsAsSchemaError) {
    TemporaryDirectory temporary;
    write_file(temporary.path() / "manifest.json",
               manifest({"resources/things/a.json", "resources/things/a.json"}));
    write_file(temporary.path() / "resources/things/a.json", resource());
    const auto result = load_content_pack(temporary.path());

    ASSERT_NE(result.document_diagnostic(), nullptr);
    ASSERT_NE(result.document_diagnostic()->schema_diagnostics(), nullptr);
    EXPECT_EQ(result.load_diagnostic(), nullptr);
}

TEST(ContentPackLoaderSymlinkTest, RejectsResourceSymlinkEscapingPackRootWhenSupported) {
    TemporaryDirectory temporary;
    const auto pack_root = temporary.path() / "pack";
    fs::create_directory(pack_root);
    write_file(pack_root / "manifest.json", manifest({"resources/things/a.json"}));
    const auto outside = temporary.path() / "outside.json";
    write_file(outside, resource());
    fs::create_directories(pack_root / "resources/things");
    std::error_code error;
    fs::create_symlink(outside, pack_root / "resources/things/a.json", error);
    if (error) {
        GTEST_SKIP() << "File symlink creation is unavailable: " << error.message();
    }

    const auto result = load_content_pack(pack_root);
    ASSERT_NE(result.load_diagnostic(), nullptr);
    EXPECT_EQ(result.load_diagnostic()->code(),
              ContentPackLoadDiagnosticCode::resource_path_escape);
}

TEST(ContentPackLoaderSymlinkTest, RejectsManifestSymlinkEscapingPackRootWhenSupported) {
    TemporaryDirectory temporary;
    const auto pack_root = temporary.path() / "pack";
    fs::create_directory(pack_root);
    const auto outside = temporary.path() / "manifest.json";
    write_file(outside, manifest());
    std::error_code error;
    fs::create_symlink(outside, pack_root / "manifest.json", error);
    if (error) {
        GTEST_SKIP() << "File symlink creation is unavailable: " << error.message();
    }

    const auto result = load_content_pack(pack_root);
    ASSERT_NE(result.load_diagnostic(), nullptr);
    EXPECT_EQ(result.load_diagnostic()->code(),
              ContentPackLoadDiagnosticCode::manifest_path_escape);
}

TEST(ContentPackLoaderSymlinkTest, AllowsPackRootSymlinkWhenSupported) {
    TemporaryDirectory temporary;
    const auto target = temporary.path() / "target";
    fs::create_directory(target);
    write_file(target / "manifest.json", manifest());
    const auto link = temporary.path() / "pack-link";
    std::error_code error;
    fs::create_directory_symlink(target, link, error);
    if (error) {
        GTEST_SKIP() << "Directory symlink creation is unavailable: " << error.message();
    }

    const auto result = load_content_pack(link);
    EXPECT_TRUE(result.succeeded());
}

TEST(ContentPackLoaderCoreBoundaryTest, LeavesDefinitionOwnershipValidationToCore) {
    TemporaryDirectory temporary;
    write_file(temporary.path() / "manifest.json",
               manifest({"resources/things/a.json"}, "example.one"));
    write_file(temporary.path() / "resources/things/a.json",
               resource("example.two:things/a"));
    const auto load_result = load_content_pack(temporary.path());

    ASSERT_TRUE(load_result.succeeded());
    ASSERT_NE(load_result.pack(), nullptr);
    const auto catalog_result = ContentCatalog::create({*load_result.pack()});
    ASSERT_FALSE(catalog_result.succeeded());
    ASSERT_NE(catalog_result.diagnostics(), nullptr);
    ASSERT_EQ(catalog_result.diagnostics()->size(), 1U);
    EXPECT_EQ(catalog_result.diagnostics()->front().code(),
              ContentDiagnosticCode::definition_pack_mismatch);
}

TEST(ContentPackLoaderTest, PublicHeaderDoesNotExposeJsoncons) {
    const auto header = fs::path(CORERPG_SOURCE_DIR) /
                        "include/corerpg/content_json/content_pack_loader.hpp";
    std::ifstream input(header, std::ios::binary);
    ASSERT_TRUE(input.is_open());
    const std::string contents{std::istreambuf_iterator<char>(input),
                               std::istreambuf_iterator<char>()};
    EXPECT_EQ(contents.find("jsoncons"), std::string::npos);
}

}  // namespace
