#pragma once
#include <corerpg/action.hpp>
#include <corerpg/world_state.hpp>
#include <corerpg/random_streams.hpp>
#include <span>
#include <vector>

namespace corerpg {
/// Generic integer campaign ticks. Epoch and tick duration belong to content.
/// Privileged API: the host authorizes requests before enqueueing; no render/FPS input.
struct TimedAction final {
    std::uint64_t id{}; // Contiguous, caller-owned idempotency key, starting at 1.
    std::uint64_t due{};
    std::uint64_t parent{}; // Optional earlier action; only successful parents permit execution.
    EntityId actor;
    ActionOperation operation;
};
enum class TimedState : std::uint8_t { pending, accepted, no_change, rejected, causal_rejected };
struct TimedRecord final {
    TimedAction action;
    TimedState state{TimedState::pending};
    std::uint64_t revision_before{}, revision_after{}, roll{};
    ActionRejection rejection{ActionRejection::none};
    bool has_roll{};
};
struct TimelineSnapshot final {
    std::uint64_t now{};
    std::vector<TimedRecord> records = std::vector<TimedRecord>(0);
    TimelineSnapshot() = default;
    TimelineSnapshot(const TimelineSnapshot&) = default;
    TimelineSnapshot& operator=(const TimelineSnapshot&) = default;
    TimelineSnapshot& operator=(TimelineSnapshot&&) = default;
    // MSVC checked-iterator vector moves may allocate in a noexcept constructor.
    // Use the throwing count constructor above, then a non-allocating swap.
    TimelineSnapshot(TimelineSnapshot&& other) : now{other.now} { records.swap(other.records); }
};
enum class TimelineStatus : std::uint8_t {
    completed, no_change, invalid_request, key_conflict, limit_exceeded,
    backwards_time, action_limit, allocation_failure, capacity_failure
};
struct TimelineResult final {
    TimelineStatus status{TimelineStatus::completed};
    std::size_t processed{};
};
class CampaignPersistence;
class Timeline final {
public:
    static constexpr std::size_t max_records = 4096;
    [[nodiscard]] std::uint64_t now() const noexcept { return state_.now; }
    [[nodiscard]] std::span<const TimedRecord> records() const noexcept { return state_.records; }
    [[nodiscard]] TimelineSnapshot snapshot() const { return state_; }
    /// Exact duplicate returns no_change even after completion. Collision fails safely.
    /// No borrowed preconditions/revision guards are retained. ActionEngine rechecks
    /// structural validity at execution. Domain authorization is a host responsibility.
    [[nodiscard]] TimelineStatus schedule(const WorldState&, const TimedAction&);
    /// Orders by (due, id), atomic per action, terminal rejection is never retried.
    /// Resume a bounded/failing call with the SAME horizon. Saves are valid at returns.
    [[nodiscard]] TimelineResult advance(WorldState&, RandomStreams&, std::uint64_t horizon,
                                         std::size_t max_actions = 64) noexcept;
    [[nodiscard]] static bool validate(const TimelineSnapshot&, std::uint64_t last_entity,
                                        std::uint64_t revision) noexcept;
private:
    friend class CampaignPersistence;
    TimelineSnapshot state_;
};
} // namespace corerpg
