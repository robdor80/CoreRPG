#pragma once
#include <corerpg/persistence.hpp>
#include <corerpg/timeline.hpp>

namespace corerpg {
/// Additive archive: unchanged Core SAVE v1-v3 plus a versioned generic timeline.
/// Old Persistence remains source/wire compatible; it cannot silently discard time.
enum class CampaignArchiveStatus : std::uint8_t {
    success, invalid_magic, unsupported_version, invalid_size, checksum_mismatch,
    limit_exceeded, invalid_timeline, core_rejected
};
struct CampaignArchiveSnapshot final {
    CampaignSnapshot campaign;
    TimelineSnapshot timeline;
};
struct CampaignArchiveBytes final {
    CampaignArchiveStatus status{CampaignArchiveStatus::success};
    std::vector<std::uint8_t> bytes = std::vector<std::uint8_t>(0);
};
struct DecodedCampaignArchive final {
    CampaignArchiveStatus status{CampaignArchiveStatus::success};
    std::optional<CampaignArchiveSnapshot> snapshot;
};
class CampaignPersistence final {
public:
    [[nodiscard]] static CampaignArchiveSnapshot capture(const WorldState&,const RandomStreams&,const Timeline&);
    [[nodiscard]] static CampaignArchiveStatus validate(const CampaignArchiveSnapshot&,PersistenceLimits = {});
    [[nodiscard]] static CampaignArchiveBytes serialize(const CampaignArchiveSnapshot&,PersistenceLimits = {});
    [[nodiscard]] static DecodedCampaignArchive deserialize(std::span<const std::uint8_t>,PersistenceLimits = {});
    /// Allocation/validation failure preserves all three live destinations together.
    [[nodiscard]] static CampaignArchiveStatus restore(const CampaignArchiveSnapshot&,WorldState&,RandomStreams&,
                                                       Timeline&,PersistenceLimits = {},const ContentCatalog* = nullptr);
    [[nodiscard]] static CampaignArchiveStatus load(std::span<const std::uint8_t>,WorldState&,RandomStreams&,
                                                    Timeline&,PersistenceLimits = {},const ContentCatalog* = nullptr);
};
} // namespace corerpg
