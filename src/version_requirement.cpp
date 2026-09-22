#include <corerpg/version_requirement.hpp>

#include <optional>
#include <utility>

namespace corerpg {

std::optional<VersionRequirement>
VersionRequirement::create(SemanticVersion minimum_inclusive,
                           std::optional<SemanticVersion> maximum_exclusive) {
    if (maximum_exclusive.has_value() && *maximum_exclusive <= minimum_inclusive) {
        return std::nullopt;
    }

    return VersionRequirement{std::move(minimum_inclusive), std::move(maximum_exclusive)};
}

bool VersionRequirement::is_satisfied_by(const SemanticVersion& version) const noexcept {
    return version >= minimum_inclusive_ &&
           (!maximum_exclusive_.has_value() || version < *maximum_exclusive_);
}

} // namespace corerpg
