#include <corerpg/persistence.hpp>
#include <corerpg/simulation.hpp>

#include <gtest/gtest.h>

#include <array>
#include <cstdlib>
#include <limits>
#include <new>

namespace {

thread_local std::size_t allocations_before_failure = std::numeric_limits<std::size_t>::max();
thread_local bool allocation_failed = false;

class AllocationFailure final {
  public:
    explicit AllocationFailure(std::size_t index) noexcept {
        allocation_failed = false;
        allocations_before_failure = index;
    }
    ~AllocationFailure() {
        allocations_before_failure = std::numeric_limits<std::size_t>::max();
    }
};

} // namespace

// Isolated executable, ordinary allocation only. Never armed during assertions.
void* operator new(std::size_t size) {
    if (allocations_before_failure != std::numeric_limits<std::size_t>::max()) {
        if (allocations_before_failure == 0) {
            allocations_before_failure = std::numeric_limits<std::size_t>::max();
            allocation_failed = true;
            throw std::bad_alloc{};
        }
        --allocations_before_failure;
    }
    if (auto* allocation = std::malloc(size == 0 ? 1 : size)) {
        return allocation;
    }
    throw std::bad_alloc{};
}
void* operator new[](std::size_t size) {
    return ::operator new(size);
}
void operator delete(void* allocation) noexcept {
    std::free(allocation);
}
void operator delete[](void* allocation) noexcept {
    std::free(allocation);
}
void operator delete(void* allocation, std::size_t) noexcept {
    std::free(allocation);
}
void operator delete[](void* allocation, std::size_t) noexcept {
    std::free(allocation);
}

// The runner may use nothrow temporary buffers. Route them through the same
// replacement so sanitizer allocation/deallocation families remain matched.
void* operator new(std::size_t size, const std::nothrow_t&) noexcept {
    try {
        return ::operator new(size);
    } catch (const std::bad_alloc&) {
        return nullptr;
    }
}
void* operator new[](std::size_t size, const std::nothrow_t&) noexcept {
    return ::operator new(size, std::nothrow);
}
void operator delete(void* allocation, const std::nothrow_t&) noexcept {
    std::free(allocation);
}
void operator delete[](void* allocation, const std::nothrow_t&) noexcept {
    std::free(allocation);
}

namespace {
using namespace corerpg;
constexpr EntityId actor{1}, unit{2}, first{3}, second{4}, observer{5};
constexpr RelationKind kind{7};

class ActionAllocationTest : public ::testing::Test {
  protected:
    WorldState world;
    RandomStreams random{42};
    void SetUp() override {
        for (int i = 0; i < 5; ++i)
            ASSERT_TRUE(world.create_entity().is_valid());
        ASSERT_EQ(world.place_entity(unit, first), ContainmentChange::applied);
        ASSERT_EQ(world.learn_fact(observer, {unit, FactKind::existence, {}, {}}),
                  KnowledgeChange::applied);
        static_cast<void>(random.stream(7).next());
    }

