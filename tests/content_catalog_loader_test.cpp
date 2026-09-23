#include <array>
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

#include "corerpg/content_diagnostic.hpp"
#include "corerpg/content_json/content_catalog_loader.hpp"
#include "corerpg/definition_id.hpp"

namespace {

namespace fs = std::filesystem;

using corerpg::ContentDiagnosticCode;
using corerpg::DefinitionId;
using corerpg::content_json::ContentPackLoadDiagnosticCode;
using corerpg::content_json::JsonDiagnosticCode;
using corerpg::content_json::load_content_catalog;
using corerpg::content_json::load_content_pack;

class TemporaryDirectory final {
public:
    TemporaryDirectory() {
        static std::atomic<unsigned long long> sequence{0U};
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        path_ = fs::temp_directory_path() /
                ("corerpg-content-catalog-loader-" + std::to_string(stamp) + "-" +
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

std::string manifest(std::string_view pack_id, std::vector<std::string> resources,
                     std::optional<std::string_view> dependency = std::nullopt,
                     std::string_view minimum = "1.0.0",
                     std::string_view maximum = "2.0.0") {
    std::string dependencies = "[]";
    if (dependency.has_value()) {
        dependencies =
            "[{\"pack_id\":\"" + std::string(*dependency) +
            "\",\"version_requirement\":{\"minimum_inclusive\":\"" +
            std::string(minimum) + "\",\"maximum_exclusive\":\"" +
            std::string(maximum) + "\"}}]";
    }
    std::string resource_array;
    for (const auto& locator : resources) {
        if (!resource_array.empty()) {
            resource_array += ',';
        }
        resource_array += '"' + locator + '"';
    }
    return "{\"pack_id\":\"" + std::string(pack_id) +
           "\",\"pack_version\":\"1.0.0\","
           "\"content_contract_version\":\"0.1.0\","
           "\"minimum_core_version\":\"0.1.0\",\"dependencies\":" +
           dependencies + ",\"resources\":[" + resource_array +
           "],\"display_name\":\"\",\"description\":\"\"}";
}

std::string resource(std::string_view id,
                     std::optional<std::string_view> target = std::nullopt) {
    std::string references = "[]";
    if (target.has_value()) {
        references = "[{\"target\":\"" + std::string(*target) +
                     "\",\"expected_kind\":\"thing\"}]";
    }
    return "{\"id\":\"" + std::string(id) +
           "\",\"kind\":\"thing\",\"references\":" + references + "}";
}

DefinitionId id(std::string_view value) {
    return *DefinitionId::parse(value);
}

fs::path fixture_root(std::string_view name) {
    return fs::path(CORERPG_SOURCE_DIR) / "content-packs" / name;
}

TEST(ContentCatalogLoaderFixtureTest, ExampleAddonLoadsExactly) {
    const auto result = load_content_pack(fixture_root("example.addon"));

    ASSERT_TRUE(result.succeeded());
    ASSERT_NE(result.pack(), nullptr);
    const auto& pack = *result.pack();
    EXPECT_EQ(pack.manifest().pack_id().value(), "example.addon");
    EXPECT_EQ(pack.manifest().pack_version().value(), "1.0.0");
    EXPECT_EQ(pack.manifest().content_contract_version().value(), "0.1.0");
    EXPECT_EQ(pack.manifest().minimum_core_version().value(), "0.1.0");
    EXPECT_EQ(pack.manifest().display_name(), "Example Addon");
    EXPECT_EQ(pack.manifest().description(),
              "Neutral dependent content pack used to validate multi-pack catalog loading.");
    ASSERT_EQ(pack.manifest().dependencies().size(), 1U);
    const auto& dependency = pack.manifest().dependencies()[0];
    EXPECT_EQ(dependency.pack_id().value(), "example.test");
    EXPECT_EQ(dependency.requirement().minimum_inclusive().value(), "1.0.0");
    ASSERT_TRUE(dependency.requirement().maximum_exclusive().has_value());
    EXPECT_EQ(dependency.requirement().maximum_exclusive()->value(), "2.0.0");

    ASSERT_EQ(pack.definitions().size(), 1U);
    const auto& definition = pack.definitions()[0];
    EXPECT_EQ(definition.definition().id().value(), "example.addon:things/c");
    EXPECT_EQ(definition.definition().kind().value(), "thing");
    ASSERT_EQ(definition.references().size(), 1U);
    EXPECT_EQ(definition.references()[0].target().value(), "example.test:things/a");
    EXPECT_EQ(definition.references()[0].expected_kind().value(), "thing");
}

TEST(ContentCatalogLoaderTest, LoadsExplicitBaseAndAddonIntoValidatedCatalog) {
    const std::array roots{fixture_root("example.test"), fixture_root("example.addon")};
    const auto result = load_content_catalog(roots);

    ASSERT_TRUE(result.succeeded());
    ASSERT_NE(result.catalog(), nullptr);
    EXPECT_EQ(result.physical_failure(), nullptr);
    EXPECT_EQ(result.semantic_diagnostics(), nullptr);
    EXPECT_EQ(result.catalog()->size(), 3U);
    EXPECT_TRUE(result.catalog()->contains(id("example.test:things/a")));
    EXPECT_TRUE(result.catalog()->contains(id("example.test:things/b")));
    EXPECT_TRUE(result.catalog()->contains(id("example.addon:things/c")));
    ASSERT_NE(result.catalog()->find(id("example.addon:things/c")), nullptr);
    EXPECT_EQ(result.catalog()->find(id("example.addon:things/c"))->kind().value(),
              "thing");
}

TEST(ContentCatalogLoaderTest, ReversingRootsProducesEquivalentObservableCatalog) {
    const std::array forward_roots{fixture_root("example.test"),
                                   fixture_root("example.addon")};
    const std::array reverse_roots{fixture_root("example.addon"),
                                   fixture_root("example.test")};
    const auto forward = load_content_catalog(forward_roots);
    const auto reverse = load_content_catalog(reverse_roots);

    ASSERT_TRUE(forward.succeeded());
    ASSERT_TRUE(reverse.succeeded());
    ASSERT_EQ(forward.catalog()->size(), reverse.catalog()->size());
    ASSERT_EQ(forward.catalog()->definitions().size(),
              reverse.catalog()->definitions().size());
    for (std::size_t index = 0; index < forward.catalog()->definitions().size(); ++index) {
        EXPECT_EQ(forward.catalog()->definitions()[index].id(),
                  reverse.catalog()->definitions()[index].id());
        EXPECT_EQ(forward.catalog()->definitions()[index].kind(),
                  reverse.catalog()->definitions()[index].kind());
    }
}

TEST(ContentCatalogLoaderTest, LoadsExampleTestAlone) {
    const std::array roots{fixture_root("example.test")};
    const auto result = load_content_catalog(roots);

    ASSERT_TRUE(result.succeeded());
    ASSERT_NE(result.catalog(), nullptr);
    EXPECT_EQ(result.catalog()->size(), 2U);
    EXPECT_TRUE(result.catalog()->contains(id("example.test:things/a")));
    EXPECT_TRUE(result.catalog()->contains(id("example.test:things/b")));
}

TEST(ContentCatalogLoaderTest, DoesNotAutoLoadSiblingDependency) {
    const std::array roots{fixture_root("example.addon")};
    const auto result = load_content_catalog(roots);

    ASSERT_FALSE(result.succeeded());
    EXPECT_EQ(result.catalog(), nullptr);
    EXPECT_EQ(result.physical_failure(), nullptr);
    ASSERT_NE(result.semantic_diagnostics(), nullptr);
    ASSERT_EQ(result.semantic_diagnostics()->size(), 1U);
    const auto& diagnostic = result.semantic_diagnostics()->front();
    EXPECT_EQ(diagnostic.code(), ContentDiagnosticCode::dependency_not_found);
    EXPECT_EQ(diagnostic.pack_id().value(), "example.addon");
    ASSERT_TRUE(diagnostic.related_pack_id().has_value());
    EXPECT_EQ(diagnostic.related_pack_id()->value(), "example.test");
}

TEST(ContentCatalogLoaderTest, DuplicateRootReachesCoreAsDuplicatePack) {
    const auto root = fixture_root("example.test");
    const std::array roots{root, root};
    const auto result = load_content_catalog(roots);

    ASSERT_NE(result.semantic_diagnostics(), nullptr);
    ASSERT_EQ(result.semantic_diagnostics()->size(), 1U);
    EXPECT_EQ(result.semantic_diagnostics()->front().code(),
              ContentDiagnosticCode::duplicate_pack);
    EXPECT_EQ(result.physical_failure(), nullptr);
}

TEST(ContentCatalogLoaderTest, CoreReportsDependencyVersionMismatch) {
    TemporaryDirectory temporary;
    write_file(temporary.path() / "manifest.json",
               manifest("example.incompatible", {}, "example.test", "2.0.0",
                        "3.0.0"));
    const std::array roots{fixture_root("example.test"), temporary.path()};
    const auto result = load_content_catalog(roots);

    ASSERT_NE(result.semantic_diagnostics(), nullptr);
    ASSERT_EQ(result.semantic_diagnostics()->size(), 1U);
    const auto& diagnostic = result.semantic_diagnostics()->front();
    EXPECT_EQ(diagnostic.code(), ContentDiagnosticCode::dependency_version_mismatch);
    EXPECT_EQ(diagnostic.pack_id().value(), "example.incompatible");
    ASSERT_TRUE(diagnostic.related_pack_id().has_value());
    EXPECT_EQ(diagnostic.related_pack_id()->value(), "example.test");
}

TEST(ContentCatalogLoaderTest, CoreRejectsCrossPackReferenceWithoutDependency) {
    TemporaryDirectory temporary;
    write_file(temporary.path() / "manifest.json",
               manifest("example.unauthorized", {"resources/things/c.json"}));
    write_file(temporary.path() / "resources/things/c.json",
               resource("example.unauthorized:things/c", "example.test:things/a"));
    const std::array roots{fixture_root("example.test"), temporary.path()};
    const auto result = load_content_catalog(roots);

    ASSERT_NE(result.semantic_diagnostics(), nullptr);
    ASSERT_EQ(result.semantic_diagnostics()->size(), 1U);
    const auto& diagnostic = result.semantic_diagnostics()->front();
    EXPECT_EQ(diagnostic.code(),
              ContentDiagnosticCode::cross_pack_reference_not_declared);
    ASSERT_TRUE(diagnostic.definition_id().has_value());
    EXPECT_EQ(diagnostic.definition_id()->value(),
              "example.unauthorized:things/c");
    ASSERT_TRUE(diagnostic.related_definition_id().has_value());
    EXPECT_EQ(diagnostic.related_definition_id()->value(), "example.test:things/a");
}

TEST(ContentCatalogLoaderPhysicalFailureTest, PreservesFailingRootAndLoadDiagnostic) {
    TemporaryDirectory temporary;
    const auto missing = temporary.path() / "missing";
    const std::array roots{fixture_root("example.test"), missing};
    const auto result = load_content_catalog(roots);

    EXPECT_FALSE(result.succeeded());
    EXPECT_EQ(result.catalog(), nullptr);
    EXPECT_EQ(result.semantic_diagnostics(), nullptr);
    ASSERT_NE(result.physical_failure(), nullptr);
    EXPECT_EQ(result.physical_failure()->pack_root(), missing);
    ASSERT_NE(result.physical_failure()->load_result().load_diagnostic(), nullptr);
    EXPECT_EQ(result.physical_failure()->load_result().load_diagnostic()->code(),
              ContentPackLoadDiagnosticCode::pack_root_not_found);
}

TEST(ContentCatalogLoaderPhysicalFailureTest, PreservesDocumentDiagnosticWithoutPartialCatalog) {
    TemporaryDirectory temporary;
    write_file(temporary.path() / "manifest.json", "{");
    const std::array roots{fixture_root("example.test"), temporary.path()};
    const auto result = load_content_catalog(roots);

    EXPECT_FALSE(result.succeeded());
    EXPECT_EQ(result.catalog(), nullptr);
    EXPECT_EQ(result.semantic_diagnostics(), nullptr);
    ASSERT_NE(result.physical_failure(), nullptr);
    EXPECT_EQ(result.physical_failure()->pack_root(), temporary.path());
    ASSERT_NE(result.physical_failure()->load_result().document_diagnostic(), nullptr);
    ASSERT_NE(result.physical_failure()
                  ->load_result()
                  .document_diagnostic()
                  ->json_diagnostic(),
              nullptr);
    EXPECT_EQ(result.physical_failure()
                  ->load_result()
                  .document_diagnostic()
                  ->json_diagnostic()
                  ->code(),
              JsonDiagnosticCode::invalid_json);
}

TEST(ContentCatalogLoaderTest, EmptyExplicitRootSetProducesEmptyCatalog) {
    const std::array<fs::path, 0U> roots{};
    const auto result = load_content_catalog(roots);

    ASSERT_TRUE(result.succeeded());
    ASSERT_NE(result.catalog(), nullptr);
    EXPECT_TRUE(result.catalog()->empty());
}

TEST(ContentCatalogLoaderTest, PublicHeaderDoesNotExposeJsoncons) {
    const auto header = fs::path(CORERPG_SOURCE_DIR) /
                        "include/corerpg/content_json/content_catalog_loader.hpp";
    std::ifstream input(header, std::ios::binary);
    ASSERT_TRUE(input.is_open());
    const std::string contents{std::istreambuf_iterator<char>(input),
                               std::istreambuf_iterator<char>()};
    EXPECT_EQ(contents.find("jsoncons"), std::string::npos);
}

}  // namespace
