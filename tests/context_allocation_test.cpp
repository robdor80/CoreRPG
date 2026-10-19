#include <corerpg/persistence.hpp>
#include <corerpg/context_builder.hpp>
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
TEST(ContextAllocationTest, SelectionNeverAllocatesEvenWhenReplacingOwnedTextOrRejectingCapacity) {
    WorldState world;
    RandomStreams random{77};
    const auto actor = world.create_entity();
    const auto target = world.create_entity();
    auto catalog = corerpg_test::structural_catalog();
    ASSERT_EQ(world.bind_definition(target, *catalog.catalog(), *DefinitionId::parse("example.structural:unit")), DefinitionBindingChange::applied);
    const std::array grants{FactKey{target}, FactKey{target, FactKind::definition}};
    const auto view = Visibility::project(world, actor, grants);
    std::array<VisibleFact, 8> output;
    const auto before = Persistence::capture(world, random);
    ContextResult complete, truncated, rejected;
    {
        AllocationFailure fail{0};
        complete = ContextBuilder::build(view, {}, output);
        complete = ContextBuilder::build(view, {}, output); // replace retained text too
        truncated = ContextBuilder::build(view, {}, std::span{output}.first(1));
        rejected = ContextBuilder::build(view, {0}, output);
    }
    EXPECT_FALSE(allocation_failed);
    EXPECT_EQ(complete.status, ContextStatus::complete);
    EXPECT_EQ(complete.emitted, 2U);
    EXPECT_EQ(truncated.status, ContextStatus::output_capacity);
    EXPECT_EQ(rejected.status, ContextStatus::unsupported_version);
    EXPECT_EQ(output[1].definition_id(), "example.structural:unit");
    EXPECT_EQ(Persistence::capture(world, random), before);
}

TEST(ContextAllocationTest, DirectedLookupNeverAllocatesOrMutatesViewWorldOrRng) {
    WorldState world;
    RandomStreams random{77};
    static_cast<void>(random.stream(7).next());
    const auto actor = world.create_entity();
    for (std::size_t i = 0; i < 4097; ++i) {
        const auto id = world.create_entity();
        ASSERT_EQ(world.learn_fact(actor, {id}), KnowledgeChange::applied);
    }
    const auto target = world.create_entity();
    const auto hidden = world.create_entity();
    auto catalog = corerpg_test::structural_catalog();
    ASSERT_EQ(world.bind_definition(target, *catalog.catalog(), *DefinitionId::parse("example.structural:unit")), DefinitionBindingChange::applied);
    const std::array grants{FactKey{target}, FactKey{target, FactKind::definition}};
    const auto view = Visibility::project(world, actor, grants);
    const auto original_view = view;
    const auto before = Persistence::capture(world, random);
    std::array<VisibleFact, 4> output;
    ContextResult complete, typed, truncated, examined, content;
    std::array<ContextResult, 3> empty;
    bool buffers_unchanged = true;
    {
        AllocationFailure fail{0};
        complete = ContextBuilder::build(view, {1, target}, output);
        complete = ContextBuilder::build(view, {1, target}, output); // replace owned text
        typed = ContextBuilder::build(view, {1, target, FactKind::definition}, output);
        truncated = ContextBuilder::build(view, {1, target}, std::span{output}.first(1));
        examined = ContextBuilder::build(view, {1, target}, output, {1, 4, 4096});
        content = ContextBuilder::build(view, {1, target}, output, {4, 4, 59});
        const auto original = output;
        const std::array ids{EntityId{}, hidden, EntityId{UINT64_MAX}};
        for (std::size_t i = 0; i < ids.size(); ++i) {
            empty[i] = ContextBuilder::build(view, {1, ids[i]}, output);
            buffers_unchanged = buffers_unchanged && output == original;
        }
    }
    EXPECT_FALSE(allocation_failed);
    EXPECT_EQ(complete.status, ContextStatus::complete);
    EXPECT_EQ(complete.examined, 2U);
    EXPECT_EQ(complete.emitted, 2U);
    EXPECT_EQ(typed.examined, 2U);
    EXPECT_EQ(typed.emitted, 1U);
    EXPECT_EQ(truncated.status, ContextStatus::output_capacity);
    EXPECT_EQ(examined.status, ContextStatus::examination_limit);
    EXPECT_EQ(content.status, ContextStatus::content_limit);
    for (const auto& result : empty) EXPECT_EQ(result, ContextResult{});
    EXPECT_TRUE(buffers_unchanged);
    EXPECT_EQ(output[1].definition_id(), "example.structural:unit");
    EXPECT_EQ(view, original_view);
    EXPECT_EQ(Persistence::capture(world, random), before);
}

TEST(ContextAllocationTest, EveryProjectionAllocationFailurePreservesPublishedContextAndWorld) {
    WorldState world;
    RandomStreams random{77};
    const auto actor = world.create_entity();
    const auto target = world.create_entity();
    auto catalog = corerpg_test::structural_catalog();
    ASSERT_EQ(world.learn_fact(actor, {target}), KnowledgeChange::applied);
    ASSERT_EQ(world.bind_definition(target, *catalog.catalog(), *DefinitionId::parse("example.structural:unit")), DefinitionBindingChange::applied);
    const auto before = Persistence::capture(world, random);
    const std::array grants{FactKey{target, FactKind::definition}};
    ObserverView published = Visibility::project(world, actor);
    const auto original_view = published;
    std::array<VisibleFact, 8> output;
    const auto original_result = ContextBuilder::build(published, {}, output);
    const auto original_output = output;
    std::size_t failures = 0;
    bool completed = false;
    for (std::size_t index = 0; index < 128; ++index) {
        bool threw = false;
        {
            AllocationFailure fail{index};
            try { published = Visibility::project(world, actor, grants); }
            catch (const std::bad_alloc&) { threw = true; }
        }
        ASSERT_EQ(threw, allocation_failed);
        EXPECT_EQ(Persistence::capture(world, random), before);
        if (threw) {
            ++failures;
            EXPECT_EQ(published, original_view);
            EXPECT_EQ(output, original_output);
            EXPECT_EQ(ContextBuilder::build(published, {}, output), original_result);
        } else {
            completed = true;
            break;
        }
    }
    EXPECT_TRUE(completed);
    EXPECT_GT(failures, 0U);
    RecordProperty("injected_allocation_failures", static_cast<int>(failures));
    EXPECT_EQ(ContextBuilder::build(published, {}, output).emitted, 2U);
    EXPECT_EQ(output[1].provenance(), FactProvenance::temporary_grant);
}
} // namespace
