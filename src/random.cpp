#include <corerpg/random.hpp>

#include <limits>

namespace corerpg {

DeterministicRandom::result_type DeterministicRandom::next() noexcept {
    state_ += state_type{0x9E3779B97F4A7C15ULL};

    auto value = state_;
    value = (value ^ (value >> 30U)) * result_type{0xBF58476D1CE4E5B9ULL};
    value = (value ^ (value >> 27U)) * result_type{0x94D049BB133111EBULL};
    return value ^ (value >> 31U);
}

std::optional<DeterministicRandom::result_type>
DeterministicRandom::uniform_closed(result_type minimum, result_type maximum) noexcept {
    if (minimum > maximum) {
        return std::nullopt;
    }

    if (minimum == maximum) {
        return minimum;
    }

    const auto range_size = maximum - minimum + 1U;
    if (range_size == 0U) {
        return next();
    }

    const auto rejection_threshold =
        (std::numeric_limits<result_type>::max() - range_size + 1U) % range_size;
    while (true) {
        const auto sample = next();
        if (sample >= rejection_threshold) {
            return minimum + (sample % range_size);
        }
    }
}

} // namespace corerpg
