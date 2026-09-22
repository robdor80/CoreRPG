#include <corerpg/random.hpp>

#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <limits>

namespace {

TEST(DeterministicRandomTest, SameSeedProducesSameSequence) {
    corerpg::DeterministicRandom first{42};
    corerpg::DeterministicRandom second{42};

    for (std::uint32_t index = 0; index < 32U; ++index) {
        EXPECT_EQ(first.next(), second.next());
    }
}

TEST(DeterministicRandomTest, DifferentSeedsProduceDifferentSequences) {
    corerpg::DeterministicRandom first{1};
    corerpg::DeterministicRandom second{2};
    bool found_difference = false;

    for (std::uint32_t index = 0; index < 32U; ++index) {
        const auto first_value = first.next();
        const auto second_value = second.next();
        found_difference = found_difference || first_value != second_value;
    }

    EXPECT_TRUE(found_difference);
}

TEST(DeterministicRandomTest, SeedZeroMatchesGoldenVector) {
    corerpg::DeterministicRandom random{0};
    constexpr std::array<std::uint64_t, 5> expected{
        0xE220A8397B1DCDAFULL,
        0x6E789E6AA1B965F4ULL,
        0x06C45D188009454FULL,
        0xF88BB8A8724C81ECULL,
        0x1B39896A51A8749BULL,
    };

    for (const auto value : expected) {
        EXPECT_EQ(random.next(), value);
    }
}

TEST(DeterministicRandomTest, UniformClosedStaysInsideRange) {
    corerpg::DeterministicRandom random{123456789};

    for (std::uint32_t index = 0; index < 1'000U; ++index) {
        const auto value = random.uniform_closed(1, 100);
        ASSERT_TRUE(value.has_value());
        EXPECT_GE(*value, 1U);
        EXPECT_LE(*value, 100U);
    }
}

TEST(DeterministicRandomTest, FixedRangeDoesNotAdvanceState) {
    corerpg::DeterministicRandom random{7};
    const auto initial_state = random.state();

    const auto value = random.uniform_closed(25, 25);

    ASSERT_TRUE(value.has_value());
    EXPECT_EQ(*value, 25U);
    EXPECT_EQ(random.state(), initial_state);
}

TEST(DeterministicRandomTest, InvalidRangeDoesNotAdvanceState) {
    corerpg::DeterministicRandom random{7};
    const auto initial_state = random.state();

    EXPECT_FALSE(random.uniform_closed(100, 1).has_value());
    EXPECT_EQ(random.state(), initial_state);
}

TEST(DeterministicRandomTest, FullRangeConsumesExactlyOneValue) {
    corerpg::DeterministicRandom ranged{99};
    corerpg::DeterministicRandom direct{99};

    const auto value = ranged.uniform_closed(0, std::numeric_limits<std::uint64_t>::max());

    ASSERT_TRUE(value.has_value());
    EXPECT_EQ(*value, direct.next());
    EXPECT_EQ(ranged.state(), direct.state());
}

TEST(DeterministicRandomTest, SavedStateCanResumeSequence) {
    corerpg::DeterministicRandom original{314159};
    static_cast<void>(original.next());
    corerpg::DeterministicRandom resumed{original.state()};

    EXPECT_EQ(resumed.next(), original.next());
    EXPECT_EQ(resumed.state(), original.state());
}

} // namespace
