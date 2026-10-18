#include <corerpg/persistence.hpp>
#include <corerpg/visibility.hpp>
#include <array>

#include "entity_structure_helpers.hpp"

#include <gtest/gtest.h>

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
constexpr EntityId observer{1}, target{2}, group{3}, extra{4};

class KnowledgeAllocationTest : public ::testing::Test {
  protected:
    WorldState source;
    WorldState destination;
    RandomStreams random{42};
    RandomStreams destination_random{99};
    std::optional<CatalogBuildResult> catalog;
    CampaignSnapshot original;
    CampaignSnapshot destination_before;
    std::vector<std::uint8_t> bytes;
    ObserverView output;
    ObserverView output_before;

    void SetUp() override {
        catalog.emplace(corerpg_test::structural_catalog());
        for (int i = 0; i < 4; ++i) static_cast<void>(source.create_entity());
        ASSERT_EQ(source.learn_fact(observer, {target}), KnowledgeChange::applied);
        ASSERT_EQ(source.learn_fact(observer, {group}), KnowledgeChange::applied);
        ASSERT_EQ(source.learn_fact(observer, {target, FactKind::definition}), KnowledgeChange::applied);
        ASSERT_EQ(source.bind_definition(target, *catalog->catalog(), *DefinitionId::parse("example.structural:unit")), DefinitionBindingChange::applied);
        ASSERT_EQ(source.declare_population(group, 10), PopulationChange::applied);
        ASSERT_EQ(source.place_entity(target, group), ContainmentChange::applied);
        ASSERT_EQ(source.learn_fact(observer, {target, FactKind::containment}), KnowledgeChange::applied);
        ASSERT_EQ(source.bind_definition(group, *catalog->catalog(), *DefinitionId::parse("example.structural:collective")), DefinitionBindingChange::applied);
        ASSERT_EQ(source.learn_fact(observer, {group, FactKind::definition}), KnowledgeChange::applied);
        WorldState compact{source};
        source = std::move(compact);
        static_cast<void>(random.stream(7).next());
        ASSERT_TRUE(destination.create_entity().is_valid());
        static_cast<void>(destination_random.stream(8).next());
        original = Persistence::capture(source, random);
        destination_before = Persistence::capture(destination, destination_random);
        bytes = Persistence::serialize(original).bytes;
        output = Visibility::project(source, observer);
        output_before = output;
    }

    template <typename Operation>
    void fail_each(Operation operation, bool changes_source = false, bool replaces_destination = false,
                   bool changes_output = false, bool replaces_world_only = false) {
        const auto* knowledge_reference = source.knowledge_of(observer).data();
        const auto* rng_reference = destination_random.find_stream(8);
        bool completed = false;
        std::size_t failures = 0;
        for (std::size_t index = 0; index < 2048; ++index) {
            bool threw = false;
            bool okay = false;
            {
                AllocationFailure failure{index};
                try { okay = operation(); }
                catch (const std::bad_alloc&) { threw = true; }
            }
            ASSERT_EQ(threw, allocation_failed) << index;
            if (threw) {
                ++failures;
                EXPECT_EQ(Persistence::capture(source, random), original) << index;
                EXPECT_EQ(source.knowledge_of(observer).data(), knowledge_reference);
                EXPECT_EQ(Persistence::capture(destination, destination_random), destination_before);
                EXPECT_EQ(destination_random.find_stream(8), rng_reference);
                EXPECT_EQ(output, output_before);
                continue;
            }
            EXPECT_TRUE(okay);
            if (!changes_source) EXPECT_EQ(Persistence::capture(source, random), original);
            EXPECT_EQ(Persistence::capture(destination, replaces_world_only ? random : destination_random),
                      (replaces_destination || replaces_world_only) ? original : destination_before);
            if (!changes_output) EXPECT_EQ(output, output_before);
            completed = true;
            break;
        }
        EXPECT_TRUE(completed);
        EXPECT_GT(failures, 0U);
        RecordProperty("injected_allocation_failures", static_cast<int>(failures));
    }
};