    void fail_each(ActionOperation operation) {
        const auto before = Persistence::capture(world, random);
        const auto* knowledge_reference = world.knowledge_of(observer).data();
        const auto* random_reference = random.find_stream(7);
        bool completed = false;
        std::size_t failures = 0;
        for (std::size_t index = 0; index < 64; ++index) {
            ActionResult output;
            output.revision_before = 999;
            const auto sentinel = output;
            bool threw = false;
            {
                AllocationFailure failure{index};
                try {
                    output = ActionEngine::process(world, random, {actor, operation});
                } catch (const std::bad_alloc&) {
                    threw = true;
                }
            }
            ASSERT_EQ(threw, allocation_failed) << index;
            if (threw) {
                ++failures;
                EXPECT_EQ(output, sentinel);
                EXPECT_EQ(Persistence::capture(world, random), before);
                EXPECT_EQ(world.knowledge_of(observer).data(), knowledge_reference);
                EXPECT_EQ(random.find_stream(7), random_reference);
            } else {
                EXPECT_EQ(output.outcome, ActionOutcome::accepted);
                completed = true;
                break;
            }
        }
        EXPECT_TRUE(completed);
        EXPECT_GT(failures, 0U);
        RecordProperty("injected_allocation_failures", static_cast<int>(failures));
    }
};

TEST_F(ActionAllocationTest, ContainmentInsertionPreservesEntireWorldAndRngAtEveryFailure) {
    WorldState compact{world};
    world = std::move(compact);
    fail_each(PlaceEntityAction{actor, first});
}

TEST_F(ActionAllocationTest, RelationInsertionPreservesEntireWorldAndRngAtEveryFailure) {
    fail_each(AddRelationAction{{unit, kind, second}});
}

TEST_F(ActionAllocationTest, OpeningRollStreamNeverPublishesAStreamWithoutItsResult) {
    fail_each(StreamRollAction{17, 0, (std::uint64_t{1} << 63) + 1});
}

TEST_F(ActionAllocationTest, RejectionsNoOpsReassignmentRemovalAndExistingRollDoNotAllocate) {
    ASSERT_EQ(world.add_relation({unit, kind, first}), RelationChange::applied);
    const auto before = Persistence::capture(world, random);
    const std::array checks{RuleCheck{RuleCheckKind::entity_uncontained, unit}};
    bool okay = false;
    {
        AllocationFailure failure{0};
        okay =
            ActionEngine::process(world, random, {actor, PlaceEntityAction{unit, first}}).outcome ==
                ActionOutcome::no_change &&
            ActionEngine::process(world, random, {actor, AddRelationAction{{unit, kind, first}}})
                    .outcome == ActionOutcome::no_change &&
            ActionEngine::process(world, random, {actor, StreamRollAction{17, 9, 0}}).rejection ==
                ActionRejection::invalid_range &&
            ActionEngine::process(world, random, {actor, StreamRollAction{17, 5, 5}}).outcome ==
                ActionOutcome::accepted &&
            ActionEngine::process(world, random, {{}, StreamRollAction{17, 0, 10}}).rejection ==
                ActionRejection::actor_not_found &&
            ActionEngine::process(world, random, {actor, StreamRollAction{17, 0, 10}, {}, checks})
                    .rejection == ActionRejection::precondition_failed &&
            ActionEngine::process(world, random,
                                  {actor, StreamRollAction{17, 0, 10}, world.revision() - 1})
                    .rejection == ActionRejection::stale_revision &&
            ActionEngine::process(world, random, {actor, PlaceEntityAction{unit, second}})
                    .outcome == ActionOutcome::accepted &&
            ActionEngine::process(world, random, {actor, RemoveContainmentAction{unit}}).outcome ==
                ActionOutcome::accepted &&
            ActionEngine::process(world, random, {actor, RemoveRelationAction{{unit, kind, first}}})
                    .outcome == ActionOutcome::accepted &&
            ActionEngine::process(world, random, {actor, StreamRollAction{7, 0, 100}}).outcome ==
                ActionOutcome::accepted;
    }
    EXPECT_TRUE(okay);
    EXPECT_FALSE(allocation_failed);
    EXPECT_EQ(world.revision(), before.revision + 3);
    EXPECT_FALSE(random.contains_stream(17));
    EXPECT_EQ(Persistence::capture(world, random).knowledge, before.knowledge);
}

TEST_F(ActionAllocationTest, EveryInterruptedStepPublishesExactPrefixAndResumesWithoutDuplicates) {
    const std::array actions{
        ActionRequest{actor, PlaceEntityAction{unit, second}}, // reassignment: no allocation
        ActionRequest{unit, StreamRollAction{7, 0, 100}},      // existing stream: no allocation
        ActionRequest{unit, AddRelationAction{{unit, kind, second}}},
        ActionRequest{unit, StreamRollAction{17, 0, 100}},
        ActionRequest{actor, PlaceEntityAction{actor, first}},
        ActionRequest{{}, RemoveContainmentAction{unit}}}; // rejection still completes
    WorldState expected{world};
    RandomStreams expected_random{random};
    std::array<ActionResult, actions.size()> expected_output;
    ASSERT_EQ(Simulation::step(expected, expected_random, actions, expected_output).processed,
              actions.size());
    const auto final = Persistence::capture(expected, expected_random);
    std::size_t failures = 0;
    bool completed = false;
    for (std::size_t index = 0; index < 128; ++index) {
        WorldState candidate{world};
        RandomStreams candidate_random{random};
        std::array<ActionResult, actions.size()> output;
        for (auto& result : output)
            result.revision_before = 999;
        const auto untouched = output;
        SimulationResult progress;
        {
            AllocationFailure failure{index};
            progress = Simulation::step(candidate, candidate_random, actions, output);
        }
        if (allocation_failed) {
            ++failures;
            ASSERT_EQ(progress.status, SimulationStatus::allocation_failure);
            ASSERT_GE(progress.processed, 2U);
            ASSERT_LT(progress.processed, actions.size());
            WorldState prefix{world};
            RandomStreams prefix_random{random};
            std::array<ActionResult, actions.size()> prefix_output;
            ASSERT_EQ(Simulation::step(prefix, prefix_random,
                                       std::span{actions}.first(progress.processed), prefix_output)
                          .processed,
                      progress.processed);
            EXPECT_EQ(Persistence::capture(candidate, candidate_random),
                      Persistence::capture(prefix, prefix_random));
            for (std::size_t i = 0; i < progress.processed; ++i)
                EXPECT_EQ(output[i], expected_output[i]);
            for (std::size_t i = progress.processed; i < output.size(); ++i)
                EXPECT_EQ(output[i], untouched[i]);
            const auto resumed = Simulation::step(candidate, candidate_random,
                                                  std::span{actions}.subspan(progress.processed),
                                                  std::span{output}.subspan(progress.processed));
            EXPECT_EQ(resumed.status, SimulationStatus::completed);
            EXPECT_EQ(resumed.processed, actions.size() - progress.processed);
            EXPECT_EQ(Persistence::capture(candidate, candidate_random), final);
            EXPECT_EQ(output, expected_output);
        } else {
            EXPECT_EQ(progress.status, SimulationStatus::completed);
            EXPECT_EQ(progress.processed, actions.size());
            EXPECT_EQ(output, expected_output);
            EXPECT_EQ(Persistence::capture(candidate, candidate_random), final);
            completed = true;
            break;
        }
    }
    EXPECT_TRUE(completed);
    EXPECT_GT(failures, 0U);
    RecordProperty("injected_allocation_failures", static_cast<int>(failures));
}

TEST_F(ActionAllocationTest, BudgetAndOutputFailuresRequireNoAllocationOrMutation) {
    const std::array actions{ActionRequest{actor, StreamRollAction{17, 0, 100}}};
    const auto before = Persistence::capture(world, random);
    std::array<ActionResult, 1> output;
    const auto untouched = output;
    bool okay = false;
    {
        AllocationFailure failure{0};
        okay = Simulation::step(world, random, actions, output, {0}).status ==
                   SimulationStatus::action_limit_exceeded &&
               Simulation::step(world, random, actions, {}).status ==
                   SimulationStatus::insufficient_output;
    }
    EXPECT_TRUE(okay);
    EXPECT_FALSE(allocation_failed);
    EXPECT_EQ(output, untouched);
    EXPECT_EQ(Persistence::capture(world, random), before);
}

} // namespace
