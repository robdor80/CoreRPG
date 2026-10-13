#include <corerpg/random_streams.hpp>

#include <cstdint>

namespace corerpg {
namespace {

// Versioned seed policy for stream IDs: all operations use uint64 modulo 2^64.
// The additive constant and odd multiplier make each stream ID's mixed input
// distinct for a given master seed. SplitMix64 is reused without changing it.
// Changing any constant or this composition is a compatibility change.
[[nodiscard]] DeterministicRandom::state_type derive_seed(
    DeterministicRandom::state_type master_seed, RandomStreams::stream_id_type stream_id) noexcept {
    constexpr std::uint64_t kOffset = 0xD1B54A32D192ED03ULL;
    constexpr std::uint64_t kMultiplier = 0x9E3779B97F4A7C15ULL;
    DeterministicRandom seed_mixer{
        master_seed ^ ((stream_id + kOffset) * kMultiplier)};
    return seed_mixer.next();
}

}  // namespace

bool RandomStreams::contains_stream(stream_id_type stream_id) const noexcept {
    return streams_.contains(stream_id);
}

DeterministicRandom& RandomStreams::stream(stream_id_type stream_id) {
    auto [iterator, inserted] = streams_.try_emplace(
        stream_id, derive_seed(master_seed_, stream_id));
    static_cast<void>(inserted);
    return iterator->second;
}

const DeterministicRandom* RandomStreams::find_stream(stream_id_type stream_id) const noexcept {
    const auto iterator = streams_.find(stream_id);
    return iterator == streams_.end() ? nullptr : &iterator->second;
}

}  // namespace corerpg
