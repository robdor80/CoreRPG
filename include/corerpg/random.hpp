#pragma once

#include <cstdint>
#include <optional>

namespace corerpg {

/// SplitMix64 deterministic random generator with an explicit 64-bit state.
class DeterministicRandom final {
public:
    using result_type = std::uint64_t;
    using state_type = std::uint64_t;

    constexpr explicit DeterministicRandom(state_type seed) noexcept : state_{seed} {}

    [[nodiscard]] result_type next() noexcept;

    /// Returns a value in [minimum, maximum], or nullopt for an invalid range.
    [[nodiscard]] std::optional<result_type> uniform_closed(result_type minimum,
                                                            result_type maximum) noexcept;

    [[nodiscard]] constexpr state_type state() const noexcept { return state_; }

private:
    state_type state_;
};

} // namespace corerpg
