#include <corerpg/campaign_persistence.hpp>
#include "persistence_internal.hpp"
#include "timeline_internal.hpp"
#include <algorithm>
#include <array>
#include <limits>

namespace corerpg {
namespace {
constexpr std::array<std::uint8_t,8> magic{'C','R','P','G','T','I','M','E'};
constexpr std::size_t header=44, record_size=112, overhead=header+4;
bool fits(std::uint64_t core,std::uint64_t count,PersistenceLimits limits) noexcept {
    const auto maximum=std::min(limits.max_bytes,static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max()));
    return maximum>=overhead && core<=maximum-overhead && count<=Timeline::max_records &&
        count<=(maximum-overhead-core)/record_size;
}
}
CampaignArchiveSnapshot CampaignPersistence::capture(const WorldState& w,const RandomStreams& r,const Timeline& t) {
    return {Persistence::capture(w,r),t.snapshot()};
}
CampaignArchiveStatus CampaignPersistence::validate(const CampaignArchiveSnapshot& s,PersistenceLimits limits) {
    if (Persistence::validate(s.campaign,limits)!=PersistenceStatus::success) return CampaignArchiveStatus::core_rejected;
    if (!Timeline::validate(s.timeline,s.campaign.last_entity_id,s.campaign.revision)) return CampaignArchiveStatus::invalid_timeline;
    const auto size=detail::encoded_save_size(s.campaign,limits);
    return size && fits(*size,s.timeline.records.size(),limits) ? CampaignArchiveStatus::success : CampaignArchiveStatus::limit_exceeded;
}
CampaignArchiveBytes CampaignPersistence::serialize(const CampaignArchiveSnapshot& s,PersistenceLimits limits) {
    CampaignArchiveBytes result;
    result.status=validate(s,limits);
    if (result.status!=CampaignArchiveStatus::success) return result;
    const auto core=Persistence::serialize(s.campaign,limits);
    if (core.status!=PersistenceStatus::success) { result.status=CampaignArchiveStatus::core_rejected; return result; }
    auto& bytes=result.bytes;
    bytes.reserve(overhead+core.bytes.size()+record_size*s.timeline.records.size());
    bytes.insert(bytes.end(),magic.begin(),magic.end());
    const auto put=[&](auto v,unsigned width=8) { detail::append_integer(bytes,static_cast<std::uint64_t>(v),width); };
    put(1,4); put(core.bytes.size()); put(s.timeline.now); put(s.timeline.records.size()); put(1); // tick policy 1: opaque uint64
    bytes.insert(bytes.end(),core.bytes.begin(),core.bytes.end());
    for (const auto& r:s.timeline.records) {
        put(r.action.id); put(r.action.due); put(r.action.parent); put(r.action.actor.value());
        for (auto v:detail::timed_operation(r.action.operation)) put(v);
        put(r.state); put(r.revision_before); put(r.revision_after); put(r.roll); put(r.rejection); put(r.has_roll);
    }
    put(detail::save_checksum(bytes),4);
    return result;
}
DecodedCampaignArchive CampaignPersistence::deserialize(std::span<const std::uint8_t> bytes,PersistenceLimits limits) {
    if (bytes.size()>limits.max_bytes) return {CampaignArchiveStatus::limit_exceeded,{}};
    if (bytes.size()<overhead) return {CampaignArchiveStatus::invalid_size,{}};
    if (!std::equal(magic.begin(),magic.end(),bytes.begin())) return {CampaignArchiveStatus::invalid_magic,{}};
    std::size_t pos=8;
    const auto read=[&](unsigned width=8) { std::uint64_t v=0; for (unsigned i=0;i<width;++i) v|=static_cast<std::uint64_t>(bytes[pos++])<<(8*i); return v; };
    if (read(4)!=1) return {CampaignArchiveStatus::unsupported_version,{}};
    const auto core_size=read(), now=read(), count=read(), policy=read();
    if (policy!=1) return {CampaignArchiveStatus::unsupported_version,{}};
    if (!fits(core_size,count,limits)) return {CampaignArchiveStatus::limit_exceeded,{}};
    if (bytes.size()!=overhead+core_size+count*record_size) return {CampaignArchiveStatus::invalid_size,{}};
    pos=bytes.size()-4;
    if (read(4)!=detail::save_checksum(bytes.first(bytes.size()-4))) return {CampaignArchiveStatus::checksum_mismatch,{}};
    const auto decoded=Persistence::deserialize(bytes.subspan(header,static_cast<std::size_t>(core_size)),limits);
    if (decoded.status!=PersistenceStatus::success) return {CampaignArchiveStatus::core_rejected,{}};
    CampaignArchiveSnapshot s{*decoded.snapshot,{}};
    s.timeline.now=now; s.timeline.records.reserve(static_cast<std::size_t>(count));
    pos=header+static_cast<std::size_t>(core_size);
    for (std::uint64_t i=0;i<count;++i) {
        TimedRecord r;
        r.action.id=read(); r.action.due=read(); r.action.parent=read(); r.action.actor=EntityId{read()};
        const auto tag=read(), a=read(), b=read(), c=read();
        switch(tag) {
        case 1: if(c) return {CampaignArchiveStatus::invalid_timeline,{}}; r.action.operation=PlaceEntityAction{EntityId{a},EntityId{b}}; break;
        case 2: if(b || c) return {CampaignArchiveStatus::invalid_timeline,{}}; r.action.operation=RemoveContainmentAction{EntityId{a}}; break;
        case 3: r.action.operation=AddRelationAction{{EntityId{a},RelationKind{b},EntityId{c}}}; break;
        case 4: r.action.operation=RemoveRelationAction{{EntityId{a},RelationKind{b},EntityId{c}}}; break;
        case 5: r.action.operation=StreamRollAction{a,b,c}; break;
        default: return {CampaignArchiveStatus::invalid_timeline,{}};
        }
        const auto state=read(); r.revision_before=read(); r.revision_after=read(); r.roll=read();
        const auto rejection=read(), has_roll=read();
        if (state>4 || rejection>static_cast<unsigned>(ActionRejection::invalid_operation) || has_roll>1)
            return {CampaignArchiveStatus::invalid_timeline,{}};
        r.state=static_cast<TimedState>(state); r.rejection=static_cast<ActionRejection>(rejection); r.has_roll=has_roll!=0;
        s.timeline.records.push_back(r);
    }
    const auto status=validate(s,limits);
    if(status!=CampaignArchiveStatus::success) return {status,{}};
    return {status,std::move(s)};
}
CampaignArchiveStatus CampaignPersistence::restore(const CampaignArchiveSnapshot& s,WorldState& w,RandomStreams& r,
                                                    Timeline& t,PersistenceLimits limits,const ContentCatalog* catalog) {
    const auto status=validate(s,limits);
    if (status!=CampaignArchiveStatus::success) return status;
    auto timeline=s.timeline; // All timeline allocations precede Core's atomic publication.
    if (Persistence::restore(s.campaign,w,r,limits,catalog)!=PersistenceStatus::success) return CampaignArchiveStatus::core_rejected;
    t.state_.records.swap(timeline.records); t.state_.now=timeline.now;
    return CampaignArchiveStatus::success;
}
CampaignArchiveStatus CampaignPersistence::load(std::span<const std::uint8_t> bytes,WorldState& w,RandomStreams& r,
                                                 Timeline& t,PersistenceLimits limits,const ContentCatalog* catalog) {
    const auto decoded=deserialize(bytes,limits);
    return decoded.status==CampaignArchiveStatus::success ? restore(*decoded.snapshot,w,r,t,limits,catalog) : decoded.status;
}
} // namespace corerpg
