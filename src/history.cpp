#include <corerpg/history.hpp>
#include <algorithm>
#include <atomic>
#include <bit>
#include <fstream>
#include <limits>
#include <set>
#include <stdexcept>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>
#endif

namespace corerpg {
namespace {
using Bytes=std::vector<std::uint8_t>;
using U=std::uint64_t;
constexpr U scalar_mask=(U(1)<<48)-1;
struct Invalid { HistoryStatus status; };
void require(bool condition,HistoryStatus status=HistoryStatus::integrity_error) { if(!condition)throw Invalid{status}; }
struct Encoder {
    Bytes data;
    void put(U v,unsigned width=8) { for(unsigned i=0;i<width;++i)data.push_back(static_cast<std::uint8_t>(v>>(i*8))); }
    void bytes(std::span<const std::uint8_t> value) { data.insert(data.end(),value.begin(),value.end()); }
    void text(std::string_view value) { put(value.size());data.insert(data.end(),value.begin(),value.end()); }
};
struct Decoder {
    std::span<const std::uint8_t> data; std::size_t pos{};
    U get(unsigned width=8) {require(width<=data.size()-pos);U v=0;for(unsigned i=0;i<width;++i)v|=U(data[pos++])<<(i*8);return v;}
    auto bytes(U n) {require(n<=data.size()-pos);auto s=data.subspan(pos,static_cast<std::size_t>(n));pos+=static_cast<std::size_t>(n);return s;}
    std::string text(U max=4096) {auto n=get();require(n<=max,HistoryStatus::limit_exceeded);auto s=bytes(n);return {s.begin(),s.end()};}
    template<std::size_t N> std::array<std::uint8_t,N> array() {std::array<std::uint8_t,N> a{};auto s=bytes(N);std::copy(s.begin(),s.end(),a.begin());return a;}
    void magic(std::string_view value) {auto s=bytes(value.size());require(std::equal(s.begin(),s.end(),value.begin()));}
    void done() {require(pos==data.size());}
};
void magic(Encoder& e,std::string_view value) {e.data.insert(e.data.end(),value.begin(),value.end());e.put(1,4);}
void version(Decoder& d,std::string_view value) {d.magic(value);require(d.get(4)==1,HistoryStatus::unsupported_version);}
std::string hex(const HistoryHash& h) {static constexpr char digits[]="0123456789abcdef";std::string s;for(auto v:h){s.push_back(digits[v>>4]);s.push_back(digits[v&15]);}return s;}
auto object_path(const std::filesystem::path& dir,const HistoryHash& hash) {return dir/(hex(hash)+".obj");}
Bytes read_file(const std::filesystem::path& path,U maximum) {
    std::error_code ec;auto n=std::filesystem::file_size(path,ec);require(!ec,HistoryStatus::io_error);
    require(n<=maximum && n<=std::numeric_limits<std::size_t>::max(),HistoryStatus::limit_exceeded);
    std::ifstream f(path,std::ios::binary);require(bool(f),HistoryStatus::io_error);
    Bytes bytes(static_cast<std::size_t>(n));if(n)f.read(reinterpret_cast<char*>(bytes.data()),static_cast<std::streamsize>(n));
    require(bool(f) && f.peek()==std::char_traits<char>::eof(),HistoryStatus::io_error);return bytes;
}
struct WriterLock {
#ifdef _WIN32
    HANDLE handle{INVALID_HANDLE_VALUE};
    explicit WriterLock(const std::filesystem::path& dir) {
        handle=CreateFileW((dir/L"writer.lock").c_str(),GENERIC_READ|GENERIC_WRITE,0,nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
        require(handle!=INVALID_HANDLE_VALUE,HistoryStatus::conflict);
    }
    ~WriterLock(){if(handle!=INVALID_HANDLE_VALUE)CloseHandle(handle);}
#else
    int handle{-1};
    explicit WriterLock(const std::filesystem::path& dir) {
        handle=::open((dir/"writer.lock").c_str(),O_RDWR|O_CREAT,0600);
        require(handle>=0,HistoryStatus::io_error);
        if(::flock(handle,LOCK_EX|LOCK_NB)!=0){::close(handle);handle=-1;throw Invalid{HistoryStatus::conflict};}
    }
    ~WriterLock(){if(handle>=0)::close(handle);}
#endif
};
// Flush file contents, then atomically replace one pointer. POSIX also flushes the
// directory. Windows uses the documented write-through move; failures are surfaced.
void durable_write(const std::filesystem::path& path,std::span<const std::uint8_t> bytes) {
    static std::atomic<U> nonce{};
#ifdef _WIN32
    const auto pid=GetCurrentProcessId();
#else
    const auto pid=::getpid();
#endif
    auto tmp=path;tmp+=".tmp-"+std::to_string(pid)+"-"+std::to_string(++nonce);
#ifdef _WIN32
    HANDLE h=CreateFileW(tmp.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL|FILE_FLAG_WRITE_THROUGH,nullptr);
    require(h!=INVALID_HANDLE_VALUE,HistoryStatus::io_error);
    bool ok=true;std::size_t offset=0;
    while(offset<bytes.size()) {DWORD n=0;const auto chunk=static_cast<DWORD>(std::min(bytes.size()-offset,std::size_t(1024*1024)));
        if(!WriteFile(h,bytes.data()+offset,chunk,&n,nullptr)||n!=chunk){ok=false;break;}offset+=n;}
    ok=ok && FlushFileBuffers(h)!=0;const auto closed=CloseHandle(h)!=0;
    require(ok && closed,HistoryStatus::io_error);
    require(MoveFileExW(tmp.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0,HistoryStatus::io_error);
#else
    int fd=::open(tmp.c_str(),O_WRONLY|O_CREAT|O_EXCL,0600);require(fd>=0,HistoryStatus::io_error);
    bool ok=true;std::size_t offset=0;
    while(offset<bytes.size()){auto n=::write(fd,bytes.data()+offset,bytes.size()-offset);if(n<=0){ok=false;break;}offset+=static_cast<std::size_t>(n);}
    ok=ok && ::fsync(fd)==0;const auto closed=::close(fd)==0;require(ok && closed,HistoryStatus::io_error);
    require(::rename(tmp.c_str(),path.c_str())==0,HistoryStatus::io_error);
    fd=::open(path.parent_path().c_str(),O_RDONLY|O_DIRECTORY);require(fd>=0,HistoryStatus::io_error);
    ok=::fsync(fd)==0;const auto dir_closed=::close(fd)==0;require(ok && dir_closed,HistoryStatus::io_error);
#endif
}
void store_object(const std::filesystem::path& dir,const HistoryHash& hash,std::span<const std::uint8_t> bytes) {
    auto path=object_path(dir,hash);
    if(std::filesystem::exists(path)){require(read_file(path,bytes.size())==Bytes(bytes.begin(),bytes.end()));return;}
    durable_write(path,bytes);require(read_file(path,bytes.size())==Bytes(bytes.begin(),bytes.end()),HistoryStatus::io_error);
}
void inject(HistoryFault actual,HistoryFault requested){require(actual!=requested,HistoryStatus::injected_failure);}
bool in_ranges(std::span<const HistoryIdRange> ranges,U id) {
    auto it=std::upper_bound(ranges.begin(),ranges.end(),id,[](U value,const auto& range){return value<range.first;});
    return it!=ranges.begin() && id<=(--it)->last;
}
std::vector<HistoryIdRange> ranges_of(std::span<const HistorySelection> ids) {
    std::vector<HistoryIdRange> ranges;
    for(const auto& id:ids){const U n=id.entity.value();if(!ranges.empty() && n-ranges.back().last==1)ranges.back().last=n;else ranges.push_back({n,n});}
    return ranges;
}
void encode_ranges(Encoder& e,const std::vector<HistoryIdRange>& ids){e.put(ids.size());for(auto r:ids){e.put(r.first);e.put(r.last);}}
std::vector<HistoryIdRange> decode_ranges(Decoder& d,U max) {
    auto n=d.get();require(n<=max && n<=(d.data.size()-d.pos)/16,HistoryStatus::limit_exceeded);std::vector<HistoryIdRange> ids;
    for(U i=0;i<n;++i){HistoryIdRange r{d.get(),d.get()};require(r.first && r.first<=r.last && (ids.empty() || ids.back().last<r.first-1));ids.push_back(r);}return ids;
}
bool selected(std::span<const HistorySelection> ids,EntityId id) {
    auto it=std::lower_bound(ids.begin(),ids.end(),id,[](const auto& a,EntityId b){return a.entity<b;});return it!=ids.end() && it->entity==id;
}
bool zero(const auto& a){return std::all_of(a.begin(),a.end(),[](auto v){return v==0;});}
}

// Portable SHA-256 (FIPS 180-4), independent of native byte order.
HistoryHash HistoryPersistence::hash(std::span<const std::uint8_t> input) {
    constexpr std::array<std::uint32_t,64> k{
        0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
        0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
        0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
        0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
        0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
        0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
        0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
        0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2};
    std::array<std::uint32_t,8> h{0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19};
    const auto blocks=input.size()/64+((input.size()%64)<56?1:2);
    for(std::size_t block=0;block<blocks;++block){
        std::array<std::uint32_t,64> w{};
        for(std::size_t i=0;i<64;++i){auto pos=block*64+i;std::uint8_t b=0;
            if(pos<input.size())b=input[pos];else if(pos==input.size())b=0x80;
            else if(pos>=blocks*64-8)b=static_cast<std::uint8_t>((U(input.size())*8)>>((blocks*64-1-pos)*8));
            w[i/4]|=std::uint32_t(b)<<((3-i%4)*8);}
        for(std::size_t i=16;i<64;++i){auto a=w[i-15],b=w[i-2];w[i]=w[i-16]+(std::rotr(a,7)^std::rotr(a,18)^(a>>3))+w[i-7]+(std::rotr(b,17)^std::rotr(b,19)^(b>>10));}
        auto a=h[0],b=h[1],c=h[2],d=h[3],e=h[4],f=h[5],g=h[6],v=h[7];
        for(std::size_t i=0;i<64;++i){auto t=v+(std::rotr(e,6)^std::rotr(e,11)^std::rotr(e,25))+((e&f)^(~e&g))+k[i]+w[i];
            auto t2=(std::rotr(a,2)^std::rotr(a,13)^std::rotr(a,22))+((a&b)^(a&c)^(b&c));v=g;g=f;f=e;e=d+t;d=c;c=b;b=a;a=t+t2;}
        h[0]+=a;h[1]+=b;h[2]+=c;h[3]+=d;h[4]+=e;h[5]+=f;h[6]+=g;h[7]+=v;
    }
    HistoryHash out{};for(std::size_t i=0;i<32;++i)out[i]=static_cast<std::uint8_t>(h[i/4]>>((3-i%4)*8));return out;
}

struct HistoryPersistence::Segment {
    HistoryCampaignId campaign{};U sequence{},begin{},end{};HistoryHash previous{};
    std::vector<HistorySelection> ids;CampaignSnapshot facts;
    Bytes encode(HistoryLimits limits) const {
        auto core=Persistence::serialize(facts,limits.records);require(core.status==PersistenceStatus::success,HistoryStatus::invalid_projection);
        Encoder e;magic(e,"CRPGSEG1");e.bytes(campaign);e.put(sequence);e.bytes(previous);e.put(begin);e.put(end);e.put(ids.size());
        for(auto id:ids){e.put(id.entity.value());e.put(id.begin);e.put(id.end);}e.put(core.bytes.size());e.bytes(core.bytes);
        require(e.data.size()<=limits.records.max_bytes,HistoryStatus::limit_exceeded);return e.data;
    }
};
std::vector<EntityId> HistoryPersistence::bound_entities(const WorldState& w) {
    std::vector<EntityId> ids;ids.reserve(w.definitions_.size());for(const auto& d:w.definitions_)ids.push_back(d.first);return ids;
}
HistoryLocation HistoryArchive::locate(EntityId id,U issued) const noexcept {
    if(!id.is_valid() || id.value()>issued)return HistoryLocation::nonexistent;
    for(const auto& s:segments_)if(in_ranges(s.ids,id.value()))return HistoryLocation::archived;
    return HistoryLocation::resident;
}
U HistoryArchive::archived_count() const noexcept {U n=0;for(const auto& s:segments_)for(auto r:s.ids)n+=r.last-r.first+1;return n;}
U HistoryArchive::resident_count(const WorldState& w) const noexcept {return w.entity_count()-archived_count();}
U HistoryArchive::count(U rule,EntityId group) const noexcept {auto it=counts_.find({rule,group.value()});return it==counts_.end()?0:it->second;}

Bytes HistoryPersistence::encode_manifest(const HistoryArchive& a) {
    Encoder e;magic(e,"CRPGHST1");e.bytes(a.campaign_);e.put(a.generation_);e.put(a.issued_);e.put(a.revision_);e.put(a.frontier_);
    e.put(a.now_);e.bytes(a.checkpoint_);e.put(a.checkpoint_bytes_);e.put(a.segments_.size());
    for(const auto& s:a.segments_){e.put(s.sequence);e.put(s.bytes);e.put(s.begin);e.put(s.end);e.put(s.issued);e.put(s.revision);e.bytes(s.hash);e.bytes(s.previous);encode_ranges(e,s.ids);}
    e.put(a.rules_.size());for(const auto& r:a.rules_){e.put(r.key);e.text(r.definition);e.put(r.group_relation.value());e.put(r.alternatives.size());
        for(const auto& conjunction:r.alternatives){e.put(conjunction.size());for(auto p:conjunction){e.put(p.field,2);e.put(static_cast<U>(p.op),1);e.put(p.operand);}}}
    e.put(a.counts_.size());for(auto [key,count]:a.counts_){e.put(key.first);e.put(key.second);e.put(count);}return e.data;
}
HistoryStatus HistoryPersistence::decode_manifest(std::span<const std::uint8_t> bytes,HistoryArchive& a,HistoryLimits limits) {
    try {
        require(bytes.size()<=limits.max_manifest_bytes,HistoryStatus::limit_exceeded);Decoder d{bytes};version(d,"CRPGHST1");a.campaign_=d.array<16>();
        a.generation_=d.get();a.issued_=d.get();a.revision_=d.get();a.frontier_=d.get();a.now_=d.get();a.checkpoint_=d.array<32>();a.checkpoint_bytes_=d.get();
        require(!zero(a.campaign_) && a.generation_ && a.revision_>=a.issued_ && a.frontier_<=a.now_ && a.checkpoint_bytes_<=limits.records.max_bytes);
        auto count=d.get();require(count<=limits.max_segments,HistoryStatus::limit_exceeded);std::vector<HistoryIdRange> all;
        for(U i=0;i<count;++i){HistoryDescriptor s;s.sequence=d.get();s.bytes=d.get();s.begin=d.get();s.end=d.get();s.issued=d.get();s.revision=d.get();s.hash=d.array<32>();s.previous=d.array<32>();s.ids=decode_ranges(d,limits.max_selected);
            require(s.sequence==i+1 && s.bytes<=limits.records.max_bytes && s.begin<s.end && s.end<=a.frontier_ && !s.ids.empty());
            require(s.issued<=a.issued_ && s.revision<=a.revision_ && s.revision>=s.issued && s.ids.back().last<=s.issued);
            require(s.previous==(a.segments_.empty()?HistoryHash{}:a.segments_.back().hash));
            for(auto r:s.ids)all.push_back(r);a.segments_.push_back(std::move(s));}
        std::sort(all.begin(),all.end(),[](auto x,auto y){return x.first<y.first;});for(std::size_t i=1;i<all.size();++i)require(all[i-1].last<all[i].first);
        count=d.get();require(count<=64,HistoryStatus::limit_exceeded);std::set<U> keys;
        for(U i=0;i<count;++i){HistoryCountRule r;r.key=d.get();r.definition=d.text(limits.records.max_definition_bytes);r.group_relation=RelationKind{d.get()};
            require(r.key && keys.insert(r.key).second && !r.definition.empty() && r.group_relation.value());auto terms=d.get();require(terms<=64,HistoryStatus::limit_exceeded);
            for(U j=0;j<terms;++j){auto n=d.get();require(n<=64,HistoryStatus::limit_exceeded);std::vector<HistoryPredicate> predicates;
                for(U z=0;z<n;++z){HistoryPredicate p;p.field=static_cast<std::uint16_t>(d.get(2));auto op=d.get(1);require(op<=3);p.op=static_cast<HistoryPredicateOp>(op);p.operand=d.get();require(p.field && p.operand<=scalar_mask);if(p.op==HistoryPredicateOp::less_field)require(p.operand>0 && p.operand<=65535);predicates.push_back(p);}r.alternatives.push_back(std::move(predicates));}a.rules_.push_back(std::move(r));}
        count=d.get();require(count<=limits.records.max_relations && count<=(d.data.size()-d.pos)/24,HistoryStatus::limit_exceeded);
        std::pair<U,U> last{};for(U i=0;i<count;++i){std::pair<U,U> key{d.get(),d.get()};auto n=d.get();require(keys.contains(key.first) && key.second && key.second<=a.issued_ && n && last<key);last=key;a.counts_.emplace(key,n);}
        d.done();return HistoryStatus::success;
    }catch(const Invalid& e){return e.status;}
}

HistoryStatus HistoryPersistence::read_segment(const HistoryArchive& a,const HistoryDescriptor& desc,Segment& s,HistoryLimits limits,HistoryFault fault) {
    try {
        inject(fault,HistoryFault::read);auto bytes=read_file(object_path(a.directory_,desc.hash),limits.records.max_bytes);require(bytes.size()==desc.bytes && hash(bytes)==desc.hash);
        Decoder d{bytes};version(d,"CRPGSEG1");s.campaign=d.array<16>();s.sequence=d.get();s.previous=d.array<32>();s.begin=d.get();s.end=d.get();
        require(s.campaign==a.campaign_ && s.sequence==desc.sequence && s.previous==desc.previous && s.begin==desc.begin && s.end==desc.end);
        auto count=d.get();require(count<=limits.max_selected && count<=(d.data.size()-d.pos)/24,HistoryStatus::limit_exceeded);
        for(U i=0;i<count;++i){HistorySelection id{EntityId{d.get()},d.get(),d.get()};require(id.entity.is_valid() && (s.ids.empty() || s.ids.back().entity<id.entity) && s.begin<=id.begin && id.begin<id.end && id.end<=s.end);s.ids.push_back(id);}
        require(ranges_of(s.ids)==desc.ids);auto n=d.get();auto decoded=Persistence::deserialize(d.bytes(n),limits.records);d.done();require(decoded.status==PersistenceStatus::success && decoded.snapshot.has_value());s.facts=std::move(*decoded.snapshot);
        require(s.facts.last_entity_id==desc.issued && s.facts.revision==desc.revision && !s.facts.master_seed && s.facts.streams.empty() && s.facts.populations.empty() && s.facts.population_memberships.empty());
        for(const auto& r:s.facts.definitions)require(selected(s.ids,r.entity));
        for(const auto& r:s.facts.relations)require(selected(s.ids,r.source));
        for(const auto& r:s.facts.containment)require(selected(s.ids,r.entity));
        for(const auto& r:s.facts.knowledge)require(selected(s.ids,r.observer));
        return HistoryStatus::success;
    }catch(const Invalid& e){return e.status;}catch(const std::filesystem::filesystem_error&){return HistoryStatus::io_error;}
}

void HistoryPersistence::reduce(HistoryArchive& a,const Segment& s) {
    // Single pass by source; temporary tables never exceed one segment.
    for(const auto& binding:s.facts.definitions){
        auto first=std::lower_bound(s.facts.relations.begin(),s.facts.relations.end(),binding.entity,[](const auto& r,EntityId id){return r.source<id;});
        auto last=first;while(last!=s.facts.relations.end() && last->source==binding.entity)++last;
        for(const auto& rule:a.rules_)if(rule.definition==binding.definition_id){
            std::map<U,U> scalars;U group=0;
            for(auto it=first;it!=last;++it){if(it->kind==rule.group_relation){require(!group);group=it->target.value();}
                if(it->source==it->target){auto field=it->kind.value()>>48;if(field){require(!scalars.contains(field));scalars[field]=it->kind.value()&scalar_mask;}}}
            bool match=rule.alternatives.empty();for(const auto& term:rule.alternatives){bool yes=true;for(auto p:term){auto value=scalars.find(p.field);
                switch(p.op){case HistoryPredicateOp::present:yes &=value!=scalars.end();break;case HistoryPredicateOp::absent:yes &=value==scalars.end();break;
                case HistoryPredicateOp::equal:yes &=value!=scalars.end() && value->second==p.operand;break;
                case HistoryPredicateOp::less_field:{auto rhs=scalars.find(p.operand);yes &=value!=scalars.end() && rhs!=scalars.end() && value->second<rhs->second;break;}}}match|=yes;}
            if(match){require(group && !selected(s.ids,EntityId{group}),HistoryStatus::dependency_pinned);auto& count=a.counts_[{rule.key,group}];require(count!=std::numeric_limits<U>::max(),HistoryStatus::limit_exceeded);++count;}
        }
    }
}

HistoryStatus HistoryPersistence::validate_active(const HistoryArchive& a,const CampaignArchiveSnapshot& snapshot,HistoryLimits limits) {
    const auto& c=snapshot.campaign;
    if(CampaignPersistence::validate(snapshot,limits.records)!=CampaignArchiveStatus::success || c.last_entity_id<a.issued_ || c.revision<a.revision_ || snapshot.timeline.now<a.now_)return HistoryStatus::invalid_projection;
    auto archived=[&](EntityId id){return a.locate(id,c.last_entity_id)==HistoryLocation::archived;};
    for(const auto& entry:a.counts_)if(archived(EntityId{entry.first.second}))return HistoryStatus::dependency_pinned;
    for(const auto& d:c.definitions)if(archived(d.entity))return HistoryStatus::invalid_projection;
    for(auto r:c.relations)if(archived(r.source))return HistoryStatus::invalid_projection;
    // Historical containers/groups/observers are not live operational structures.
    for(auto r:c.containment)if(archived(r.entity)||archived(r.container))return HistoryStatus::invalid_projection;
    for(auto r:c.populations)if(archived(r.group))return HistoryStatus::invalid_projection;
    for(auto r:c.population_memberships)if(archived(r.entity)||archived(r.population))return HistoryStatus::invalid_projection;
    for(const auto& r:c.knowledge)if(archived(r.observer))return HistoryStatus::invalid_projection;
    for(const auto& r:snapshot.timeline.records)if(r.state==TimedState::pending){
        bool bad=archived(r.action.actor);std::visit([&](const auto& op){using T=std::decay_t<decltype(op)>;
            if constexpr(std::is_same_v<T,PlaceEntityAction>)bad|=archived(op.entity)||archived(op.container);
            else if constexpr(std::is_same_v<T,RemoveContainmentAction>)bad|=archived(op.entity);
            else if constexpr(std::is_same_v<T,AddRelationAction>||std::is_same_v<T,RemoveRelationAction>)bad|=archived(op.relation.source);
        },r.action.operation);if(bad)return HistoryStatus::dependency_pinned;}
    return HistoryStatus::success;
}

HistoryStatus HistoryPersistence::commit(HistoryArchive& a,const CampaignArchiveSnapshot& snapshot,const Segment* segment,HistoryLimits limits,HistoryFault fault) {
    try {
        WriterLock lock(a.directory_);const auto current=a.directory_/"CURRENT";
        if(a.generation_){auto pointer=read_file(current,128);Decoder d{pointer};version(d,"CRPGROOT");require(d.get()==a.generation_ && d.array<32>()==a.root_,HistoryStatus::conflict);d.done();}
        else require(!std::filesystem::exists(current),HistoryStatus::conflict);
        // Validate the complete existing generation before extending it. Incomplete
        // storage is an error, never a reason to publish a partially valid history.
        for(const auto& desc:a.segments_){Segment verified;auto result=read_segment(a,desc,verified,limits,fault);require(result==HistoryStatus::success,result);}
        auto next=a;inject(fault,HistoryFault::before_segment);
        if(segment){auto bytes=segment->encode(limits);HistoryDescriptor desc;desc.sequence=segment->sequence;desc.bytes=bytes.size();desc.begin=segment->begin;desc.end=segment->end;
            desc.issued=segment->facts.last_entity_id;desc.revision=segment->facts.revision;desc.previous=segment->previous;desc.ids=ranges_of(segment->ids);desc.hash=hash(bytes);
            require(next.segments_.size()<limits.max_segments,HistoryStatus::limit_exceeded);next.segments_.push_back(desc);next.frontier_=std::max(next.frontier_,segment->end);reduce(next,*segment);
            store_object(a.directory_,desc.hash,bytes);Segment verified;auto status=read_segment(next,desc,verified,limits,HistoryFault::none);require(status==HistoryStatus::success,status);}
        inject(fault,HistoryFault::after_segment);
        require(next.generation_!=std::numeric_limits<U>::max(),HistoryStatus::limit_exceeded);++next.generation_;next.issued_=snapshot.campaign.last_entity_id;next.revision_=snapshot.campaign.revision;next.now_=snapshot.timeline.now;
        auto status=validate_active(next,snapshot,limits);require(status==HistoryStatus::success,status);
        auto bytes=CampaignPersistence::serialize(snapshot,limits.records);require(bytes.status==CampaignArchiveStatus::success,HistoryStatus::invalid_projection);
        next.checkpoint_=hash(bytes.bytes);next.checkpoint_bytes_=bytes.bytes.size();store_object(a.directory_,next.checkpoint_,bytes.bytes);inject(fault,HistoryFault::after_checkpoint);
        inject(fault,HistoryFault::before_manifest);auto manifest=encode_manifest(next);require(manifest.size()<=limits.max_manifest_bytes,HistoryStatus::limit_exceeded);
        // Round-trip validation checks rules, descriptor ownership, and canonical ranges.
        HistoryArchive decoded;status=decode_manifest(manifest,decoded,limits);require(status==HistoryStatus::success,status);
        next.root_=hash(manifest);store_object(a.directory_,next.root_,manifest);Encoder pointer;magic(pointer,"CRPGROOT");pointer.put(next.generation_);pointer.bytes(next.root_);
        inject(fault,HistoryFault::before_publish);durable_write(current,pointer.data);inject(fault,HistoryFault::after_publish);
        a=std::move(next);return HistoryStatus::success;
    }catch(const Invalid& e){return e.status;}catch(const std::filesystem::filesystem_error&){return HistoryStatus::io_error;}
}

HistoryStatus HistoryPersistence::migrate(const std::filesystem::path& path,HistoryCampaignId campaign,const WorldState& w,const RandomStreams& r,const Timeline& t,HistoryArchive& out,std::vector<HistoryCountRule> rules,HistoryLimits limits,HistoryFault fault) {
    try {
        require(!zero(campaign),HistoryStatus::invalid_request);auto snapshot=CampaignPersistence::capture(w,r,t);
        require(CampaignPersistence::validate(snapshot,limits.records)==CampaignArchiveStatus::success,HistoryStatus::invalid_projection);
        HistoryArchive next;next.directory_=std::filesystem::absolute(path);next.campaign_=campaign;next.rules_=std::move(rules);
        // Never adopt, overwrite or clean up a pre-existing destination, even if empty.
        require(std::filesystem::create_directory(next.directory_),HistoryStatus::conflict);
        auto status=commit(next,snapshot,nullptr,limits,fault);if(status==HistoryStatus::success)out=std::move(next);return status;
    }catch(const Invalid& e){return e.status;}catch(const std::filesystem::filesystem_error&){return HistoryStatus::io_error;}
}

HistoryStatus HistoryPersistence::consolidate(HistoryArchive& a,WorldState& w,RandomStreams& r,Timeline& t,std::span<const HistorySelection> ids,U frontier,const ContentCatalog* catalog,HistoryLimits limits,HistoryFault fault) {
    try {
        require(!ids.empty() && ids.size()<=limits.max_selected && frontier>=a.frontier_ && frontier<=t.now(),HistoryStatus::invalid_request);
        auto original=CampaignPersistence::capture(w,r,t);auto status=validate_active(a,original,limits);require(status==HistoryStatus::success,status);
        for(std::size_t i=0;i<ids.size();++i){auto id=ids[i];require(id.entity.is_valid() && id.entity.value()<=w.entity_count() && (!i || ids[i-1].entity<id.entity) && id.begin<id.end && id.end<=frontier,HistoryStatus::invalid_request);
            require(a.locate(id.entity,w.entity_count())==HistoryLocation::resident,HistoryStatus::invalid_request);}
        const auto chosen=[&](EntityId id){return selected(ids,id);};
        for(auto p:original.campaign.populations)require(!chosen(p.group),HistoryStatus::dependency_pinned);
        for(auto p:original.campaign.population_memberships)require(!chosen(p.entity)&&!chosen(p.population),HistoryStatus::dependency_pinned);
        for(auto p:original.campaign.containment)require(chosen(p.entity)||!chosen(p.container),HistoryStatus::dependency_pinned);
        Segment segment;segment.campaign=a.campaign_;segment.sequence=a.segments_.size()+1;segment.previous=a.segments_.empty()?HistoryHash{}:a.segments_.back().hash;
        segment.begin=frontier;segment.end=frontier;segment.ids.assign(ids.begin(),ids.end());for(auto id:ids)segment.begin=std::min(segment.begin,id.begin);
        auto& past=segment.facts;past.format_version=save_format_version_v3;past.last_entity_id=original.campaign.last_entity_id;past.revision=original.campaign.revision;
        auto& active=original.campaign;
        for(const auto& d:active.definitions)if(chosen(d.entity))past.definitions.push_back(d);
        for(const auto& d:active.relations)if(chosen(d.source))past.relations.push_back(d);
        for(const auto& d:active.containment)if(chosen(d.entity))past.containment.push_back(d);
        for(const auto& d:active.knowledge)if(chosen(d.observer))past.knowledge.push_back(d);
        std::erase_if(active.definitions,[&](const auto& d){return chosen(d.entity);});std::erase_if(active.relations,[&](const auto& d){return chosen(d.source);});
        std::erase_if(active.containment,[&](const auto& d){return chosen(d.entity);});std::erase_if(active.knowledge,[&](const auto& d){return chosen(d.observer);});
        require(active.revision!=std::numeric_limits<U>::max(),HistoryStatus::limit_exceeded);++active.revision;
        // Stage every potentially allocating restore BEFORE durable publication.
        WorldState nw;RandomStreams nr{0};Timeline nt;
        require(CampaignPersistence::restore(original,nw,nr,nt,limits.records,catalog)==CampaignArchiveStatus::success,HistoryStatus::invalid_projection);
        status=commit(a,original,&segment,limits,fault);if(status!=HistoryStatus::success)return status;
        w=std::move(nw);r=std::move(nr);t.state_.records.swap(nt.state_.records);std::swap(t.state_.now,nt.state_.now);return HistoryStatus::success;
    }catch(const Invalid& e){return e.status;}
}
HistoryStatus HistoryPersistence::publish(HistoryArchive& a,const WorldState& w,const RandomStreams& r,const Timeline& t,HistoryLimits limits,HistoryFault fault) {
    auto snapshot=CampaignPersistence::capture(w,r,t);auto status=validate_active(a,snapshot,limits);if(status!=HistoryStatus::success)return status;
    return commit(a,snapshot,nullptr,limits,fault);
}

HistoryStatus HistoryPersistence::recover(const std::filesystem::path& path,HistoryArchive& out,WorldState& w,RandomStreams& r,Timeline& t,const ContentCatalog* catalog,HistoryLimits limits,HistoryFault fault) {
    try {
        inject(fault,HistoryFault::read);auto dir=std::filesystem::absolute(path);auto pointer=read_file(dir/"CURRENT",128);Decoder d{pointer};version(d,"CRPGROOT");auto generation=d.get();auto root=d.array<32>();d.done();
        auto bytes=read_file(object_path(dir,root),limits.max_manifest_bytes);require(hash(bytes)==root);HistoryArchive next;auto status=decode_manifest(bytes,next,limits);require(status==HistoryStatus::success,status);
        next.directory_=dir;next.root_=root;require(next.generation_==generation);auto computed=next;computed.counts_.clear();
        for(const auto& desc:next.segments_){Segment segment;status=read_segment(next,desc,segment,limits,fault);require(status==HistoryStatus::success,status);reduce(computed,segment);}
        require(computed.counts_==next.counts_);bytes=read_file(object_path(dir,next.checkpoint_),limits.records.max_bytes);require(bytes.size()==next.checkpoint_bytes_ && hash(bytes)==next.checkpoint_);
        auto snapshot=CampaignPersistence::deserialize(bytes,limits.records);require(snapshot.status==CampaignArchiveStatus::success && snapshot.snapshot.has_value());
        require(snapshot.snapshot->campaign.last_entity_id==next.issued_ && snapshot.snapshot->campaign.revision==next.revision_ && snapshot.snapshot->timeline.now==next.now_);status=validate_active(next,*snapshot.snapshot,limits);require(status==HistoryStatus::success,status);
        WorldState nw;RandomStreams nr{0};Timeline nt;require(CampaignPersistence::restore(*snapshot.snapshot,nw,nr,nt,limits.records,catalog)==CampaignArchiveStatus::success,HistoryStatus::invalid_projection);
        out=std::move(next);w=std::move(nw);r=std::move(nr);t.state_.records.swap(nt.state_.records);std::swap(t.state_.now,nt.state_.now);return HistoryStatus::success;
    }catch(const Invalid& e){return e.status;}catch(const std::filesystem::filesystem_error&){return HistoryStatus::io_error;}
}

HistoryPage HistoryPersistence::query(const HistoryArchive& a,EntityId after,U limit,HistoryLimits limits,HistoryFault fault) {
    HistoryPage page;page.next=after;
    try {
        require(limit && limit<=limits.max_page,HistoryStatus::invalid_request);
        // Select only limit+1 IDs using the small manifest range index. Segment creation
        // order may differ from identity order; the public cursor is always global ID.
        std::vector<std::pair<U,std::size_t>> candidates;
        for(std::size_t i=0;i<a.segments_.size();++i)for(auto range:a.segments_[i].ids){if(range.last<=after.value())continue;auto first=std::max(range.first,after.value()+1);
            for(U id=first;;++id){
                if(candidates.size()==limit+1 && id>=candidates.back().first)break;
                auto item=std::pair{id,i};candidates.insert(std::lower_bound(candidates.begin(),candidates.end(),item),item);
                if(candidates.size()>limit+1)candidates.pop_back();
                if(id==range.last || id-first==limit)break;}}
        std::sort(candidates.begin(),candidates.end());if(candidates.size()>limit){page.more=true;candidates.resize(static_cast<std::size_t>(limit));}
        std::map<std::size_t,std::vector<U>> wanted;for(auto [id,index]:candidates)wanted[index].push_back(id);
        for(const auto& [index,ids]:wanted){Segment s;auto status=read_segment(a,a.segments_[index],s,limits,fault);require(status==HistoryStatus::success,status);
            for(auto id:ids){HistoryFact fact;auto it=std::lower_bound(s.ids.begin(),s.ids.end(),EntityId{id},[](const auto& x,EntityId y){return x.entity<y;});require(it!=s.ids.end() && it->entity.value()==id);fact.identity=*it;
                auto def=std::lower_bound(s.facts.definitions.begin(),s.facts.definitions.end(),EntityId{id},[](const auto& x,EntityId y){return x.entity<y;});if(def!=s.facts.definitions.end() && def->entity.value()==id)fact.definition=def->definition_id;
                auto rel=std::lower_bound(s.facts.relations.begin(),s.facts.relations.end(),EntityId{id},[](const auto& x,EntityId y){return x.source<y;});for(;rel!=s.facts.relations.end() && rel->source.value()==id;++rel)fact.relations.push_back(*rel);
                auto con=std::lower_bound(s.facts.containment.begin(),s.facts.containment.end(),EntityId{id},[](const auto& x,EntityId y){return x.entity<y;});if(con!=s.facts.containment.end() && con->entity.value()==id)fact.container=con->container;
                for(const auto& k:s.facts.knowledge)if(k.observer.value()==id)fact.knowledge.push_back(k);page.facts.push_back(std::move(fact));}}
        std::sort(page.facts.begin(),page.facts.end(),[](const auto& x,const auto& y){return x.identity.entity<y.identity.entity;});if(!page.facts.empty())page.next=page.facts.back().identity.entity;
    }catch(const Invalid& e){page.status=e.status;page.facts.clear();page.more=false;page.next=after;}catch(const std::filesystem::filesystem_error&){page.status=HistoryStatus::io_error;page.facts.clear();page.more=false;page.next=after;}
    return page;
}
} // namespace corerpg
