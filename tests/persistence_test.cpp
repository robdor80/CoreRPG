#include <corerpg/persistence.hpp>

#include <gtest/gtest.h>

#include <limits>
#include <string_view>

#include "../src/persistence_internal.hpp"

namespace {

using corerpg::CampaignSnapshot;
using corerpg::EntityId;
using corerpg::Persistence;
using corerpg::PersistenceStatus;

std::vector<std::uint8_t> from_hex(std::string_view hex) {
    std::vector<std::uint8_t> bytes;
    const auto digit = [](char c) { return c <= '9' ? c - '0' : c - 'a' + 10; };
    for (std::size_t i = 0; i < hex.size(); i += 2) {
        bytes.push_back(static_cast<std::uint8_t>(digit(hex[i]) * 16 + digit(hex[i + 1])));
    }
    return bytes;
}

CampaignSnapshot representative_snapshot() {
    CampaignSnapshot snapshot;
    snapshot.last_entity_id = 3;
    snapshot.revision = 5;
    snapshot.master_seed = 42;
    snapshot.containment = {{EntityId{1}, EntityId{3}}, {EntityId{2}, EntityId{3}}};
    snapshot.streams = {{0, 0}, {UINT64_MAX, UINT64_MAX}};
    return snapshot;
}

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

TEST(PersistenceExportTest, CaptureIsCompleteCanonicalAndDoesNotConsumeOrMutate) {
    corerpg::WorldState world;
    for (int i = 0; i < 4; ++i) {
        static_cast<void>(world.create_entity());
    }
    ASSERT_EQ(world.place_entity(EntityId{3}, EntityId{4}), corerpg::ContainmentChange::applied);
    ASSERT_EQ(world.place_entity(EntityId{1}, EntityId{3}), corerpg::ContainmentChange::applied);
    corerpg::RandomStreams random{42};
    static_cast<void>(random.stream(UINT64_MAX).next());
    static_cast<void>(random.stream(0).next());
    const auto before = Persistence::capture(world, random);
    EXPECT_EQ(before.last_entity_id, 4U);
    EXPECT_EQ(before.revision, 6U);
    EXPECT_EQ(before.master_seed, 42U);
    EXPECT_EQ(before.containment, (std::vector<corerpg::ContainmentSnapshot>{
        {EntityId{1}, EntityId{3}}, {EntityId{3}, EntityId{4}}}));
    EXPECT_EQ(before.streams, (std::vector<corerpg::RandomStreamSnapshot>{
        {0, random.find_stream(0)->state()}, {UINT64_MAX, random.find_stream(UINT64_MAX)->state()}}));
    const auto first = Persistence::serialize(before);
    ASSERT_EQ(first.status, PersistenceStatus::success);
    EXPECT_EQ(first.bytes, Persistence::serialize(Persistence::capture(world, random)).bytes);
    EXPECT_EQ(before, Persistence::capture(world, random));
    EXPECT_FALSE(random.contains_stream(1));
}

TEST(PersistenceExportTest, ChecksumMatchesIndependentCastagnoliVector) {
    constexpr std::array<std::uint8_t, 9> input{'1','2','3','4','5','6','7','8','9'};
    EXPECT_EQ(corerpg::detail::save_checksum(input), 0xE3069283U);
}

TEST(PersistenceExportTest, EmptySnapshotMatchesPortableGoldenBytes) {
    const auto encoded = Persistence::serialize(CampaignSnapshot{});
    ASSERT_EQ(encoded.status, PersistenceStatus::success);
    EXPECT_EQ(encoded.bytes, from_hex(
        "4352504753415645010000000100000000000000000000000000000000000000"
        "00000000000000000000000000000000000000000000000063dc3e1f"));
}

TEST(PersistenceExportTest, PopulatedSnapshotMatchesIndependentPortableGoldenBytes) {
    const auto encoded = Persistence::serialize(representative_snapshot());
    ASSERT_EQ(encoded.status, PersistenceStatus::success);
    EXPECT_EQ(encoded.bytes, from_hex(
        "4352504753415645010000000100000003000000000000000500000000000000"
        "2a00000000000000020000000000000002000000000000000100000000000000"
        "0300000000000000020000000000000003000000000000000000000000000000"
        "0000000000000000ffffffffffffffffffffffffffffffffa54c8b86"));
}

TEST(PersistenceExportTest, RejectsInvalidCandidatesInsteadOfSerializingThem) {
    auto snapshot = representative_snapshot();
    snapshot.containment.push_back(snapshot.containment.back());
    snapshot.revision = 6;
    const auto encoded = Persistence::serialize(snapshot);
    EXPECT_EQ(encoded.status, PersistenceStatus::noncanonical_links);
    EXPECT_TRUE(encoded.bytes.empty());
    snapshot = representative_snapshot();
    snapshot.format_version = 99;
    EXPECT_EQ(Persistence::serialize(snapshot).status, PersistenceStatus::unsupported_version);
}

TEST(PersistenceExportTest, MaximumImplicitIdentityRangeNeedsNoEntityAllocation) {
    CampaignSnapshot snapshot;
    snapshot.last_entity_id = UINT64_MAX;
    snapshot.revision = UINT64_MAX;
    snapshot.master_seed = UINT64_MAX;
    EXPECT_EQ(Persistence::serialize(snapshot).bytes.size(), 60U);
    snapshot.containment.push_back({EntityId{1}, EntityId{UINT64_MAX}});
    EXPECT_EQ(Persistence::serialize(snapshot).status, PersistenceStatus::invalid_world);
}

} // namespace
