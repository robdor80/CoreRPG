#pragma once

#include <corerpg/visibility.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

namespace corerpg {

inline constexpr std::uint32_t context_contract_version = 1;
/// Logical payload accounting, independent of ABI padding and Host serialization:
/// key(25), presence(1), reference(8), total/latent(16), provenance(1), text length(8).
inline constexpr std::size_t context_fact_fixed_bytes = 59;

/// Equality filters combined with AND. No filter selects all facts in the view.
/// An invalid/unknown/hidden subject uniformly matches nothing. No world lookup.
struct ContextRequest final {
    std::uint32_t contract_version{context_contract_version};
    std::optional<EntityId> subject{};
    std::optional<FactKind> kind{};
};

struct ContextLimits final {
    static constexpr std::size_t examination_ceiling = 65536;
    static constexpr std::size_t selection_ceiling = 256;
    static constexpr std::size_t content_ceiling = 1024 * 1024;
    std::size_t max_examined{4096};
    std::size_t max_selected{128};
    /// Fixed logical bytes above plus definition_id().size() per emitted fact.
    /// Excludes envelope, allocator overhead, retained shared ownership and tokens.
    std::size_t max_content_bytes{65536};
};

enum class ContextStatus : std::uint8_t {
    complete,
    examination_limit,
    selection_limit,
    output_capacity,
    content_limit,
    unsupported_version,
    invalid_request,
    invalid_limits,
};

/// Only output.first(emitted) belongs to this call. Tail remains unchanged.
/// Complete is relative to the filtered view, never proof of world completeness.
/// An examined matching record blocked by output limits is included in examined.
struct ContextResult final {
    std::uint32_t contract_version{context_contract_version};
    ContextStatus status{ContextStatus::complete};
    std::size_t examined{};
    std::size_t emitted{};
    std::size_t content_bytes{};
    bool operator==(const ContextResult&) const noexcept = default;
};

/// Pure authorized selection, canonical FactKey order, no allocation or RNG.
/// Caller owns output; emitted VisibleFact values retain immutable text even after
/// view/world destruction. Copies are not revoked/refreshed automatically.
/// No dynamic scratch (zero bytes); bounded caller storage is the only record buffer.
/// Input/output must not overlap; caller excludes concurrent access to output.
/// No references to the input survive. Errors before selection leave output intact.
/// Truncation returns a valid prefix with an explicit reason, never silent success.
class ContextBuilder final {
  public:
    [[nodiscard]] static ContextResult build(const ObserverView& view,
                                             const ContextRequest& request,
                                             std::span<VisibleFact> output,
                                             ContextLimits limits = {}) noexcept;
};

} // namespace corerpg
