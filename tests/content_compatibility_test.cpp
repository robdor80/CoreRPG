#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "corerpg/content_catalog.hpp"
#include "corerpg/content_compatibility.hpp"

namespace {

using corerpg::ContentCompatibility;
using corerpg::ContentDefinitionInput;
using corerpg::ContentDiagnostic;
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

SemanticVersion version(std::string_view value) {
    return SemanticVersion::parse(value).value();
}

PackId pack_id(std::string_view value) {
    return PackId::parse(value).value();
}

corerpg::DefinitionId definition_id(std::string_view value) {
    return corerpg::DefinitionId::parse(value).value();
}

ResourceKind kind(std::string_view value) {
    return ResourceKind::parse(value).value();
}

ContentManifest manifest(
    std::string_view id, std::string_view contract_version = "0.1.0",
    std::string_view minimum_core_version = "0.1.0",
    std::string_view pack_version = "1.0.0",
    std::vector<PackDependency> dependencies = {}, std::string display_name = {},
    std::string description = {}) {
    return ContentManifest(pack_id(id), version(pack_version), version(contract_version),
                           version(minimum_core_version), std::move(dependencies),
                           std::move(display_name), std::move(description));
}

ContentDefinitionInput definition(
    std::string_view id, std::string_view resource_kind,
    std::vector<DefinitionReference> references = {}) {
    return ContentDefinitionInput(
        ResourceDefinition(definition_id(id), kind(resource_kind)),
        std::move(references));
}

DefinitionReference reference(std::string_view target,
                              std::string_view expected_kind) {
    return DefinitionReference(definition_id(target), kind(expected_kind));
}

PackDependency dependency(std::string_view target) {
    return PackDependency(
        pack_id(target), VersionRequirement::create(version("1.0.0")).value());
}

ContentPackInput pack(
    std::string_view id, std::string_view contract_version = "0.1.0",
    std::string_view minimum_core_version = "0.1.0",
    std::vector<PackDependency> dependencies = {},
    std::vector<ContentDefinitionInput> definitions = {}) {
    return ContentPackInput(
        manifest(id, contract_version, minimum_core_version, "1.0.0",
                 std::move(dependencies)),
        std::move(definitions));
}

void expect_same_diagnostic(const ContentDiagnostic& left,
                            const ContentDiagnostic& right) {
    EXPECT_EQ(left.code(), right.code());
    EXPECT_EQ(left.pack_id(), right.pack_id());
    EXPECT_EQ(left.related_pack_id(), right.related_pack_id());
    EXPECT_EQ(left.definition_id(), right.definition_id());
    EXPECT_EQ(left.related_definition_id(), right.related_definition_id());
    EXPECT_EQ(left.actual_version(), right.actual_version());
    EXPECT_EQ(left.supported_version(), right.supported_version());
    ASSERT_EQ(left.version_requirement().has_value(),
              right.version_requirement().has_value());
    if (left.version_requirement().has_value()) {
        EXPECT_EQ(left.version_requirement()->minimum_inclusive(),
                  right.version_requirement()->minimum_inclusive());
        EXPECT_EQ(left.version_requirement()->maximum_exclusive(),
                  right.version_requirement()->maximum_exclusive());
    }
}

TEST(ContentCompatibilityTest, CurrentPolicySupportsInitialContract) {
    const auto compatibility = ContentCompatibility::current();

    EXPECT_EQ(compatibility.core_version(), version("0.1.0"));
    EXPECT_EQ(compatibility.supported_content_contract_version(), version("0.1.0"));
}

TEST(ContentCompatibilityTest, MatchingContractAndEqualMinimumCoreAreCompatible) {
    const auto diagnostics = ContentCompatibility::current().validate(
        manifest("example.pack", "0.1.0", "0.1.0"));

    EXPECT_TRUE(diagnostics.empty());
}

TEST(ContentCompatibilityTest, MinimumCoreBelowCurrentCoreIsCompatible) {
    const auto diagnostics = ContentCompatibility::current().validate(
        manifest("example.pack", "0.1.0", "0.0.9"));

    EXPECT_TRUE(diagnostics.empty());
}

TEST(ContentCompatibilityTest, MinimumCoreAboveCurrentCorePreservesRequirement) {
    const auto diagnostics = ContentCompatibility::current().validate(
        manifest("example.pack", "0.1.0", "0.2.0"));

    ASSERT_EQ(diagnostics.size(), 1U);
    const auto& diagnostic = diagnostics.front();
    EXPECT_EQ(diagnostic.code(),
              ContentDiagnosticCode::minimum_core_version_not_met);
    EXPECT_EQ(diagnostic.pack_id(), pack_id("example.pack"));
    ASSERT_TRUE(diagnostic.version_requirement().has_value());
    EXPECT_EQ(diagnostic.version_requirement()->minimum_inclusive(),
              version("0.2.0"));
    EXPECT_FALSE(diagnostic.version_requirement()->maximum_exclusive().has_value());
    ASSERT_TRUE(diagnostic.actual_version().has_value());
    EXPECT_EQ(*diagnostic.actual_version(), version("0.1.0"));
}

TEST(ContentCompatibilityTest, OlderContentContractIsRejected) {
    const auto diagnostics = ContentCompatibility::current().validate(
        manifest("example.pack", "0.0.9", "0.1.0"));

    ASSERT_EQ(diagnostics.size(), 1U);
    EXPECT_EQ(diagnostics.front().code(),
              ContentDiagnosticCode::unsupported_content_contract_version);
}

TEST(ContentCompatibilityTest, NewerContentContractIsRejected) {
    const auto diagnostics = ContentCompatibility::current().validate(
        manifest("example.pack", "0.2.0", "0.1.0"));

    ASSERT_EQ(diagnostics.size(), 1U);
    EXPECT_EQ(diagnostics.front().code(),
              ContentDiagnosticCode::unsupported_content_contract_version);
}

TEST(ContentCompatibilityTest, ContractDiagnosticPreservesStructuredVersions) {
    const auto diagnostics = ContentCompatibility::current().validate(
        manifest("example.pack", "1.0.0", "0.1.0"));

    ASSERT_EQ(diagnostics.size(), 1U);
    const auto& diagnostic = diagnostics.front();
    EXPECT_EQ(diagnostic.pack_id(), pack_id("example.pack"));
    ASSERT_TRUE(diagnostic.actual_version().has_value());
    EXPECT_EQ(*diagnostic.actual_version(), version("1.0.0"));
    ASSERT_TRUE(diagnostic.supported_version().has_value());
    EXPECT_EQ(*diagnostic.supported_version(), version("0.1.0"));
}

TEST(ContentCompatibilityTest, IndependentFailuresProduceBothDiagnostics) {
    const auto diagnostics = ContentCompatibility::current().validate(
        manifest("example.pack", "0.2.0", "0.2.0"));

    ASSERT_EQ(diagnostics.size(), 2U);
    EXPECT_EQ(diagnostics[0].code(),
              ContentDiagnosticCode::unsupported_content_contract_version);
    EXPECT_EQ(diagnostics[1].code(),
              ContentDiagnosticCode::minimum_core_version_not_met);
}

TEST(ContentCompatibilityTest, PackVersionDoesNotAffectCompatibility) {
    const auto compatibility = ContentCompatibility::current();

    EXPECT_TRUE(compatibility
                    .validate(manifest("example.pack", "0.1.0", "0.1.0",
                                       "0.0.1"))
                    .empty());
    EXPECT_TRUE(compatibility
                    .validate(manifest("example.pack", "0.1.0", "0.1.0",
                                       "99.0.0"))
                    .empty());
}

TEST(ContentCompatibilityTest, InformativeMetadataDoesNotAffectCompatibility) {
    const auto compatibility = ContentCompatibility::current();

    EXPECT_TRUE(compatibility
                    .validate(manifest("example.pack", "0.1.0", "0.1.0",
                                       "1.0.0", {}, "Display", "Description"))
                    .empty());
}

TEST(ContentCompatibilityTest, DiagnosticsAreCanonicalAcrossPackPermutations) {
    const auto forward = corerpg::ContentCatalog::create(
        {pack("zeta.pack", "0.2.0", "0.2.0"),
         pack("alpha.pack", "0.2.0", "0.2.0")});
    const auto reverse = corerpg::ContentCatalog::create(
        {pack("alpha.pack", "0.2.0", "0.2.0"),
         pack("zeta.pack", "0.2.0", "0.2.0")});

    ASSERT_NE(forward.diagnostics(), nullptr);
    ASSERT_NE(reverse.diagnostics(), nullptr);
    ASSERT_EQ(forward.diagnostics()->size(), 4U);
    ASSERT_EQ(reverse.diagnostics()->size(), 4U);
    for (std::size_t index = 0; index < forward.diagnostics()->size(); ++index) {
        expect_same_diagnostic((*forward.diagnostics())[index],
                               (*reverse.diagnostics())[index]);
    }
    EXPECT_EQ((*forward.diagnostics())[0].pack_id(), pack_id("alpha.pack"));
    EXPECT_EQ((*forward.diagnostics())[2].pack_id(), pack_id("zeta.pack"));
}

TEST(ContentCompatibilityTest, DuplicatePackPhasePrecedesCompatibility) {
    const auto result = corerpg::ContentCatalog::create(
        {pack("example.same", "0.2.0"), pack("example.same", "0.0.9")});

    ASSERT_NE(result.diagnostics(), nullptr);
    ASSERT_EQ(result.diagnostics()->size(), 1U);
    EXPECT_EQ(result.diagnostics()->front().code(),
              ContentDiagnosticCode::duplicate_pack);
}

TEST(ContentCompatibilityTest, OwnershipPhasePrecedesCompatibility) {
    const auto result = corerpg::ContentCatalog::create(
        {pack("example.one", "0.2.0", "0.2.0", {},
              {definition("example.two:item", "item")})});

    ASSERT_NE(result.diagnostics(), nullptr);
    ASSERT_EQ(result.diagnostics()->size(), 1U);
    EXPECT_EQ(result.diagnostics()->front().code(),
              ContentDiagnosticCode::definition_pack_mismatch);
}

TEST(ContentCompatibilityTest, CompatibilityFailureStopsLaterValidation) {
    const auto result = corerpg::ContentCatalog::create(
        {pack("example.one", "0.2.0", "0.2.0",
              {dependency("example.missing")},
              {definition("example.one:source", "node",
                          {reference("example.other:missing", "node")})})});

    ASSERT_NE(result.diagnostics(), nullptr);
    ASSERT_EQ(result.diagnostics()->size(), 2U);
    EXPECT_EQ((*result.diagnostics())[0].code(),
              ContentDiagnosticCode::unsupported_content_contract_version);
    EXPECT_EQ((*result.diagnostics())[1].code(),
              ContentDiagnosticCode::minimum_core_version_not_met);
}

TEST(ContentCompatibilityTest, FailedBuildDoesNotModifyExistingCatalog) {
    const auto valid = corerpg::ContentCatalog::create(
        {pack("example.valid", "0.1.0", "0.1.0", {},
              {definition("example.valid:item", "item")})});
    ASSERT_NE(valid.catalog(), nullptr);

    const auto invalid = corerpg::ContentCatalog::create(
        {pack("example.invalid", "0.2.0", "0.2.0")});

    EXPECT_FALSE(invalid.succeeded());
    EXPECT_TRUE(valid.catalog()->contains(definition_id("example.valid:item")));
    EXPECT_EQ(valid.catalog()->size(), 1U);
}

}  // namespace
