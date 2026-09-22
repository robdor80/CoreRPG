#include <corerpg/version.hpp>

#include <gtest/gtest.h>

#include <string_view>

TEST(CoreRPGSmokeTest, PublicApiReportsLibraryVersion) {
    constexpr std::string_view expected_version{"0.1.0"};

    EXPECT_EQ(corerpg::library_version(), expected_version);
}

TEST(CoreRPGSmokeTest, SemanticCoreVersionMatchesLibraryVersion) {
    const auto parsed_library_version =
        corerpg::SemanticVersion::parse(corerpg::library_version());

    ASSERT_TRUE(parsed_library_version.has_value());
    EXPECT_EQ(corerpg::current_core_version(), *parsed_library_version);
}
