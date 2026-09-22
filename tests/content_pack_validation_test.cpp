#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "corerpg/content_catalog.hpp"

namespace {

using corerpg::CatalogBuildResult;
using corerpg::ContentDefinitionInput;
using corerpg::ContentDiagnosticCode;
using corerpg::ContentManifest;
using corerpg::ContentPackInput;
using corerpg::DefinitionReference;
using corerpg::PackDependency;
using corerpg::PackId;
using corerpg::ResourceDefinition;
using corerpg::ResourceKind;
using corerpg::SemanticVersion;
using corerpg::VersionRequirement;

PackId pack_id(std::string_view value) {
    return *PackId::parse(value);
}

SemanticVersion version(std::string_view value) {
    return *SemanticVersion::parse(value);
}

ResourceKind kind(std::string_view value) {
    return *ResourceKind::parse(value);
}

corerpg::DefinitionId definition_id(std::string_view value) {
    return *corerpg::DefinitionId::parse(value);
}

VersionRequirement requirement(std::string_view minimum,
                               std::optional<std::string_view> maximum = std::nullopt) {
    return *VersionRequirement::create(
        version(minimum), maximum.has_value()
                              ? std::optional<SemanticVersion>(version(*maximum))
                              : std::nullopt);
}

PackDependency dependency(std::string_view target, std::string_view minimum = "1.0.0",
                          std::optional<std::string_view> maximum = std::nullopt) {
    return PackDependency(pack_id(target), requirement(minimum, maximum));
}

DefinitionReference reference(std::string_view target, std::string_view expected_kind) {
    return DefinitionReference(definition_id(target), kind(expected_kind));
}

ContentDefinitionInput definition(
    std::string_view id, std::string_view resource_kind,
    std::vector<DefinitionReference> references = {}) {
    return ContentDefinitionInput(
        ResourceDefinition(definition_id(id), kind(resource_kind)), std::move(references));
}

ContentPackInput pack(std::string_view id, std::string_view pack_version = "1.0.0",
                      std::vector<PackDependency> dependencies = {},
                      std::vector<ContentDefinitionInput> definitions = {}) {
    return ContentPackInput(
        ContentManifest(pack_id(id), version(pack_version), version("0.1.0"),
                        version("0.1.0"), std::move(dependencies)),
        std::move(definitions));
}

void expect_same_diagnostics(const CatalogBuildResult& left, const CatalogBuildResult& right) {
    ASSERT_NE(left.diagnostics(), nullptr);
    ASSERT_NE(right.diagnostics(), nullptr);
    ASSERT_EQ(left.diagnostics()->size(), right.diagnostics()->size());
    for (std::size_t index = 0; index < left.diagnostics()->size(); ++index) {
        const auto& first = (*left.diagnostics())[index];
        const auto& second = (*right.diagnostics())[index];
        EXPECT_EQ(first.code(), second.code());
        EXPECT_EQ(first.pack_id(), second.pack_id());
        EXPECT_EQ(first.related_pack_id(), second.related_pack_id());
        EXPECT_EQ(first.definition_id(), second.definition_id());
        EXPECT_EQ(first.related_definition_id(), second.related_definition_id());
    }
}

TEST(ContentPackValidationTest, OnePackWithoutDependenciesIsValid) {
    const auto result = corerpg::ContentCatalog::create(
        {pack("example.one", "1.0.0", {},
              {definition("example.one:item", "item-kind")})});

    ASSERT_TRUE(result.succeeded());
    ASSERT_NE(result.catalog(), nullptr);
    EXPECT_TRUE(result.catalog()->contains(definition_id("example.one:item")));
}

TEST(ContentPackValidationTest, IndependentPacksAreValid) {
    const auto result = corerpg::ContentCatalog::create(
        {pack("example.one"), pack("example.two"), pack("example.three")});

    EXPECT_TRUE(result.succeeded());
}

TEST(ContentPackValidationTest, DuplicatePackIdIsRejected) {
    const auto result = corerpg::ContentCatalog::create(
        {pack("example.same"), pack("example.same")});

    ASSERT_FALSE(result.succeeded());
    ASSERT_NE(result.diagnostics(), nullptr);
    ASSERT_EQ(result.diagnostics()->size(), 1U);
    EXPECT_EQ(result.diagnostics()->front().code(), ContentDiagnosticCode::duplicate_pack);
    EXPECT_EQ(result.diagnostics()->front().pack_id().value(), "example.same");
}

TEST(ContentPackValidationTest, DuplicatePackIdWithDifferentVersionsIsRejected) {
    const auto result = corerpg::ContentCatalog::create(
        {pack("example.same", "1.0.0"), pack("example.same", "2.0.0")});

    ASSERT_FALSE(result.succeeded());
    ASSERT_NE(result.diagnostics(), nullptr);
    ASSERT_EQ(result.diagnostics()->size(), 1U);
    EXPECT_EQ(result.diagnostics()->front().code(), ContentDiagnosticCode::duplicate_pack);
}

TEST(ContentPackValidationTest, DefinitionMustBelongToItsManifestPack) {
    const auto result = corerpg::ContentCatalog::create(
        {pack("example.one", "1.0.0", {},
              {definition("example.two:item", "item-kind")})});

    ASSERT_FALSE(result.succeeded());
    ASSERT_NE(result.diagnostics(), nullptr);
    ASSERT_EQ(result.diagnostics()->size(), 1U);
    const auto& diagnostic = result.diagnostics()->front();
    EXPECT_EQ(diagnostic.code(), ContentDiagnosticCode::definition_pack_mismatch);
    EXPECT_EQ(diagnostic.pack_id().value(), "example.one");
    ASSERT_TRUE(diagnostic.definition_id().has_value());
    EXPECT_EQ(diagnostic.definition_id()->value(), "example.two:item");
}

TEST(ContentPackValidationTest, PresentCompatibleDependencyIsValid) {
    const auto result = corerpg::ContentCatalog::create(
        {pack("example.app", "1.0.0", {dependency("example.base", "1.0.0")}),
         pack("example.base", "1.5.0")});

    EXPECT_TRUE(result.succeeded());
}

TEST(ContentPackValidationTest, MissingDependencyIsRejected) {
    const auto result = corerpg::ContentCatalog::create(
        {pack("example.app", "1.0.0", {dependency("example.missing")})});

    ASSERT_FALSE(result.succeeded());
    ASSERT_NE(result.diagnostics(), nullptr);
    const auto& diagnostic = result.diagnostics()->front();
    EXPECT_EQ(diagnostic.code(), ContentDiagnosticCode::dependency_not_found);
    EXPECT_EQ(diagnostic.pack_id().value(), "example.app");
    ASSERT_TRUE(diagnostic.related_pack_id().has_value());
    EXPECT_EQ(diagnostic.related_pack_id()->value(), "example.missing");
}

TEST(ContentPackValidationTest, VersionBelowMinimumIsRejectedWithStructuredData) {
    const auto result = corerpg::ContentCatalog::create(
        {pack("example.app", "1.0.0", {dependency("example.base", "2.0.0")}),
         pack("example.base", "1.9.9")});

    ASSERT_FALSE(result.succeeded());
    ASSERT_NE(result.diagnostics(), nullptr);
    const auto& diagnostic = result.diagnostics()->front();
    EXPECT_EQ(diagnostic.code(), ContentDiagnosticCode::dependency_version_mismatch);
    ASSERT_TRUE(diagnostic.version_requirement().has_value());
    EXPECT_EQ(diagnostic.version_requirement()->minimum_inclusive(), version("2.0.0"));
    ASSERT_TRUE(diagnostic.actual_version().has_value());
    EXPECT_EQ(*diagnostic.actual_version(), version("1.9.9"));
}

TEST(ContentPackValidationTest, VersionEqualToMinimumIsValid) {
    const auto result = corerpg::ContentCatalog::create(
        {pack("example.app", "1.0.0", {dependency("example.base", "2.0.0")}),
         pack("example.base", "2.0.0")});

    EXPECT_TRUE(result.succeeded());
}

TEST(ContentPackValidationTest, VersionEqualToExclusiveMaximumIsRejected) {
    const auto result = corerpg::ContentCatalog::create(
        {pack("example.app", "1.0.0",
              {dependency("example.base", "1.0.0", "2.0.0")}),
         pack("example.base", "2.0.0")});

    ASSERT_FALSE(result.succeeded());
    ASSERT_NE(result.diagnostics(), nullptr);
    EXPECT_EQ(result.diagnostics()->front().code(),
              ContentDiagnosticCode::dependency_version_mismatch);
}

TEST(ContentPackValidationTest, VersionInsideBoundedRequirementIsValid) {
    const auto result = corerpg::ContentCatalog::create(
        {pack("example.app", "1.0.0",
              {dependency("example.base", "1.0.0", "2.0.0")}),
         pack("example.base", "1.5.0")});

    EXPECT_TRUE(result.succeeded());
}

TEST(ContentPackValidationTest, DuplicateDependencyIsRejectedWithoutMergingRanges) {
    const auto result = corerpg::ContentCatalog::create(
        {pack("example.app", "1.0.0",
              {dependency("example.base", "1.0.0"),
               dependency("example.base", "2.0.0")}),
         pack("example.base", "2.0.0")});

    ASSERT_FALSE(result.succeeded());
    ASSERT_NE(result.diagnostics(), nullptr);
    ASSERT_EQ(result.diagnostics()->size(), 1U);
    EXPECT_EQ(result.diagnostics()->front().code(),
              ContentDiagnosticCode::duplicate_dependency);
}

TEST(ContentPackValidationTest, SelfDependencyIsRejected) {
    const auto result = corerpg::ContentCatalog::create(
        {pack("example.self", "1.0.0", {dependency("example.self")})});

    ASSERT_FALSE(result.succeeded());
    ASSERT_NE(result.diagnostics(), nullptr);
    EXPECT_EQ(result.diagnostics()->front().code(), ContentDiagnosticCode::self_dependency);
}

TEST(ContentPackValidationTest, TwoPackDependencyCycleIsRejected) {
    const auto result = corerpg::ContentCatalog::create(
        {pack("example.a", "1.0.0", {dependency("example.b")}),
         pack("example.b", "1.0.0", {dependency("example.a")})});

    ASSERT_FALSE(result.succeeded());
    ASSERT_NE(result.diagnostics(), nullptr);
    ASSERT_EQ(result.diagnostics()->size(), 1U);
    EXPECT_EQ(result.diagnostics()->front().code(), ContentDiagnosticCode::dependency_cycle);
    EXPECT_EQ(result.diagnostics()->front().pack_id().value(), "example.b");
    EXPECT_EQ(result.diagnostics()->front().related_pack_id()->value(), "example.a");
}

TEST(ContentPackValidationTest, ThreePackDependencyCycleIsRejected) {
    const auto result = corerpg::ContentCatalog::create(
        {pack("example.a", "1.0.0", {dependency("example.b")}),
         pack("example.b", "1.0.0", {dependency("example.c")}),
         pack("example.c", "1.0.0", {dependency("example.a")})});

    ASSERT_FALSE(result.succeeded());
    ASSERT_NE(result.diagnostics(), nullptr);
    EXPECT_EQ(result.diagnostics()->front().code(), ContentDiagnosticCode::dependency_cycle);
}

TEST(ContentPackValidationTest, AcyclicDependencyGraphIsValid) {
    const auto result = corerpg::ContentCatalog::create(
        {pack("example.a", "1.0.0", {dependency("example.b")}),
         pack("example.b", "1.0.0", {dependency("example.c")}),
         pack("example.c")});

    EXPECT_TRUE(result.succeeded());
}

TEST(ContentPackValidationTest, SamePackReferenceNeedsNoDependency) {
    const auto result = corerpg::ContentCatalog::create(
        {pack("example.one", "1.0.0", {},
              {definition("example.one:source", "node",
                          {reference("example.one:target", "node")}),
               definition("example.one:target", "node")})});

    EXPECT_TRUE(result.succeeded());
}

TEST(ContentPackValidationTest, CrossPackReferenceWithoutDependencyIsRejected) {
    const auto result = corerpg::ContentCatalog::create(
        {pack("example.one", "1.0.0", {},
              {definition("example.one:source", "node",
                          {reference("example.two:target", "node")})}),
         pack("example.two", "1.0.0", {},
              {definition("example.two:target", "node")})});

    ASSERT_FALSE(result.succeeded());
    ASSERT_NE(result.diagnostics(), nullptr);
    const auto& diagnostic = result.diagnostics()->front();
    EXPECT_EQ(diagnostic.code(),
              ContentDiagnosticCode::cross_pack_reference_not_declared);
    EXPECT_EQ(diagnostic.definition_id()->value(), "example.one:source");
    EXPECT_EQ(diagnostic.related_definition_id()->value(), "example.two:target");
}

TEST(ContentPackValidationTest, TransitiveDependencyDoesNotAuthorizeReference) {
    const auto result = corerpg::ContentCatalog::create(
        {pack("example.a", "1.0.0", {dependency("example.b")},
              {definition("example.a:source", "node",
                          {reference("example.c:target", "node")})}),
         pack("example.b", "1.0.0", {dependency("example.c")}),
         pack("example.c", "1.0.0", {},
              {definition("example.c:target", "node")})});

    ASSERT_FALSE(result.succeeded());
    ASSERT_NE(result.diagnostics(), nullptr);
    EXPECT_EQ(result.diagnostics()->front().code(),
              ContentDiagnosticCode::cross_pack_reference_not_declared);
}

TEST(ContentPackValidationTest, ExplicitDirectDependencyAuthorizesReference) {
    const auto result = corerpg::ContentCatalog::create(
        {pack("example.a", "1.0.0",
              {dependency("example.b"), dependency("example.c")},
              {definition("example.a:source", "node",
                          {reference("example.c:target", "node")})}),
         pack("example.b", "1.0.0", {dependency("example.c")}),
         pack("example.c", "1.0.0", {},
              {definition("example.c:target", "node")})});

    EXPECT_TRUE(result.succeeded());
}

TEST(ContentPackValidationTest, PackOrderDoesNotAffectResult) {
    const auto forward = corerpg::ContentCatalog::create(
        {pack("example.a"), pack("example.b"), pack("example.c")});
    const auto reverse = corerpg::ContentCatalog::create(
        {pack("example.c"), pack("example.b"), pack("example.a")});

    ASSERT_TRUE(forward.succeeded());
    ASSERT_TRUE(reverse.succeeded());
    EXPECT_EQ(forward.catalog()->size(), reverse.catalog()->size());
}

TEST(ContentPackValidationTest, DependencyOrderHasNoPriorityOrLoadOrderMeaning) {
    const auto forward = corerpg::ContentCatalog::create(
        {pack("example.app", "1.0.0",
              {dependency("example.a"), dependency("example.b")}),
         pack("example.a"), pack("example.b")});
    const auto reverse = corerpg::ContentCatalog::create(
        {pack("example.app", "1.0.0",
              {dependency("example.b"), dependency("example.a")}),
         pack("example.b"), pack("example.a")});

    ASSERT_TRUE(forward.succeeded());
    ASSERT_TRUE(reverse.succeeded());
    EXPECT_EQ(forward.catalog()->size(), reverse.catalog()->size());
}

TEST(ContentPackValidationTest, DependencyDiagnosticsAreCanonicalAcrossPermutations) {
    const auto forward = corerpg::ContentCatalog::create(
        {pack("zeta.pack", "1.0.0", {dependency("zeta.missing")}),
         pack("alpha.pack", "1.0.0", {dependency("alpha.missing")})});
    const auto reverse = corerpg::ContentCatalog::create(
        {pack("alpha.pack", "1.0.0", {dependency("alpha.missing")}),
         pack("zeta.pack", "1.0.0", {dependency("zeta.missing")})});

    expect_same_diagnostics(forward, reverse);
    ASSERT_NE(forward.diagnostics(), nullptr);
    ASSERT_EQ(forward.diagnostics()->size(), 2U);
    EXPECT_EQ((*forward.diagnostics())[0].pack_id().value(), "alpha.pack");
    EXPECT_EQ((*forward.diagnostics())[1].pack_id().value(), "zeta.pack");
}

}  // namespace
