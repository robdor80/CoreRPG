#include <corerpg/persistence.hpp>
#include <corerpg/simulation.hpp>

#include <gtest/gtest.h>

#include <array>
#include <limits>
#include <string_view>
#include <vector>

namespace {
using namespace corerpg;

TEST(ActionDeterminismTest, FixedTraceMatchesIndependentRngAndCanonicalSaveAcrossPlatforms) {
    WorldState world;
    RandomStreams random{42};
    for (int i = 0; i < 4; ++i)
        ASSERT_TRUE(world.create_entity().is_valid());
    constexpr EntityId actor{1}, unit{2}, first{3}, second{4};
    constexpr RelationKind kind{7};
    constexpr auto maximum = std::numeric_limits<std::uint64_t>::max();
    const std::array actions{
        ActionRequest{actor, PlaceEntityAction{unit, first}},
        ActionRequest{actor, StreamRollAction{9, 0, maximum}},
        ActionRequest{unit, AddRelationAction{{unit, kind, second}}},
        ActionRequest{actor, StreamRollAction{9, 0, (std::uint64_t{1} << 63) + 1}},
        ActionRequest{second, PlaceEntityAction{second, unit}},
        ActionRequest{first, PlaceEntityAction{first, second}}, // cycle
        ActionRequest{unit, RemoveRelationAction{{unit, kind, second}}},
        ActionRequest{unit, RemoveContainmentAction{unit}},
        ActionRequest{actor, StreamRollAction{0, 77, 77}},
        ActionRequest{second, StreamRollAction{maximum, 1, 6}},
        ActionRequest{second, RemoveContainmentAction{second}},
        ActionRequest{unit, PlaceEntityAction{unit, first}},
        ActionRequest{unit, AddRelationAction{{unit, kind, second}}},
        ActionRequest{unit, AddRelationAction{{unit, kind, second}}}};
    std::array<ActionResult, actions.size()> output;
    ASSERT_EQ(Simulation::step(world, random, actions, output),
              (SimulationResult{SimulationStatus::completed, actions.size()}));
    constexpr std::array<std::uint64_t, 14> before{4, 5, 5, 6, 6, 7, 7, 8, 9, 9, 9, 10, 11, 12};
    constexpr std::array<std::uint64_t, 14> after{5, 5, 6, 6, 7, 7, 8, 9, 9, 9, 10, 11, 12, 12};
    for (std::size_t i = 0; i < output.size(); ++i) {
        EXPECT_EQ(output[i].revision_before, before[i]) << i;
        EXPECT_EQ(output[i].revision_after, after[i]) << i;
        const auto outcome = i == 5    ? ActionOutcome::rejected
                             : i == 13 ? ActionOutcome::no_change
                                       : ActionOutcome::accepted;
        EXPECT_EQ(output[i].outcome, outcome) << i;
        EXPECT_EQ(output[i].rejection,
                  i == 5 ? ActionRejection::cycle_detected : ActionRejection::none);
    }
    EXPECT_EQ(std::get<RollConsequence>(output[1].consequence).value, 3309584210414323361ULL);
    EXPECT_EQ(std::get<RollConsequence>(output[3].consequence).value, 6021639052601619331ULL);
    EXPECT_EQ(std::get<RollConsequence>(output[8].consequence), (RollConsequence{0, 77, false}));
    EXPECT_EQ(std::get<RollConsequence>(output[9].consequence).value, 3U);
    EXPECT_FALSE(random.contains_stream(0));
    ASSERT_NE(random.find_stream(9), nullptr);
    ASSERT_NE(random.find_stream(maximum), nullptr);
    EXPECT_EQ(random.find_stream(9)->state(), 0xF45EB8A6F89B72FCULL);
    EXPECT_EQ(random.find_stream(maximum)->state(), 0x867B011C09D972D2ULL);

    // Generated independently by fixtures/actions_simulation_reference.py. Same
    // literal is tested by Linux/GCC, Windows/MSVC and sanitizer CI jobs.
    constexpr std::string_view hex =
        "4352504753415645020000000100000004000000000000000c00000000000000"
        "2a00000000000000010000000000000002000000000000000000000000000000"
        "0100000000000000000000000000000000000000000000000200000000000000"
        "03000000000000000900000000000000fc729bf8a6b85ef4ffffffffffffffff"
        "d272d9091c017b86020000000000000007000000000000000400000000000000"
        "a725e45f";
    std::vector<std::uint8_t> golden;
    const auto digit = [](char c) { return c <= '9' ? c - '0' : c - 'a' + 10; };
    for (std::size_t i = 0; i < hex.size(); i += 2) {
        golden.push_back(static_cast<std::uint8_t>(16 * digit(hex[i]) + digit(hex[i + 1])));
    }
    const auto snapshot = Persistence::capture(world, random);
    const auto serialized = Persistence::serialize(snapshot);
    ASSERT_EQ(serialized.status, PersistenceStatus::success);
    EXPECT_EQ(serialized.bytes, golden);
    const auto decoded = Persistence::deserialize(golden);
    ASSERT_EQ(decoded.status, PersistenceStatus::success);
    ASSERT_TRUE(decoded.snapshot);
    EXPECT_EQ(*decoded.snapshot, snapshot);
}

} // namespace
