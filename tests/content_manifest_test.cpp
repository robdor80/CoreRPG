#include <corerpg/content_manifest.hpp>
#include <corerpg/pack_dependency.hpp>
#include <corerpg/pack_id.hpp>
#include <corerpg/semantic_version.hpp>
#include <corerpg/version_requirement.hpp>

#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

[[nodiscard]] corerpg::SemanticVersion version(std::string_view text) {
    return corerpg::SemanticVersion::parse(text).value();
}

[[nodiscard]] corerpg::PackId pack_id(std::string_view text) {
    return corerpg::PackId::parse(text).value();
}

TEST(SemanticVersionTest, AcceptsStableThreeComponentVersions) {
    constexpr std::array valid_versions{
        std::string_view{"0.1.0"},
        std::string_view{"1.0.0"},
        std::string_view{"2.14.7"},
        std::string_view{"10.20.30"},
        std::string_view{"0.0.0"},
    };

    for (const auto text : valid_versions) {
        SCOPED_TRACE(std::string{text});
        const auto parsed = corerpg::SemanticVersion::parse(text);
        ASSERT_TRUE(parsed.has_value());
        EXPECT_EQ(parsed->value(), text);
    }
}

TEST(SemanticVersionTest, RejectsInvalidVersions) {
    constexpr std::array invalid_versions{
        std::string_view{""},       std::string_view{"1"},
        std::string_view{"1.0"},    std::string_view{"1.0.0.0"},
        std::string_view{"v1.0.0"}, std::string_view{"01.0.0"},
        std::string_view{"1.02.0"}, std::string_view{"1.0.03"},
        std::string_view{"1.-1.0"}, std::string_view{"1.0.x"},
        std::string_view{"1.0.0-beta"}, std::string_view{"1.0.0+build"},
        std::string_view{"1..0"},   std::string_view{"1.0."},
        std::string_view{" 1.0.0"}, std::string_view{"1.0.0 "},
    };

    for (const auto text : invalid_versions) {
        SCOPED_TRACE(std::string{text});
        EXPECT_FALSE(corerpg::SemanticVersion::parse(text).has_value());
    }
}

TEST(SemanticVersionTest, ExposesComponentsAndCanonicalRepresentation) {
    const auto parsed = corerpg::SemanticVersion::parse("10.20.30");

    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(parsed->major(), 10U);
    EXPECT_EQ(parsed->minor(), 20U);
    EXPECT_EQ(parsed->patch(), 30U);
    EXPECT_EQ(parsed->value(), "10.20.30");
}

TEST(SemanticVersionTest, SupportsEqualityAndSemanticOrdering) {
    EXPECT_EQ(version("1.2.3"), version("1.2.3"));
    EXPECT_NE(version("1.2.3"), version("1.2.4"));
    EXPECT_LT(version("1.2.3"), version("1.2.4"));
    EXPECT_LT(version("1.2.9"), version("1.3.0"));
    EXPECT_LT(version("1.99.99"), version("2.0.0"));
    EXPECT_LT(version("2.0.0"), version("10.0.0"));
}

TEST(SemanticVersionTest, AcceptsComponentMaximumAndRejectsOverflow) {
    constexpr auto maximum = std::numeric_limits<std::uint32_t>::max();
    const auto parsed = corerpg::SemanticVersion::parse(
        "4294967295.4294967295.4294967295");

    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(parsed->major(), maximum);
    EXPECT_EQ(parsed->minor(), maximum);
    EXPECT_EQ(parsed->patch(), maximum);
    EXPECT_FALSE(corerpg::SemanticVersion::parse("4294967296.0.0").has_value());
    EXPECT_FALSE(corerpg::SemanticVersion::parse("0.4294967296.0").has_value());
    EXPECT_FALSE(corerpg::SemanticVersion::parse("0.0.4294967296").has_value());
}

TEST(VersionRequirementTest, IncludesMinimumAndRejectsLowerVersions) {
    const auto requirement = corerpg::VersionRequirement::create(version("1.2.0"));

    ASSERT_TRUE(requirement.has_value());
    EXPECT_TRUE(requirement->is_satisfied_by(version("1.2.0")));
    EXPECT_TRUE(requirement->is_satisfied_by(version("9.0.0")));
    EXPECT_FALSE(requirement->is_satisfied_by(version("1.1.99")));
}

