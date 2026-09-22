#include <concepts>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "corerpg/content_catalog.hpp"

namespace {

using corerpg::CatalogBuildResult;
using corerpg::ContentDefinitionInput;
using corerpg::ContentDiagnostic;
using corerpg::ContentDiagnosticCode;
using corerpg::DefinitionId;
using corerpg::DefinitionReference;
using corerpg::ResourceDefinition;
using corerpg::ResourceKind;

ResourceKind kind(std::string_view value) {
    return *ResourceKind::parse(value);
}

DefinitionId id(std::string_view value) {
    return *DefinitionId::parse(value);
}

DefinitionReference reference(std::string_view target, std::string_view expected_kind) {
    return DefinitionReference(id(target), kind(expected_kind));
}

ContentDefinitionInput input(
    std::string_view definition_id, std::string_view definition_kind,
    std::vector<DefinitionReference> references = {}) {
    return ContentDefinitionInput(
        ResourceDefinition(id(definition_id), kind(definition_kind)), std::move(references));
}

void expect_same_diagnostics(const CatalogBuildResult& left, const CatalogBuildResult& right) {
    ASSERT_NE(left.diagnostics(), nullptr);
    ASSERT_NE(right.diagnostics(), nullptr);
    ASSERT_EQ(left.diagnostics()->size(), right.diagnostics()->size());

    for (std::size_t index = 0; index < left.diagnostics()->size(); ++index) {
        const auto& left_diagnostic = (*left.diagnostics())[index];
        const auto& right_diagnostic = (*right.diagnostics())[index];
        EXPECT_EQ(left_diagnostic.code(), right_diagnostic.code());
        EXPECT_EQ(left_diagnostic.definition_id(), right_diagnostic.definition_id());
        EXPECT_EQ(left_diagnostic.related_definition_id(),
                  right_diagnostic.related_definition_id());
        EXPECT_EQ(left_diagnostic.expected_kind(), right_diagnostic.expected_kind());
        EXPECT_EQ(left_diagnostic.actual_kind(), right_diagnostic.actual_kind());
    }
}

static_assert(!std::is_default_constructible_v<ResourceKind>);
static_assert(std::totally_ordered<ResourceKind>);
static_assert(!std::is_default_constructible_v<corerpg::ContentCatalog>);
static_assert(!std::is_default_constructible_v<CatalogBuildResult>);

TEST(ResourceKindTest, AcceptsCanonicalKinds) {
    for (const std::string_view value : {"a", "region", "world-rule", "campaign_template",
                                         "kind0", "a-", "a_"}) {
        const auto parsed = ResourceKind::parse(value);

        ASSERT_TRUE(parsed.has_value()) << value;
        EXPECT_EQ(parsed->value(), value);
    }
}

TEST(ResourceKindTest, RejectsValuesOutsideItsGrammar) {
    for (const std::string_view value : {"",        "Region", "regIon", "0region",
                                         "_region", "-region", "two words", "region.type",
                                         "region/type", "region\\type", "region:type",
                                         "regi\xC3\xB3n"}) {
        EXPECT_FALSE(ResourceKind::parse(value).has_value()) << value;
    }
}

TEST(ResourceKindTest, EqualityAndOrderingUseCanonicalBytes) {
    const auto region = kind("region");
    const auto same_region = kind("region");
    const auto species = kind("species");

    EXPECT_EQ(region, same_region);
    EXPECT_NE(region, species);
    EXPECT_LT(region, species);
    EXPECT_EQ(region.value(), "region");
}

TEST(ResourceDefinitionTest, OwnsIndependentIdentityAndKindValues) {
    const ResourceDefinition definition(id("example.pack:opaque/value"), kind("world-rule"));

    EXPECT_EQ(definition.id().value(), "example.pack:opaque/value");
    EXPECT_EQ(definition.kind().value(), "world-rule");
}

TEST(DefinitionReferenceTest, OwnsTargetAndExpectedKind) {
    const DefinitionReference value(id("example.pack:target"), kind("target-kind"));

    EXPECT_EQ(value.target().value(), "example.pack:target");
    EXPECT_EQ(value.expected_kind().value(), "target-kind");
}

TEST(ContentDefinitionInputTest, OwnsDefinitionAndReferences) {
    const auto value = input("example.pack:source", "source-kind",
                             {reference("example.pack:target", "target-kind")});

    EXPECT_EQ(value.definition().id().value(), "example.pack:source");
    EXPECT_EQ(value.definition().kind().value(), "source-kind");
    ASSERT_EQ(value.references().size(), 1U);
    EXPECT_EQ(value.references().front().target().value(), "example.pack:target");
    EXPECT_EQ(value.references().front().expected_kind().value(), "target-kind");
}

TEST(ContentCatalogTest, EmptyCatalogIsAValidExclusiveSuccess) {
    const auto result = corerpg::ContentCatalog::create({});

    ASSERT_TRUE(result.succeeded());
    ASSERT_NE(result.catalog(), nullptr);
    EXPECT_EQ(result.diagnostics(), nullptr);
    EXPECT_TRUE(result.catalog()->empty());
    EXPECT_EQ(result.catalog()->size(), 0U);
    EXPECT_TRUE(result.catalog()->definitions().empty());
}

TEST(ContentCatalogTest, DefinitionWithoutReferencesSucceeds) {
    const auto expected_id = id("example.pack:regions/north");
    const auto result = corerpg::ContentCatalog::create(
        {input("example.pack:regions/north", "region")});

    ASSERT_TRUE(result.succeeded());
    ASSERT_NE(result.catalog(), nullptr);
    EXPECT_EQ(result.catalog()->size(), 1U);
    EXPECT_TRUE(result.catalog()->contains(expected_id));
    ASSERT_NE(result.catalog()->find(expected_id), nullptr);
    EXPECT_EQ(result.catalog()->find(expected_id)->kind().value(), "region");
}

TEST(ContentCatalogTest, MultipleDefinitionsAreExposedInCanonicalIdentityOrder) {
    const auto result = corerpg::ContentCatalog::create(
        {input("zeta.pack:item", "species"), input("alpha.pack:z-last", "region"),
         input("alpha.pack:a-first", "world-rule")});

    ASSERT_TRUE(result.succeeded());
    ASSERT_NE(result.catalog(), nullptr);
    ASSERT_EQ(result.catalog()->definitions().size(), 3U);
    EXPECT_EQ(result.catalog()->definitions()[0].id().value(), "alpha.pack:a-first");
    EXPECT_EQ(result.catalog()->definitions()[1].id().value(), "alpha.pack:z-last");
    EXPECT_EQ(result.catalog()->definitions()[2].id().value(), "zeta.pack:item");
}

TEST(ContentCatalogTest, LookupOfAnAbsentIdentityReturnsNull) {
    const auto result = corerpg::ContentCatalog::create(
        {input("example.pack:present", "region")});

    ASSERT_TRUE(result.succeeded());
    ASSERT_NE(result.catalog(), nullptr);
    const auto missing = id("example.pack:absent");
    EXPECT_FALSE(result.catalog()->contains(missing));
    EXPECT_EQ(result.catalog()->find(missing), nullptr);
}

TEST(ContentCatalogTest, RejectsDuplicateIdentityWithTheSameKind) {
    const auto result = corerpg::ContentCatalog::create(
        {input("example.pack:duplicate", "region"),
         input("example.pack:duplicate", "region")});

    ASSERT_FALSE(result.succeeded());
    EXPECT_EQ(result.catalog(), nullptr);
    ASSERT_NE(result.diagnostics(), nullptr);
    ASSERT_EQ(result.diagnostics()->size(), 1U);
    EXPECT_EQ(result.diagnostics()->front().code(),
              ContentDiagnosticCode::duplicate_definition);
    EXPECT_EQ(result.diagnostics()->front().definition_id().value(),
              "example.pack:duplicate");
    EXPECT_FALSE(result.diagnostics()->front().related_definition_id().has_value());
    EXPECT_FALSE(result.diagnostics()->front().expected_kind().has_value());
    EXPECT_FALSE(result.diagnostics()->front().actual_kind().has_value());
}

TEST(ContentCatalogTest, RejectsDuplicateIdentityEvenWhenKindsDiffer) {
    const auto result = corerpg::ContentCatalog::create(
        {input("example.pack:duplicate", "region"),
         input("example.pack:duplicate", "species")});

    ASSERT_FALSE(result.succeeded());
    ASSERT_NE(result.diagnostics(), nullptr);
    ASSERT_EQ(result.diagnostics()->size(), 1U);
    EXPECT_EQ(result.diagnostics()->front().code(),
              ContentDiagnosticCode::duplicate_definition);
}

TEST(ContentCatalogTest, ReportsEveryDuplicateIdentityInCanonicalOrder) {
    const auto result = corerpg::ContentCatalog::create(
        {input("zeta.pack:duplicate", "kind-a"),
         input("alpha.pack:duplicate", "kind-b"),
         input("zeta.pack:duplicate", "kind-c"),
         input("alpha.pack:duplicate", "kind-d"),
         input("zeta.pack:duplicate", "kind-e")});

    ASSERT_FALSE(result.succeeded());
    ASSERT_NE(result.diagnostics(), nullptr);
    ASSERT_EQ(result.diagnostics()->size(), 2U);
    EXPECT_EQ((*result.diagnostics())[0].code(),
              ContentDiagnosticCode::duplicate_definition);
    EXPECT_EQ((*result.diagnostics())[0].definition_id().value(),
              "alpha.pack:duplicate");
    EXPECT_EQ((*result.diagnostics())[1].code(),
              ContentDiagnosticCode::duplicate_definition);
    EXPECT_EQ((*result.diagnostics())[1].definition_id().value(),
              "zeta.pack:duplicate");
}

TEST(ContentCatalogTest, DuplicatePhaseDoesNotAttemptReferenceResolution) {
    const auto result = corerpg::ContentCatalog::create(
        {input("example.pack:duplicate", "source-kind",
               {reference("example.pack:missing", "target-kind")}),
         input("example.pack:duplicate", "other-kind")});

    ASSERT_FALSE(result.succeeded());
    ASSERT_NE(result.diagnostics(), nullptr);
    ASSERT_EQ(result.diagnostics()->size(), 1U);
    EXPECT_EQ(result.diagnostics()->front().code(),
              ContentDiagnosticCode::duplicate_definition);
}

TEST(ContentCatalogTest, SameLocalPathCanExistUnderDifferentPacks) {
    const auto first = id("first.pack:shared/path");
    const auto second = id("second.pack:shared/path");
    const auto result = corerpg::ContentCatalog::create(
        {input("first.pack:shared/path", "region"),
         input("second.pack:shared/path", "region")});

    ASSERT_TRUE(result.succeeded());
    ASSERT_NE(result.catalog(), nullptr);
    EXPECT_EQ(result.catalog()->size(), 2U);
    EXPECT_TRUE(result.catalog()->contains(first));
    EXPECT_TRUE(result.catalog()->contains(second));
}

TEST(ContentCatalogTest, InputOrderDoesNotAffectCatalogOrderOrLookup) {
    const auto forward = corerpg::ContentCatalog::create(
        {input("alpha.pack:item", "region"), input("zeta.pack:item", "species")});
    const auto reverse = corerpg::ContentCatalog::create(
        {input("zeta.pack:item", "species"), input("alpha.pack:item", "region")});

    ASSERT_TRUE(forward.succeeded());
    ASSERT_TRUE(reverse.succeeded());
    ASSERT_NE(forward.catalog(), nullptr);
    ASSERT_NE(reverse.catalog(), nullptr);
    ASSERT_EQ(forward.catalog()->definitions().size(),
              reverse.catalog()->definitions().size());
    for (std::size_t index = 0; index < forward.catalog()->definitions().size(); ++index) {
        EXPECT_EQ(forward.catalog()->definitions()[index].id(),
                  reverse.catalog()->definitions()[index].id());
        EXPECT_EQ(forward.catalog()->definitions()[index].kind(),
                  reverse.catalog()->definitions()[index].kind());
    }
}

TEST(ContentCatalogTest, ExistingReferenceWithExpectedKindSucceeds) {
    const auto source_id = id("example.pack:source");
    const auto target_id = id("example.pack:target");
    const auto result = corerpg::ContentCatalog::create(
        {input("example.pack:source", "source-kind",
               {reference("example.pack:target", "target-kind")}),
         input("example.pack:target", "target-kind")});

    ASSERT_TRUE(result.succeeded());
    ASSERT_NE(result.catalog(), nullptr);
    EXPECT_EQ(result.diagnostics(), nullptr);
    EXPECT_TRUE(result.catalog()->contains(source_id));
    EXPECT_TRUE(result.catalog()->contains(target_id));
}

TEST(ContentCatalogTest, MissingTargetProducesStructuredExclusiveFailure) {
    const auto result = corerpg::ContentCatalog::create(
        {input("example.pack:source", "source-kind",
               {reference("example.pack:missing", "target-kind")})});

    ASSERT_FALSE(result.succeeded());
    EXPECT_EQ(result.catalog(), nullptr);
    ASSERT_NE(result.diagnostics(), nullptr);
    ASSERT_EQ(result.diagnostics()->size(), 1U);
    const auto& diagnostic = result.diagnostics()->front();
    EXPECT_EQ(diagnostic.code(), ContentDiagnosticCode::reference_target_not_found);
    EXPECT_EQ(diagnostic.definition_id().value(), "example.pack:source");
    ASSERT_TRUE(diagnostic.related_definition_id().has_value());
    EXPECT_EQ(diagnostic.related_definition_id()->value(), "example.pack:missing");
    ASSERT_TRUE(diagnostic.expected_kind().has_value());
    EXPECT_EQ(diagnostic.expected_kind()->value(), "target-kind");
    EXPECT_FALSE(diagnostic.actual_kind().has_value());
}

TEST(ContentCatalogTest, KindMismatchPreservesSourceTargetExpectedAndActual) {
    const auto result = corerpg::ContentCatalog::create(
        {input("example.pack:source", "source-kind",
               {reference("example.pack:target", "expected-kind")}),
         input("example.pack:target", "actual-kind")});

    ASSERT_FALSE(result.succeeded());
    ASSERT_NE(result.diagnostics(), nullptr);
    ASSERT_EQ(result.diagnostics()->size(), 1U);
    const auto& diagnostic = result.diagnostics()->front();
    EXPECT_EQ(diagnostic.code(), ContentDiagnosticCode::reference_kind_mismatch);
    EXPECT_EQ(diagnostic.definition_id().value(), "example.pack:source");
    ASSERT_TRUE(diagnostic.related_definition_id().has_value());
    EXPECT_EQ(diagnostic.related_definition_id()->value(), "example.pack:target");
    ASSERT_TRUE(diagnostic.expected_kind().has_value());
    EXPECT_EQ(diagnostic.expected_kind()->value(), "expected-kind");
    ASSERT_TRUE(diagnostic.actual_kind().has_value());
    EXPECT_EQ(diagnostic.actual_kind()->value(), "actual-kind");
}

TEST(ContentCatalogTest, MultipleReferenceErrorsUseCanonicalOrder) {
    const auto result = corerpg::ContentCatalog::create(
        {input("beta.pack:source", "source-kind",
               {reference("beta.pack:missing", "target-kind")}),
         input("alpha.pack:target", "actual-kind"),
         input("alpha.pack:source", "source-kind",
               {reference("alpha.pack:target", "expected-kind"),
                reference("alpha.pack:missing", "target-kind")})});

    ASSERT_FALSE(result.succeeded());
    ASSERT_NE(result.diagnostics(), nullptr);
    ASSERT_EQ(result.diagnostics()->size(), 3U);
    EXPECT_EQ((*result.diagnostics())[0].definition_id().value(), "alpha.pack:source");
    EXPECT_EQ((*result.diagnostics())[0].related_definition_id()->value(),
              "alpha.pack:missing");
    EXPECT_EQ((*result.diagnostics())[0].code(),
              ContentDiagnosticCode::reference_target_not_found);
    EXPECT_EQ((*result.diagnostics())[1].definition_id().value(), "alpha.pack:source");
    EXPECT_EQ((*result.diagnostics())[1].related_definition_id()->value(),
              "alpha.pack:target");
    EXPECT_EQ((*result.diagnostics())[1].code(),
              ContentDiagnosticCode::reference_kind_mismatch);
    EXPECT_EQ((*result.diagnostics())[2].definition_id().value(), "beta.pack:source");
}

TEST(ContentCatalogTest, PermutingInputsDoesNotChangeDiagnostics) {
    const auto forward = corerpg::ContentCatalog::create(
        {input("alpha.pack:source", "source-kind",
               {reference("alpha.pack:missing", "target-kind")}),
         input("beta.pack:source", "source-kind",
               {reference("beta.pack:missing", "target-kind")})});
    const auto reverse = corerpg::ContentCatalog::create(
        {input("beta.pack:source", "source-kind",
               {reference("beta.pack:missing", "target-kind")}),
         input("alpha.pack:source", "source-kind",
               {reference("alpha.pack:missing", "target-kind")})});

    expect_same_diagnostics(forward, reverse);
}

TEST(ContentCatalogTest, PermutingReferencesDoesNotChangeDiagnostics) {
    const auto forward = corerpg::ContentCatalog::create(
        {input("example.pack:source", "source-kind",
               {reference("example.pack:a-missing", "kind-a"),
                reference("example.pack:z-missing", "kind-z")})});
    const auto reverse = corerpg::ContentCatalog::create(
        {input("example.pack:source", "source-kind",
               {reference("example.pack:z-missing", "kind-z"),
                reference("example.pack:a-missing", "kind-a")})});

    expect_same_diagnostics(forward, reverse);
}

TEST(ContentCatalogTest, RepeatedIdenticalReferencesProduceOneDiagnostic) {
    const auto repeated = reference("example.pack:missing", "target-kind");
    const auto result = corerpg::ContentCatalog::create(
        {input("example.pack:source", "source-kind", {repeated, repeated})});

    ASSERT_FALSE(result.succeeded());
    ASSERT_NE(result.diagnostics(), nullptr);
    EXPECT_EQ(result.diagnostics()->size(), 1U);
}

TEST(ContentCatalogTest, CrossPackReferenceResolvesWithoutDependencyPolicy) {
    const auto result = corerpg::ContentCatalog::create(
        {input("first.pack:source", "source-kind",
               {reference("second.pack:target", "target-kind")}),
         input("second.pack:target", "target-kind")});

    ASSERT_TRUE(result.succeeded());
    ASSERT_NE(result.catalog(), nullptr);
    EXPECT_TRUE(result.catalog()->contains(id("first.pack:source")));
    EXPECT_TRUE(result.catalog()->contains(id("second.pack:target")));
}

TEST(ContentCatalogTest, MutualReferenceCycleIsValid) {
    const auto result = corerpg::ContentCatalog::create(
        {input("example.pack:a", "node",
               {reference("example.pack:b", "node")}),
         input("example.pack:b", "node",
               {reference("example.pack:a", "node")})});

    EXPECT_TRUE(result.succeeded());
    EXPECT_NE(result.catalog(), nullptr);
}

TEST(ContentCatalogTest, SelfReferenceIsValid) {
    const auto result = corerpg::ContentCatalog::create(
        {input("example.pack:self", "node",
               {reference("example.pack:self", "node")})});

    EXPECT_TRUE(result.succeeded());
    EXPECT_NE(result.catalog(), nullptr);
}

TEST(ContentCatalogTest, FailedCandidateDoesNotModifyExistingCatalog) {
    const auto valid = corerpg::ContentCatalog::create(
        {input("active.pack:definition", "stable-kind")});
    ASSERT_TRUE(valid.succeeded());
    ASSERT_NE(valid.catalog(), nullptr);

    const auto invalid = corerpg::ContentCatalog::create(
        {input("candidate.pack:source", "source-kind",
               {reference("candidate.pack:missing", "target-kind")})});

    EXPECT_FALSE(invalid.succeeded());
    EXPECT_TRUE(valid.catalog()->contains(id("active.pack:definition")));
    EXPECT_EQ(valid.catalog()->size(), 1U);
}

TEST(ContentCatalogTest, CreatingFromAnLvalueDoesNotModifyInputs) {
    std::vector<ContentDefinitionInput> definitions;
    definitions.push_back(input("zeta.pack:target", "target-kind"));
    definitions.push_back(input(
        "example.pack:source", "source-kind",
        {reference("zeta.pack:target", "target-kind"),
         reference("alpha.pack:target", "target-kind")}));
    definitions.push_back(input("alpha.pack:target", "target-kind"));

    const auto result = corerpg::ContentCatalog::create(definitions);

    ASSERT_TRUE(result.succeeded());
    ASSERT_EQ(definitions.size(), 3U);
    EXPECT_EQ(definitions[0].definition().id().value(), "zeta.pack:target");
    EXPECT_EQ(definitions[1].definition().id().value(), "example.pack:source");
    ASSERT_EQ(definitions[1].references().size(), 2U);
    EXPECT_EQ(definitions[1].references()[0].target().value(), "zeta.pack:target");
    EXPECT_EQ(definitions[1].references()[1].target().value(), "alpha.pack:target");
    EXPECT_EQ(definitions[2].definition().id().value(), "alpha.pack:target");
}

}  // namespace
