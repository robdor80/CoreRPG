#include <corerpg/persistence.hpp>
#include "../examples/external_content_restore.hpp"

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

using corerpg::EntityId;
using corerpg::Persistence;
using corerpg::PersistenceStatus;

class PersistenceAllocationTest : public ::testing::Test {
  protected:
    corerpg::WorldState source;
    corerpg::RandomStreams source_random{123};
    corerpg::WorldState target;
    corerpg::RandomStreams target_random{987};
    corerpg::CampaignSnapshot snapshot;
    corerpg::CampaignSnapshot original_target;
    std::vector<std::uint8_t> bytes;

    void SetUp() override {
        for (int i = 0; i < 6; ++i) {
            static_cast<void>(source.create_entity());
        }
        ASSERT_EQ(source.place_entity(EntityId{1}, EntityId{3}),
                  corerpg::ContainmentChange::applied);
        ASSERT_EQ(source.place_entity(EntityId{3}, EntityId{6}),
                  corerpg::ContainmentChange::applied);
        for (std::uint64_t id = 0; id < 7; ++id) {
            static_cast<void>(source_random.stream(id).next());
        }
        static_cast<void>(target.create_entity());
        static_cast<void>(target.create_entity());
        ASSERT_EQ(target.place_entity(EntityId{2}, EntityId{1}),
                  corerpg::ContainmentChange::applied);
        static_cast<void>(target_random.stream(45).next());
        snapshot = Persistence::capture(source, source_random);
        original_target = Persistence::capture(target, target_random);
        bytes = Persistence::serialize(snapshot).bytes;
    }

    template <typename Operation>
    void fail_each_allocation(Operation operation, bool replaces_target = false) {
        const auto* old_reference = target_random.find_stream(45);
        std::size_t failures = 0;
        bool completed = false;
        for (std::size_t index = 0; index < 512; ++index) {
            bool threw = false;
            PersistenceStatus status{};
            {
                AllocationFailure failure{index};
                try {
                    status = operation();
                } catch (const std::bad_alloc&) {
                    threw = true;
                }
            }
            ASSERT_EQ(threw, allocation_failed) << index;
            EXPECT_EQ(Persistence::capture(source, source_random), snapshot);
            EXPECT_EQ(Persistence::serialize(snapshot).bytes, bytes);
            if (threw) {
                ++failures;
                EXPECT_EQ(Persistence::capture(target, target_random), original_target) << index;
                EXPECT_EQ(target_random.find_stream(45), old_reference) << index;
                continue;
            }
            EXPECT_EQ(status, PersistenceStatus::success);
            EXPECT_EQ(Persistence::capture(target, target_random),
                      replaces_target ? snapshot : original_target);
            completed = true;
            break;
        }
        EXPECT_TRUE(completed);
        EXPECT_GT(failures, 0U);
        RecordProperty("injected_allocation_failures", static_cast<int>(failures));
    }
};

TEST_F(PersistenceAllocationTest, CapturePreservesSourceAtEveryAllocationFailure) {
    fail_each_allocation([&] {
        const auto captured = Persistence::capture(source, source_random);
        return captured == snapshot ? PersistenceStatus::success : PersistenceStatus::invalid_world;
    });
}

TEST_F(PersistenceAllocationTest, SerializePreservesInputsAtEveryAllocationFailure) {
    fail_each_allocation([&] { return Persistence::serialize(snapshot).status; });
}

TEST_F(PersistenceAllocationTest, DeserializePublishesNothingAtEveryAllocationFailure) {
    fail_each_allocation([&] { return Persistence::deserialize(bytes).status; });
}

TEST_F(PersistenceAllocationTest, RestorePreservesBothDestinationsAtEveryAllocationFailure) {
    fail_each_allocation([&] { return Persistence::restore(snapshot, target, target_random); },
                         true);
}

TEST_F(PersistenceAllocationTest, LoadPreservesBothDestinationsAtEveryAllocationFailure) {
    fail_each_allocation([&] { return Persistence::load(bytes, target, target_random); }, true);
}

TEST_F(PersistenceAllocationTest, ExternalMetadataAndLoadPreserveDestinationsAtEveryAllocationFailure) {
    const auto catalog = corerpg::ContentCatalog::create({});
    ASSERT_TRUE(catalog.succeeded());
    // Long valid identity forces the Host metadata parsers to allocate too.
    const corerpg_example::ExternalSaveMetadata metadata{
        1, {{"example.external-content-allocation", "1.0.0", std::string(64, 'a'), std::string(64, 'b')}},
        std::string(64, 'c')};
    fail_each_allocation([&] {
        const auto result = corerpg_example::restore_external_save(
            bytes, metadata, metadata, *catalog.catalog(), target, target_random);
        return result.persistence_status.value_or(PersistenceStatus::invalid_world);
    }, true);
}

} // namespace
