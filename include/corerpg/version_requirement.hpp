#pragma once

#include <corerpg/semantic_version.hpp>

#include <optional>
#include <utility>

namespace corerpg {

/// Structured version interval [minimum_inclusive, maximum_exclusive).
class VersionRequirement final {
public:
    [[nodiscard]] static std::optional<VersionRequirement>
    create(SemanticVersion minimum_inclusive,
           std::optional<SemanticVersion> maximum_exclusive = std::nullopt);

    [[nodiscard]] const SemanticVersion& minimum_inclusive() const noexcept {
        return minimum_inclusive_;
    }

    [[nodiscard]] const std::optional<SemanticVersion>& maximum_exclusive() const noexcept {
        return maximum_exclusive_;
    }

    [[nodiscard]] bool is_satisfied_by(const SemanticVersion& version) const noexcept;

private:
    VersionRequirement(SemanticVersion minimum_inclusive,
                       std::optional<SemanticVersion> maximum_exclusive)
        : minimum_inclusive_{std::move(minimum_inclusive)},
          maximum_exclusive_{std::move(maximum_exclusive)} {}

    SemanticVersion minimum_inclusive_;
    std::optional<SemanticVersion> maximum_exclusive_;
};

} // namespace corerpg
