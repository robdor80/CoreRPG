#include <corerpg/random_streams.hpp>
#include <corerpg/roll_action.hpp>
#include <corerpg/world_state.hpp>

#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <limits>

namespace {

using corerpg::RandomStreams;

TEST(RandomStreamsTest, CanonicalInitialStatesIncludeZeroAndUnsignedLimits) {
    RandomStreams zero{0};
    EXPECT_EQ(zero.master_seed(), 0U);
    EXPECT_EQ(zero.stream(0).state(), 0x9380A04DD36E21E2ULL);
    EXPECT_EQ(zero.stream(1).state(), 0x3878BC9C1137404EULL);
    EXPECT_EQ(zero.stream(2).state(), 0xA3D09CD9452CE4C9ULL);

    RandomStreams answer{42};
    EXPECT_EQ(answer.stream(1).state(), 0x4AF3904D102DEB04ULL);
    EXPECT_EQ(answer.stream(17).state(), 0x18060C1E8E77A6BBULL);

    RandomStreams maximum{std::numeric_limits<std::uint64_t>::max()};
    EXPECT_EQ(maximum.stream(std::numeric_limits<std::uint64_t>::max()).state(),
              0x87B239380834BA83ULL);
}

TEST(RandomStreamsTest, StreamsDoNotDependOnCreationOrderOrOtherDraws) {
    RandomStreams first{142};
    RandomStreams reordered{142};
    auto& a = first.stream(10);
    auto& b = first.stream(20);
    auto& reordered_b = reordered.stream(20);
    auto& reordered_a = reordered.stream(10);
    for (int index = 0; index < 30; ++index) {
        EXPECT_EQ(a.next(), reordered_a.next());
        if (index % 3 == 0) {
            EXPECT_EQ(b.next(), reordered_b.next());
        }
    }
    EXPECT_EQ(a.state(), reordered_a.state());
    EXPECT_EQ(b.state(), reordered_b.state());
    EXPECT_EQ(first.stream_count(), 2U);
}

TEST(RandomStreamsTest, ObservingAnAbsentStreamNeverCreatesOrConsumesIt) {
    RandomStreams streams{0};
    EXPECT_EQ(streams.stream_count(), 0U);
    EXPECT_FALSE(streams.contains_stream(7));
    EXPECT_EQ(streams.find_stream(7), nullptr);
    EXPECT_EQ(streams.stream_count(), 0U);
    auto& active = streams.stream(7);
    const auto seed = active.state();
    EXPECT_TRUE(streams.contains_stream(7));
    EXPECT_EQ(streams.find_stream(7), &active);
    EXPECT_EQ(&streams.stream(7), &active);
    EXPECT_EQ(active.state(), seed);
    EXPECT_EQ(streams.stream_count(), 1U);
}

TEST(RandomStreamsTest, CopiesPreserveThenIndependentlyAdvanceAllStates) {
    RandomStreams original{654321};
    static_cast<void>(original.stream(7).next());
    static_cast<void>(original.stream(7).next());
    static_cast<void>(original.stream(9).next());
    auto copy = original;
    EXPECT_EQ(copy.master_seed(), original.master_seed());
    EXPECT_EQ(copy.stream(7).state(), original.stream(7).state());
    EXPECT_EQ(copy.stream(9).state(), original.stream(9).state());
    const auto copied_state = copy.stream(7).state();
    static_cast<void>(original.stream(7).next());
    EXPECT_EQ(copy.stream(7).state(), copied_state);
    EXPECT_NE(copy.stream(7).state(), original.stream(7).state());
    EXPECT_EQ(copy.stream(9).next(), original.stream(9).next());
}

TEST(RandomStreamsTest, ReferenceStaysValidWhenNewStreamsAreAdded) {
    RandomStreams streams{50};
    auto& first = streams.stream(1);
    auto* initial_address = &first;
    const auto initial_state = first.state();
    for (std::uint64_t index = 2; index != 120; ++index) {
        static_cast<void>(streams.stream(index));
    }
    EXPECT_EQ(&streams.stream(1), initial_address);
    EXPECT_EQ(first.state(), initial_state);
}

TEST(RandomStreamsTest, RollActionConsumesOnlyItsExplicitStream) {
    corerpg::WorldState world;
    const auto entity = world.create_entity();
    RandomStreams streams{123};
    RandomStreams control{123};
    const auto untouched_before = streams.stream(2).state();
    const auto world_before = world.revision();

    const auto result = corerpg::resolve_roll(world, streams.stream(1), {entity, 1, 100});
    const auto matching = corerpg::resolve_roll(world, control.stream(1), {entity, 1, 100});
    ASSERT_TRUE(result.succeeded());
    EXPECT_EQ(result.value, matching.value);
    EXPECT_EQ(streams.stream(1).state(), control.stream(1).state());
    EXPECT_EQ(streams.stream(2).state(), untouched_before);
    EXPECT_EQ(world.revision(), world_before);

    const auto first_state = streams.stream(1).state();
    EXPECT_EQ(corerpg::resolve_roll(world, streams.stream(1),
                                   {corerpg::EntityId{999}, 1, 100}).status,
              corerpg::RollStatus::entity_not_found);
    EXPECT_EQ(corerpg::resolve_roll(world, streams.stream(1),
                                   {entity, 100, 1}).status,
              corerpg::RollStatus::invalid_range);
    EXPECT_EQ(streams.stream(1).state(), first_state);
}

}  // namespace
