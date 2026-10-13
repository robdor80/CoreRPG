#include <corerpg/persistence.hpp>

#include <gtest/gtest.h>

#include <limits>

namespace {

TEST(PersistenceContractTest, EmptyCandidateHasExplicitIndependentVersions) {
    const corerpg::CampaignSnapshot snapshot;
    EXPECT_EQ(snapshot.format_version, 1U);
    EXPECT_EQ(snapshot.rng_policy, 1U);
    EXPECT_EQ(snapshot.last_entity_id, 0U);
    EXPECT_EQ(snapshot.revision, 0U);
    EXPECT_TRUE(snapshot.containment.empty());
    EXPECT_TRUE(snapshot.streams.empty());
}

TEST(PersistenceContractTest, CandidateOwnsCopiesAndFullWidthUnsignedValues) {
    corerpg::CampaignSnapshot original;
    constexpr auto maximum = std::numeric_limits<std::uint64_t>::max();
    original.last_entity_id = maximum;
    original.revision = maximum;
    original.master_seed = maximum;
    original.streams.push_back({maximum, maximum});
    auto copy = original;
    EXPECT_EQ(copy, original);
    copy.streams.front().state = 0;
    EXPECT_NE(copy, original);
    EXPECT_EQ(original.streams.front().state, maximum);
}

} // namespace
