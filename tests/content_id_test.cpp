#include <corerpg/definition_id.hpp>
#include <corerpg/entity_id.hpp>
#include <corerpg/pack_id.hpp>

#include <gtest/gtest.h>

#include <array>
#include <concepts>
#include <string>
#include <string_view>
#include <type_traits>

namespace {

static_assert(!std::is_same_v<corerpg::EntityId, corerpg::PackId>);
static_assert(!std::is_same_v<corerpg::PackId, corerpg::DefinitionId>);
static_assert(!std::is_default_constructible_v<corerpg::PackId>);
static_assert(!std::is_default_constructible_v<corerpg::DefinitionId>);
static_assert(std::totally_ordered<corerpg::PackId>);
static_assert(std::totally_ordered<corerpg::DefinitionId>);

TEST(PackIdTest, AcceptsValidCanonicalIds) {
    constexpr std::array valid_ids{
        std::string_view{"nimroel.core"},
        std::string_view{"rome.core"},
        std::string_view{"belmont.nimroel"},
        std::string_view{"example.test"},
        std::string_view{"acme.ancient-rome"},
        std::string_view{"a.b"},
        std::string_view{"a0.b-"},
    };

    for (const auto text : valid_ids) {
        SCOPED_TRACE(std::string{text});
        const auto id = corerpg::PackId::parse(text);
        ASSERT_TRUE(id.has_value());
        EXPECT_EQ(id->value(), text);
    }
}

TEST(PackIdTest, RejectsInvalidIds) {
    constexpr std::array invalid_ids{
        std::string_view{""},
        std::string_view{"nimroel"},
        std::string_view{"Nimroel.core"},
        std::string_view{"nimroel.Core"},
        std::string_view{"nimroel..core"},
        std::string_view{".nimroel"},
        std::string_view{"nimroel."},
        std::string_view{"nimroel/core"},
        std::string_view{"nimroel:core"},
        std::string_view{"nimroel core"},
        std::string_view{"nimroel_corp.core"},
        std::string_view{"1nimroel.core"},
        std::string_view{"nimroel.1core"},
        std::string_view{"-nimroel.core"},
        std::string_view{"nimroel.-core"},
        std::string_view{"nimroel.c\xC3\xB3re"},
    };

    for (const auto text : invalid_ids) {
        SCOPED_TRACE(std::string{text});
        EXPECT_FALSE(corerpg::PackId::parse(text).has_value());
    }
}

TEST(PackIdTest, SupportsValueEqualityAndInequality) {
    const auto first = corerpg::PackId::parse("example.test");
    const auto same = corerpg::PackId::parse("example.test");
    const auto other = corerpg::PackId::parse("example.tools");

    ASSERT_TRUE(first.has_value());
    ASSERT_TRUE(same.has_value());
    ASSERT_TRUE(other.has_value());
    EXPECT_EQ(*first, *same);
    EXPECT_NE(*first, *other);
}

TEST(PackIdTest, OrdersByCanonicalBytes) {
    const auto first = corerpg::PackId::parse("alpha.core");
    const auto second = corerpg::PackId::parse("alpha.tools");
    const auto third = corerpg::PackId::parse("beta.core");

    ASSERT_TRUE(first.has_value());
    ASSERT_TRUE(second.has_value());
    ASSERT_TRUE(third.has_value());
    EXPECT_LT(*first, *second);
    EXPECT_LT(*second, *third);
}

TEST(DefinitionIdTest, AcceptsValidCanonicalIdsAndExposesComponents) {
    constexpr std::array valid_ids{
        std::string_view{"nimroel.core:characters/merchant"},
        std::string_view{"nimroel.core:regions/north-1"},
        std::string_view{"nimroel.core:rules/combat.basic"},
        std::string_view{"nimroel.core:items/sword_long"},
        std::string_view{"example.test:0"},
        std::string_view{"a.b:x/y"},
    };

    for (const auto text : valid_ids) {
        SCOPED_TRACE(std::string{text});
        const auto id = corerpg::DefinitionId::parse(text);
        ASSERT_TRUE(id.has_value());
        EXPECT_EQ(id->value(), text);
    }

    const auto id = corerpg::DefinitionId::parse("nimroel.core:characters/merchant");
    ASSERT_TRUE(id.has_value());
    EXPECT_EQ(id->pack_id().value(), "nimroel.core");
    EXPECT_EQ(id->local_path(), "characters/merchant");
}

TEST(DefinitionIdTest, RejectsMissingSeparatorAndInvalidPackIds) {
    constexpr std::array invalid_ids{
        std::string_view{"nimroel.core"},
        std::string_view{":characters/merchant"},
        std::string_view{"nimroel:characters/merchant"},
        std::string_view{"Nimroel.core:characters/merchant"},
        std::string_view{"nimroel..core:characters/merchant"},
    };

    for (const auto text : invalid_ids) {
        SCOPED_TRACE(std::string{text});
        EXPECT_FALSE(corerpg::DefinitionId::parse(text).has_value());
    }
}

TEST(DefinitionIdTest, RejectsInvalidLocalPaths) {
    constexpr std::array invalid_ids{
        std::string_view{"nimroel.core:"},
        std::string_view{"nimroel.core:/characters/merchant"},
        std::string_view{"nimroel.core:characters/"},
        std::string_view{"nimroel.core:characters//merchant"},
        std::string_view{"nimroel.core:characters/./merchant"},
        std::string_view{"nimroel.core:characters/../merchant"},
        std::string_view{"nimroel.core:Characters/merchant"},
        std::string_view{"nimroel.core:characters/Merchant"},
        std::string_view{"nimroel.core:characters\\merchant"},
        std::string_view{"nimroel.core:characters:merchant"},
        std::string_view{"nimroel.core:characters merchant"},
        std::string_view{"nimroel.core:characters/_merchant"},
        std::string_view{"nimroel.core:characters/-merchant"},
        std::string_view{"nimroel.core:characters/.merchant"},
        std::string_view{"nimroel.core:characters/\xC3\xA9lite"},
    };

    for (const auto text : invalid_ids) {
        SCOPED_TRACE(std::string{text});
        EXPECT_FALSE(corerpg::DefinitionId::parse(text).has_value());
    }
}

TEST(DefinitionIdTest, SupportsValueEqualityAndInequality) {
    const auto first = corerpg::DefinitionId::parse("example.test:items/sword");
    const auto same = corerpg::DefinitionId::parse("example.test:items/sword");
    const auto other = corerpg::DefinitionId::parse("example.test:items/shield");

    ASSERT_TRUE(first.has_value());
    ASSERT_TRUE(same.has_value());
    ASSERT_TRUE(other.has_value());
    EXPECT_EQ(*first, *same);
    EXPECT_NE(*first, *other);
}

TEST(DefinitionIdTest, OrdersByCanonicalBytes) {
    const auto first = corerpg::DefinitionId::parse("alpha.core:items/sword");
    const auto second = corerpg::DefinitionId::parse("alpha.core:rules/basic");
    const auto third = corerpg::DefinitionId::parse("beta.core:items/sword");

    ASSERT_TRUE(first.has_value());
    ASSERT_TRUE(second.has_value());
    ASSERT_TRUE(third.has_value());
    EXPECT_LT(*first, *second);
    EXPECT_LT(*second, *third);
}

} // namespace
