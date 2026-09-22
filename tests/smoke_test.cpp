#include <corerpg/version.hpp>

#include <gtest/gtest.h>

#include <string_view>

TEST(CoreRPGSmokeTest, PublicApiReportsLibraryVersion) {
    constexpr std::string_view expected_version{"0.1.0"};

    EXPECT_EQ(corerpg::library_version(), expected_version);
}
