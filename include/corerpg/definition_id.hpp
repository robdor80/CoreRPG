#pragma once

#include <corerpg/pack_id.hpp>

#include <compare>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace corerpg {

/// Stable identity of a logical definition supplied by a Content Pack.
class DefinitionId final {
public:
    /// Same grammar as parse(), without construction or allocation.
    [[nodiscard]] static bool is_valid_text(std::string_view text) noexcept;
    [[nodiscard]] static std::optional<DefinitionId> parse(std::string_view text);

    [[nodiscard]] std::string_view value() const noexcept { return value_; }
    [[nodiscard]] const PackId& pack_id() const noexcept { return pack_id_; }
    [[nodiscard]] std::string_view local_path() const noexcept { return local_path_; }

    [[nodiscard]] bool operator==(const DefinitionId& other) const noexcept {
        return value_ == other.value_;
    }

    [[nodiscard]] auto operator<=>(const DefinitionId& other) const noexcept {
        return value_ <=> other.value_;
    }

private:
    DefinitionId(PackId pack_id, std::string local_path, std::string value)
        : pack_id_{std::move(pack_id)}, local_path_{std::move(local_path)},
          value_{std::move(value)} {}

    PackId pack_id_;
    std::string local_path_;
    std::string value_;
};

} // namespace corerpg
