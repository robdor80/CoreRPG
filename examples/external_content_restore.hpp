#pragma once

// Host-side example, not a CoreRPG API or a replacement persistence codec.
#include <corerpg/content_catalog.hpp>
#include <corerpg/persistence.hpp>

#include <algorithm>
#include <string>
#include <vector>

namespace corerpg_example {

struct ContentRelease final {
    std::string pack_id;
    std::string pack_version;
    std::string definition_set_sha256;
    std::string release_sha256;
    bool operator==(const ContentRelease&) const = default;
};

// External save metadata. Store together with the CRPGSAVE bytes in the Host.
// The context fingerprint covers the initial recipe/seed/region/generator version,
// never a second copy of mutable entities or properties.
struct ExternalSaveMetadata final {
    std::uint32_t format_version{1};
    std::vector<ContentRelease> releases;
    std::string generation_context_sha256;
    bool operator==(const ExternalSaveMetadata&) const = default;
};

inline bool valid_digest(const std::string& value) noexcept {
    return value.size() == 64 && std::all_of(value.begin(), value.end(), [](char c) {
        return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
    });
}

inline bool valid_metadata(const ExternalSaveMetadata& value) noexcept {
    if (value.format_version != 1 || value.releases.empty() || value.releases.size() > 128 ||
        !valid_digest(value.generation_context_sha256)) return false;
    std::string_view previous;
    for (const auto& release : value.releases) {
        // installed came from the real manifest decoder/catalog preflight. Saved
        // text must equal it exactly below. Reconstructing owning strong IDs here
        // adds no validation and introduces unnecessary allocation/move failures.
        if (release.pack_id.empty() || release.pack_id.size() > 4096 ||
            release.pack_version.empty() || release.pack_version.size() > 128 ||
            !valid_digest(release.definition_set_sha256) || !valid_digest(release.release_sha256) ||
            release.pack_id <= previous) return false;
        previous = release.pack_id;
    }
    return true;
}

enum class ExternalRestoreStatus { success, incompatible_content, invalid_save };
struct ExternalRestoreResult final {
    ExternalRestoreStatus status;
    // Only populated after invoking Persistence. A metadata refusal does no Core work.
    std::optional<corerpg::PersistenceStatus> persistence_status;
};

// installed must come from a trusted, immutable, hash-verified installed bundle.
// Compare saved metadata BEFORE invoking load, even when DefinitionIds still exist.
// Matching hashes prove identity, not publisher authenticity or a canon migration.
// No concurrent mutations or file replacement; exceptions preserve Core destinations.
inline ExternalRestoreResult restore_external_save(
    std::span<const std::uint8_t> bytes, const ExternalSaveMetadata& saved,
    const ExternalSaveMetadata& installed, const corerpg::ContentCatalog& catalog,
    corerpg::WorldState& world, corerpg::RandomStreams& random,
    corerpg::PersistenceLimits limits = {}) {
    if (!valid_metadata(saved) || !valid_metadata(installed) || saved != installed)
        return {ExternalRestoreStatus::incompatible_content, std::nullopt};
    const auto status = corerpg::Persistence::load(bytes, world, random, limits, &catalog);
    return {status == corerpg::PersistenceStatus::success ? ExternalRestoreStatus::success
                                                       : ExternalRestoreStatus::invalid_save,
            status};
}
} // namespace corerpg_example
