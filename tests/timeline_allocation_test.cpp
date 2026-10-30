#include <corerpg/campaign_persistence.hpp>
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
TEST(TimelineAllocation, AdvanceFailureLeavesPendingActionAndRngUntouched) {
    WorldState world; RandomStreams rng{42}; Timeline time;
    const auto actor=world.create_entity();
    ASSERT_EQ(time.schedule(world,{1,10,0,actor,StreamRollAction{17,0,UINT64_MAX}}),TimelineStatus::completed);
    const auto before=CampaignPersistence::serialize(CampaignPersistence::capture(world,rng,time)).bytes;
    TimelineResult result;
    { AllocationFailure failure{0}; result=time.advance(world,rng,100); }
    EXPECT_EQ(result.status,TimelineStatus::allocation_failure);
    EXPECT_EQ(result.processed,0U);
    EXPECT_EQ(before,CampaignPersistence::serialize(CampaignPersistence::capture(world,rng,time)).bytes);
    EXPECT_EQ(time.advance(world,rng,100).processed,1U);
    EXPECT_EQ(time.advance(world,rng,100).processed,0U);
}
TEST(TimelineAllocation, RestoreIsAtomicAtEveryAllocationFailure) {
    WorldState source; RandomStreams random{42}; Timeline original;
    const auto actor=source.create_entity(); const auto room=source.create_entity();
    ASSERT_EQ(source.learn_fact(actor,{room}),KnowledgeChange::applied);
    ASSERT_EQ(original.schedule(source,{1,10,0,actor,StreamRollAction{7,0,100}}),TimelineStatus::completed);
    ASSERT_EQ(original.advance(source,random,10).processed,1U);
    ASSERT_EQ(original.schedule(source,{2,20,1,actor,PlaceEntityAction{actor,room}}),TimelineStatus::completed);
    const auto snapshot=CampaignPersistence::capture(source,random,original);
    const auto bytes=CampaignPersistence::serialize(snapshot).bytes;
    WorldState target; RandomStreams rng{99}; Timeline time;
    ASSERT_TRUE(target.create_entity().is_valid());
    const auto before=CampaignPersistence::serialize(CampaignPersistence::capture(target,rng,time)).bytes;
    std::size_t failures=0; bool completed=false;
    for(std::size_t i=0;i<256;++i) {
        bool threw=false; CampaignArchiveStatus result{};
        { AllocationFailure failure{i};
          try { result=CampaignPersistence::load(bytes,target,rng,time); }
          catch(const std::bad_alloc&) { threw=true; }
        }
        if(threw) {
            ++failures;
            EXPECT_EQ(before,CampaignPersistence::serialize(CampaignPersistence::capture(target,rng,time)).bytes) << i;
        } else {
            ASSERT_EQ(result,CampaignArchiveStatus::success);
            EXPECT_EQ(bytes,CampaignPersistence::serialize(CampaignPersistence::capture(target,rng,time)).bytes);
            completed=true; break;
        }
    }
    EXPECT_TRUE(completed); EXPECT_GT(failures,0U);
    RecordProperty("injected_allocation_failures",static_cast<int>(failures));
}
TEST(TimelineAllocation, ScheduleFailurePreservesIdempotencyLedger) {
    WorldState world; RandomStreams rng{1}; Timeline time;
    const auto actor=world.create_entity();
    const auto before=CampaignPersistence::serialize(CampaignPersistence::capture(world,rng,time)).bytes;
    bool threw=false;
    { AllocationFailure failure{0};
      try { static_cast<void>(time.schedule(world,{1,1,0,actor,StreamRollAction{0,0,10}})); }
      catch(const std::bad_alloc&) { threw=true; }
    }
    EXPECT_TRUE(threw);
    EXPECT_EQ(before,CampaignPersistence::serialize(CampaignPersistence::capture(world,rng,time)).bytes);
    EXPECT_EQ(time.schedule(world,{1,1,0,actor,StreamRollAction{0,0,10}}),TimelineStatus::completed);
}
} // namespace