TEST(VersionRequirementTest, EnforcesExclusiveMaximum) {
    const auto requirement =
        corerpg::VersionRequirement::create(version("1.2.0"), version("2.0.0"));

    ASSERT_TRUE(requirement.has_value());
    EXPECT_TRUE(requirement->is_satisfied_by(version("1.9.99")));
    EXPECT_FALSE(requirement->is_satisfied_by(version("2.0.0")));
    EXPECT_FALSE(requirement->is_satisfied_by(version("2.0.1")));
}

TEST(VersionRequirementTest, PreservesStructuredBounds) {
    const auto requirement =
        corerpg::VersionRequirement::create(version("1.2.0"), version("2.0.0"));

    ASSERT_TRUE(requirement.has_value());
    EXPECT_EQ(requirement->minimum_inclusive(), version("1.2.0"));
    ASSERT_TRUE(requirement->maximum_exclusive().has_value());
    EXPECT_EQ(*requirement->maximum_exclusive(), version("2.0.0"));

    const auto unbounded = corerpg::VersionRequirement::create(version("3.0.0"));
    ASSERT_TRUE(unbounded.has_value());
    EXPECT_FALSE(unbounded->maximum_exclusive().has_value());
}

TEST(VersionRequirementTest, RejectsEmptyOrReversedRanges) {
    EXPECT_FALSE(
        corerpg::VersionRequirement::create(version("1.0.0"), version("1.0.0")).has_value());
    EXPECT_FALSE(
        corerpg::VersionRequirement::create(version("2.0.0"), version("1.0.0")).has_value());
}

TEST(PackDependencyTest, OwnsPackIdAndRequirement) {
    auto requirement =
        corerpg::VersionRequirement::create(version("1.0.0"), version("2.0.0"));
    ASSERT_TRUE(requirement.has_value());

    const corerpg::PackDependency dependency{pack_id("example.base"),
                                              std::move(*requirement)};

    EXPECT_EQ(dependency.pack_id().value(), "example.base");
    EXPECT_TRUE(dependency.requirement().is_satisfied_by(version("1.5.0")));
    EXPECT_FALSE(dependency.requirement().is_satisfied_by(version("2.0.0")));
}

TEST(ContentManifestTest, OwnsNormativeFieldsIndependently) {
    auto requirement = corerpg::VersionRequirement::create(version("1.0.0"));
    ASSERT_TRUE(requirement.has_value());
    std::vector<corerpg::PackDependency> dependencies;
    dependencies.emplace_back(pack_id("example.base"), std::move(*requirement));

    const corerpg::ContentManifest manifest{
        pack_id("example.expansion"), version("1.4.2"), version("2.0.0"), version("0.5.0"),
        std::move(dependencies), "Example Expansion", "An informative description"};

    EXPECT_EQ(manifest.pack_id().value(), "example.expansion");
    EXPECT_EQ(manifest.pack_version(), version("1.4.2"));
    EXPECT_EQ(manifest.content_contract_version(), version("2.0.0"));
    EXPECT_EQ(manifest.minimum_core_version(), version("0.5.0"));
    ASSERT_EQ(manifest.dependencies().size(), 1U);
    EXPECT_EQ(manifest.dependencies().front().pack_id().value(), "example.base");
}

TEST(ContentManifestTest, OwnsInformativeMetadata) {
    const corerpg::ContentManifest manifest{pack_id("example.test"), version("1.0.0"),
                                            version("2.0.0"), version("0.5.0"), {},
                                            "Visible Name", "Visible description"};

    EXPECT_EQ(manifest.display_name(), "Visible Name");
    EXPECT_EQ(manifest.description(), "Visible description");
}

TEST(ContentManifestTest, MetadataAndPackVersionDoNotChangePackIdentity) {
    const corerpg::ContentManifest first{pack_id("example.test"), version("1.0.0"),
                                         version("2.0.0"), version("0.5.0"), {}, "First name"};
    const corerpg::ContentManifest second{pack_id("example.test"), version("1.1.0"),
                                          version("2.0.0"), version("0.5.0"), {}, "Other name"};

    EXPECT_EQ(first.pack_id(), second.pack_id());
    EXPECT_NE(first.pack_version(), second.pack_version());
    EXPECT_NE(first.display_name(), second.display_name());
}

} // namespace
