#pragma once

#include <corerpg/random.hpp>

#include <cstddef>
#include <cstdint>
#include <map>

namespace corerpg {

struct CampaignSnapshot;

/// Explicit owner of independent deterministic RNG streams for one simulation context.
/// The caller assigns stable numeric stream IDs; neither creation order nor other
/// streams' draws may affect a stream's initial state or subsequent sequence.
/// A RandomStreams object is never a process-global RNG or a WorldState field.
class RandomStreams final {
public:
    using seed_type = DeterministicRandom::state_type;
    using stream_id_type = std::uint64_t;

    explicit RandomStreams(seed_type master_seed) noexcept : master_seed_{master_seed} {}

    [[nodiscard]] seed_type master_seed() const noexcept { return master_seed_; }
    [[nodiscard]] std::size_t stream_count() const noexcept { return streams_.size(); }
    [[nodiscard]] bool contains_stream(stream_id_type stream_id) const noexcept;

    /// Returns an existing stream or creates it with its canonical derived seed.
    /// A first insertion may allocate and throw; failure leaves this object unchanged.
    /// References remain valid when other streams are added; copying clones stream states.
    [[nodiscard]] DeterministicRandom& stream(stream_id_type stream_id);

    /// Non-creating observation; absent streams have no consumed state.
    [[nodiscard]] const DeterministicRandom* find_stream(stream_id_type stream_id) const noexcept;

private:
    friend class Persistence;
    /// Private validated staging path; propagates sentinel/proxy allocation failures.
    explicit RandomStreams(const CampaignSnapshot& snapshot);
    seed_type master_seed_;
    std::map<stream_id_type, DeterministicRandom> streams_;
};

}  // namespace corerpg
