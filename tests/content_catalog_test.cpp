#include <concepts>
#include <string_view>
#include <type_traits>
#include <vector>

#include <gtest/gtest.h>

#include "corerpg/content_catalog.hpp"

namespace {

using corerpg::ContentCatalog;
using corerpg::DefinitionId;
using corerpg::ResourceDefinition;
using corerpg::ResourceKind;

ResourceKind kind(std::string_view value) {
    return *ResourceKind::parse(value);
}

DefinitionId id(std::string_view value) {
    return *DefinitionId::parse(value);
}

static_assert(!std::is_default_constructible_v<ResourceKind>);
static_assert(std::totally_ordered<ResourceKind>);
static_assert(!std::is_default_constructible_v<ContentCatalog>);

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

TEST(ContentCatalogTest, EmptyCatalogIsValid) {
    const auto catalog = ContentCatalog::create({});

    ASSERT_TRUE(catalog.has_value());
    EXPECT_TRUE(catalog->empty());
    EXPECT_EQ(catalog->size(), 0U);
    EXPECT_TRUE(catalog->definitions().empty());
    EXPECT_FALSE(catalog->contains(id("example.pack:missing")));
    EXPECT_EQ(catalog->find(id("example.pack:missing")), nullptr);
}

TEST(ContentCatalogTest, OneDefinitionCanBeFoundByItsFullIdentity) {
    const auto expected_id = id("example.pack:regions/north");
    const auto catalog = ContentCatalog::create(
        {ResourceDefinition(expected_id, kind("region"))});

    ASSERT_TRUE(catalog.has_value());
    EXPECT_FALSE(catalog->empty());
    EXPECT_EQ(catalog->size(), 1U);
    EXPECT_TRUE(catalog->contains(expected_id));
    ASSERT_NE(catalog->find(expected_id), nullptr);
    EXPECT_EQ(catalog->find(expected_id)->kind().value(), "region");
}

TEST(ContentCatalogTest, MultipleDefinitionsAreExposedInCanonicalIdentityOrder) {
    const auto catalog = ContentCatalog::create(
        {ResourceDefinition(id("zeta.pack:item"), kind("species")),
         ResourceDefinition(id("alpha.pack:z-last"), kind("region")),
         ResourceDefinition(id("alpha.pack:a-first"), kind("world-rule"))});

    ASSERT_TRUE(catalog.has_value());
    ASSERT_EQ(catalog->definitions().size(), 3U);
    EXPECT_EQ(catalog->definitions()[0].id().value(), "alpha.pack:a-first");
    EXPECT_EQ(catalog->definitions()[1].id().value(), "alpha.pack:z-last");
    EXPECT_EQ(catalog->definitions()[2].id().value(), "zeta.pack:item");
}

TEST(ContentCatalogTest, LookupOfAnAbsentIdentityFailsExplicitly) {
    const auto catalog = ContentCatalog::create(
        {ResourceDefinition(id("example.pack:present"), kind("region"))});

    ASSERT_TRUE(catalog.has_value());
    const auto missing = id("example.pack:absent");
    EXPECT_FALSE(catalog->contains(missing));
    EXPECT_EQ(catalog->find(missing), nullptr);
}

TEST(ContentCatalogTest, RejectsDuplicateIdentityWithTheSameKind) {
    const auto duplicate_id = id("example.pack:duplicate");

    EXPECT_FALSE(ContentCatalog::create(
                     {ResourceDefinition(duplicate_id, kind("region")),
                      ResourceDefinition(duplicate_id, kind("region"))})
                     .has_value());
}

TEST(ContentCatalogTest, RejectsDuplicateIdentityEvenWhenKindsDiffer) {
    const auto duplicate_id = id("example.pack:duplicate");

    EXPECT_FALSE(ContentCatalog::create(
                     {ResourceDefinition(duplicate_id, kind("region")),
                      ResourceDefinition(duplicate_id, kind("species"))})
                     .has_value());
}

TEST(ContentCatalogTest, SameLocalPathCanExistUnderDifferentPacks) {
    const auto first = id("first.pack:shared/path");
    const auto second = id("second.pack:shared/path");
    const auto catalog = ContentCatalog::create(
        {ResourceDefinition(first, kind("region")),
         ResourceDefinition(second, kind("region"))});

    ASSERT_TRUE(catalog.has_value());
    EXPECT_EQ(catalog->size(), 2U);
    EXPECT_TRUE(catalog->contains(first));
    EXPECT_TRUE(catalog->contains(second));
}

TEST(ContentCatalogTest, InputOrderDoesNotAffectCatalogOrderOrLookup) {
    const auto alpha = ResourceDefinition(id("alpha.pack:item"), kind("region"));
    const auto zeta = ResourceDefinition(id("zeta.pack:item"), kind("species"));
    const auto forward = ContentCatalog::create({alpha, zeta});
    const auto reverse = ContentCatalog::create({zeta, alpha});

    ASSERT_TRUE(forward.has_value());
    ASSERT_TRUE(reverse.has_value());
    ASSERT_EQ(forward->definitions().size(), reverse->definitions().size());
    for (std::size_t index = 0; index < forward->definitions().size(); ++index) {
        EXPECT_EQ(forward->definitions()[index].id(), reverse->definitions()[index].id());
        EXPECT_EQ(forward->definitions()[index].kind(), reverse->definitions()[index].kind());
    }
    EXPECT_TRUE(forward->contains(alpha.id()));
    EXPECT_TRUE(reverse->contains(alpha.id()));
}

TEST(ContentCatalogTest, CreatingFromAnLvalueDoesNotModifyTheInputDefinitions) {
    std::vector<ResourceDefinition> definitions;
    definitions.emplace_back(id("zeta.pack:item"), kind("species"));
    definitions.emplace_back(id("alpha.pack:item"), kind("region"));

    const auto catalog = ContentCatalog::create(definitions);

    ASSERT_TRUE(catalog.has_value());
    ASSERT_EQ(definitions.size(), 2U);
    EXPECT_EQ(definitions[0].id().value(), "zeta.pack:item");
    EXPECT_EQ(definitions[0].kind().value(), "species");
    EXPECT_EQ(definitions[1].id().value(), "alpha.pack:item");
    EXPECT_EQ(definitions[1].kind().value(), "region");
}

}  // namespace