TEST_F(KnowledgeAllocationTest, LearningExistencePreservesAllStateAndReferencesAtEveryFailure) {
    fail_each([&] { return source.learn_fact(observer, {extra}) == KnowledgeChange::applied; }, true);
}
TEST_F(KnowledgeAllocationTest, RefreshingDefinitionStagesTextBeforeReplacingMemory) {
    fail_each([&] { return source.learn_fact(observer, {target, FactKind::definition}) == KnowledgeChange::applied; }, true);
}
TEST_F(KnowledgeAllocationTest, NewDefinitionAndTableInsertionPublishTogetherAtEveryFailure) {
    ASSERT_EQ(source.revoke_fact(observer, {target, FactKind::definition}), KnowledgeChange::applied);
    WorldState compact{source};
    source = std::move(compact);
    original = Persistence::capture(source, random);
    bytes = Persistence::serialize(original).bytes;
    fail_each([&] { return source.learn_fact(observer, {target, FactKind::definition}) == KnowledgeChange::applied; }, true);
}
TEST_F(KnowledgeAllocationTest, CopyAssignmentPreservesBothWorldsAtEveryFailure) {
    fail_each([&] { destination = source; return true; }, false, false, false, true);
}
TEST_F(KnowledgeAllocationTest, CapturePreservesSourceAtEveryFailure) {
    fail_each([&] { return Persistence::capture(source, random) == original; });
}
TEST_F(KnowledgeAllocationTest, SerializePreservesSnapshotAtEveryFailure) {
    fail_each([&] { return Persistence::serialize(original).bytes == bytes; });
}
TEST_F(KnowledgeAllocationTest, DeserializeNeverPublishesPartialKnowledge) {
    fail_each([&] { return Persistence::deserialize(bytes).status == PersistenceStatus::success; });
}
TEST_F(KnowledgeAllocationTest, RestorePreservesWorldAndRngAtEveryFailure) {
    fail_each([&] { return Persistence::restore(original, destination, destination_random, {}, catalog->catalog()) == PersistenceStatus::success; }, false, true);
}
TEST_F(KnowledgeAllocationTest, LoadPreservesWorldAndRngAtEveryFailure) {
    fail_each([&] { return Persistence::load(bytes, destination, destination_random, {}, catalog->catalog()) == PersistenceStatus::success; }, false, true);
}
TEST_F(KnowledgeAllocationTest, ProjectionAssignmentPreservesPreviousViewAndWorldAtEveryFailure) {
    const std::array grants{FactKey{extra}, FactKey{target, FactKind::definition}};
    fail_each([&] { output = Visibility::project(source, observer, grants); return output.knows(extra); }, false, false, true);
}
TEST_F(KnowledgeAllocationTest, ViewCopyAssignmentHasStrongExceptionGuarantee) {
    ObserverView empty;
    const auto nonempty = output;
    output = empty;
    output_before = empty;
    fail_each([&] { output = nonempty; return output == nonempty; }, false, false, true);
}
TEST_F(KnowledgeAllocationTest, QueriesNoOpsAndRevocationNeedNoAllocationOrRng) {
    bool okay = false;
    {
        AllocationFailure failure{0};
        okay = source.learn_fact(observer, {target}) == KnowledgeChange::no_change &&
            source.learn_fact(observer, {group, FactKind::definition}) == KnowledgeChange::no_change &&
            source.revoke_fact(observer, {extra}) == KnowledgeChange::no_change &&
            source.learn_fact({}, {target}) == KnowledgeChange::observer_not_found &&
            source.knowledge_of(observer).size() == 5 && output.knows(target) &&
            !output.knows(extra) && output.next_entity_id() == target &&
            source.revoke_fact(observer, {group}) == KnowledgeChange::applied;
    }
    EXPECT_TRUE(okay);
    EXPECT_FALSE(allocation_failed);
    EXPECT_EQ(Persistence::capture(source, random).streams, original.streams);
    EXPECT_EQ(source.revision(), original.revision + 1);
}
} // namespace

