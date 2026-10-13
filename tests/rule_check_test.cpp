#include <corerpg/random_streams.hpp>
#include <corerpg/rule_check.hpp>
#include <corerpg/world_state.hpp>

#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <vector>

namespace {

using corerpg::EntityId;
using corerpg::RuleCheck;
using corerpg::RuleCheckKind;
using corerpg::RuleCheckStatus;
using corerpg::WorldState;

TEST(RuleCheckTest, ExistenceDistinguishesUnsatisfiedAndMalformedInput) {
    const WorldState world;
    EXPECT_EQ(corerpg::evaluate_rule_check(world, {RuleCheckKind::entity_exists, EntityId{1}, {}})
                  .status, RuleCheckStatus::unsatisfied);
    EXPECT_EQ(corerpg::evaluate_rule_check(world, {RuleCheckKind::entity_exists, {}, {}})
                  .status, RuleCheckStatus::malformed_check);
    EXPECT_EQ(corerpg::evaluate_rule_check(world,
                                          {RuleCheckKind::entity_exists, EntityId{1}, EntityId{2}})
                  .status, RuleCheckStatus::malformed_check);
    EXPECT_EQ(corerpg::evaluate_rule_check(
                  world, {static_cast<RuleCheckKind>(255), EntityId{1}, {}}).status,
              RuleCheckStatus::malformed_check);
}

TEST(RuleCheckTest, DirectContainmentIsNotTransitiveAndReportsMissingEntities) {
    WorldState world;
    const auto a = world.create_entity();
    const auto b = world.create_entity();
    const auto c = world.create_entity();
    ASSERT_EQ(world.place_entity(a, b), corerpg::ContainmentChange::applied);
    ASSERT_EQ(world.place_entity(b, c), corerpg::ContainmentChange::applied);

    EXPECT_TRUE(corerpg::evaluate_rule_check(world, {RuleCheckKind::entity_exists, a, {}})
                    .satisfied());
    EXPECT_TRUE(corerpg::evaluate_rule_check(world,
                                             {RuleCheckKind::direct_container_is, a, b})
                    .satisfied());
    EXPECT_EQ(corerpg::evaluate_rule_check(world,
                                          {RuleCheckKind::direct_container_is, a, c}).status,
              RuleCheckStatus::unsatisfied);
    EXPECT_EQ(corerpg::evaluate_rule_check(world,
                                          {RuleCheckKind::entity_uncontained, a, {}}).status,
              RuleCheckStatus::unsatisfied);
    EXPECT_TRUE(corerpg::evaluate_rule_check(world,
                                             {RuleCheckKind::entity_uncontained, c, {}})
                    .satisfied());

    EXPECT_EQ(corerpg::evaluate_rule_check(
                  world, {RuleCheckKind::direct_container_is, EntityId{99}, b}).status,
              RuleCheckStatus::subject_not_found);
    EXPECT_EQ(corerpg::evaluate_rule_check(
                  world, {RuleCheckKind::direct_container_is, a, EntityId{99}}).status,
              RuleCheckStatus::related_not_found);
    EXPECT_EQ(corerpg::evaluate_rule_check(
                  world, {RuleCheckKind::direct_container_is, a, {}}).status,
              RuleCheckStatus::malformed_check);
    EXPECT_EQ(corerpg::evaluate_rule_check(
                  world, {RuleCheckKind::entity_uncontained, EntityId{99}, {}}).status,
              RuleCheckStatus::subject_not_found);
    EXPECT_EQ(corerpg::evaluate_rule_check(
                  world, {RuleCheckKind::entity_uncontained, c, b}).status,
              RuleCheckStatus::malformed_check);
}

TEST(RuleCheckTest, OrderedAllOfFindsFirstUnmetAndAnEmptyBatchSucceeds) {
    WorldState world;
    const auto a = world.create_entity();
    const auto b = world.create_entity();
    ASSERT_EQ(world.place_entity(a, b), corerpg::ContainmentChange::applied);
    const std::array<RuleCheck, 4> checks{{
        {RuleCheckKind::entity_exists, a, {}},
        {RuleCheckKind::direct_container_is, a, b},
        {RuleCheckKind::entity_uncontained, a, {}},
        {RuleCheckKind::entity_exists, EntityId{99}, {}},
    }};
    const auto result = corerpg::evaluate_rule_checks(world, checks);
    EXPECT_FALSE(result.satisfied());
    EXPECT_EQ(result.status, RuleCheckStatus::unsatisfied);
    EXPECT_EQ(result.first_non_satisfied_index, 2U);

    const std::array<RuleCheck, 0> empty{};
    const auto empty_result = corerpg::evaluate_rule_checks(world, empty);
    EXPECT_TRUE(empty_result.satisfied());
    EXPECT_EQ(empty_result.first_non_satisfied_index, 0U);

    const auto success = corerpg::evaluate_rule_checks(world, std::span{checks}.first(2));
    EXPECT_TRUE(success.satisfied());
    EXPECT_EQ(success.first_non_satisfied_index, 2U);
}

TEST(RuleCheckTest, EvaluationIsReadOnlyAndDoesNotTouchRngOrCopies) {
    WorldState world;
    const auto a = world.create_entity();
    const auto b = world.create_entity();
    ASSERT_EQ(world.place_entity(a, b), corerpg::ContainmentChange::applied);
    auto copy = world;
    const auto world_revision = world.revision();
    const auto copy_revision = copy.revision();
    corerpg::RandomStreams streams{100};
    const auto rng_state = streams.stream(9).state();

    const RuleCheck check{RuleCheckKind::direct_container_is, a, b};
    for (int i = 0; i < 100; ++i) {
        EXPECT_TRUE(corerpg::evaluate_rule_check(world, check).satisfied());
    }
    EXPECT_EQ(world.revision(), world_revision);
    EXPECT_EQ(copy.revision(), copy_revision);
    EXPECT_EQ(streams.stream(9).state(), rng_state);

    ASSERT_EQ(copy.remove_containment(a), corerpg::ContainmentChange::applied);
    EXPECT_EQ(corerpg::evaluate_rule_check(copy, check).status,
              RuleCheckStatus::unsatisfied);
    EXPECT_TRUE(corerpg::evaluate_rule_check(world, check).satisfied());
}

}  // namespace
