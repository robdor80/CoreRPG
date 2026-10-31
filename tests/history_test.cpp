#include <corerpg/history.hpp>
#include <corerpg/content_catalog.hpp>
#include <gtest/gtest.h>
#include <atomic>
#include <chrono>
#include <fstream>

namespace {
using namespace corerpg;
using U=std::uint64_t;
constexpr U field(U n,U value){return ((0xf000+n)<<48)|value;}
struct HistoryTest : testing::Test {
    WorldState world;RandomStreams random{412};Timeline time;HistoryArchive archive;
    CatalogBuildResult catalog=ContentCatalog::create({ContentPackInput{
        ContentManifest{*PackId::parse("generic.test"),*SemanticVersion::parse("0.1.0"),*SemanticVersion::parse("0.1.0"),*SemanticVersion::parse("0.1.0")},
        {ContentDefinitionInput{ResourceDefinition{*DefinitionId::parse("generic.test:fact"),*ResourceKind::parse("structural")}}}}});
    std::filesystem::path directory;
    void SetUp() override {
        static std::atomic<U> n{};
        directory=std::filesystem::temp_directory_path()/("corerpg-history-test-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+"-"+std::to_string(++n));
        ASSERT_TRUE(catalog.succeeded());
        for(U i=1;i<=6;++i){auto id=world.create_entity();ASSERT_EQ(id.value(),i);ASSERT_EQ(world.bind_definition(id,*catalog.catalog(),*DefinitionId::parse("generic.test:fact")),DefinitionBindingChange::applied);}
        for(U i=3;i<=5;++i){ASSERT_EQ(world.add_relation({EntityId{i},RelationKind{1},EntityId{2}}),RelationChange::applied);
            ASSERT_EQ(world.add_relation({EntityId{i},RelationKind{field(1,1)},EntityId{i}}),RelationChange::applied);}
        ASSERT_EQ(world.add_relation({EntityId{3},RelationKind{2},EntityId{4}}),RelationChange::applied);
        ASSERT_EQ(world.place_entity(EntityId{3},EntityId{1}),ContainmentChange::applied);
        ASSERT_EQ(time.advance(world,random,100).status,TimelineStatus::completed);
    }
    void TearDown() override {std::error_code ec;std::filesystem::remove_all(directory,ec);}
    auto snapshot() {return CampaignPersistence::capture(world,random,time);}
    auto bytes() {return CampaignPersistence::serialize(snapshot()).bytes;}
    auto migrate(HistoryFault fault=HistoryFault::none) {
        HistoryCountRule rule{1,"generic.test:fact",RelationKind{1},{{{0xf001,HistoryPredicateOp::equal,1}}}};
        return HistoryPersistence::migrate(directory,HistoryCampaignId{1},world,random,time,archive,{rule},{},fault);
    }
    auto consolidate(HistoryFault fault=HistoryFault::none) {
        const std::array ids{HistorySelection{EntityId{3},0,10},HistorySelection{EntityId{4},10,20}};
        return HistoryPersistence::consolidate(archive,world,random,time,ids,90,catalog.catalog(),{},fault);
    }
    auto recover(HistoryFault fault=HistoryFault::none) {return HistoryPersistence::recover(directory,archive,world,random,time,catalog.catalog(),{},fault);}
    void forge_manifest(const std::function<void(std::vector<std::uint8_t>&)>& mutate) {
        const auto read=[](const auto& path){std::ifstream f(path,std::ios::binary);return std::vector<std::uint8_t>{std::istreambuf_iterator<char>(f),{}};};
        const auto write=[](const auto& path,const auto& bytes){std::ofstream f(path,std::ios::binary);f.write(reinterpret_cast<const char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));};
        const auto hex=[](const auto& hash){std::string s;const char* digits="0123456789abcdef";for(auto b:hash){s+=digits[b>>4];s+=digits[b&15];}return s;};
        auto pointer=read(directory/"CURRENT");HistoryHash root{};std::copy_n(pointer.begin()+20,32,root.begin());auto bytes=read(directory/(hex(root)+".obj"));mutate(bytes);
        auto forged=HistoryPersistence::hash(bytes);write(directory/(hex(forged)+".obj"),bytes);std::copy(forged.begin(),forged.end(),pointer.begin()+20);write(directory/"CURRENT",pointer);
    }
};
TEST(HistoryHash,KnownAnswers) {
    const auto str=[](const auto& hash){static const char* digits="0123456789abcdef";std::string s;for(auto b:hash){s+=digits[b>>4];s+=digits[b&15];}return s;};
    EXPECT_EQ(str(HistoryPersistence::hash({})),"e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    const std::string abc="abc";EXPECT_EQ(str(HistoryPersistence::hash({reinterpret_cast<const std::uint8_t*>(abc.data()),abc.size()})),"ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    std::vector<std::uint8_t> million(1'000'000,'a');EXPECT_EQ(str(HistoryPersistence::hash(million)),"cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0");
}
TEST(HistoryBoundary,UninitializedArchiveCannotPublishReadOrRetire) {
    HistoryArchive archive;WorldState world;RandomStreams random{1};Timeline time;
    ASSERT_TRUE(world.create_entity().is_valid());ASSERT_EQ(time.advance(world,random,100).status,TimelineStatus::completed);
    const auto original=CampaignPersistence::serialize(CampaignPersistence::capture(world,random,time)).bytes;
    const bool had_root=std::filesystem::exists("CURRENT"),had_lock=std::filesystem::exists("writer.lock");
    EXPECT_EQ(HistoryPersistence::validate_active(archive,CampaignPersistence::capture(world,random,time)),HistoryStatus::invalid_request);
    EXPECT_EQ(HistoryPersistence::publish(archive,world,random,time),HistoryStatus::invalid_request);
    EXPECT_EQ(HistoryPersistence::query(archive,{},1).status,HistoryStatus::invalid_request);
    const std::array ids{HistorySelection{EntityId{1},0,1}};
    EXPECT_EQ(HistoryPersistence::consolidate(archive,world,random,time,ids,1),HistoryStatus::invalid_request);
    EXPECT_EQ(CampaignPersistence::serialize(CampaignPersistence::capture(world,random,time)).bytes,original);
    EXPECT_EQ(std::filesystem::exists("CURRENT"),had_root);EXPECT_EQ(std::filesystem::exists("writer.lock"),had_lock);
}
TEST_F(HistoryTest,OriginalFactsAndCrossReferencesSurviveWithoutReactivation) {
    auto before=snapshot();ASSERT_EQ(migrate(),HistoryStatus::success);ASSERT_EQ(consolidate(),HistoryStatus::success);
    EXPECT_TRUE(world.contains(EntityId{3}));EXPECT_EQ(world.entity_count(),6);EXPECT_EQ(archive.resident_count(world),4);EXPECT_EQ(archive.count(1,EntityId{2}),2);
    EXPECT_EQ(archive.locate(EntityId{3},world.entity_count()),HistoryLocation::archived);EXPECT_EQ(archive.locate(EntityId{7},world.entity_count()),HistoryLocation::nonexistent);
    EXPECT_EQ(world.definition_of(EntityId{3}),nullptr);EXPECT_TRUE(world.relations_from(EntityId{3}).empty());
    auto page=HistoryPersistence::query(archive,{},1);ASSERT_EQ(page.status,HistoryStatus::success);ASSERT_EQ(page.facts.size(),1);EXPECT_TRUE(page.more);EXPECT_EQ(page.facts[0].identity.entity,EntityId{3});
    std::vector<EntityRelation> original;for(auto r:before.campaign.relations)if(r.source==EntityId{3})original.push_back(r);EXPECT_EQ(page.facts[0].relations,original);
    EXPECT_EQ(page.facts[0].definition,"generic.test:fact");EXPECT_EQ(page.facts[0].container,EntityId{1});
    auto second=HistoryPersistence::query(archive,page.next,1);ASSERT_EQ(second.facts.size(),1);EXPECT_EQ(second.facts[0].identity.entity,EntityId{4});EXPECT_FALSE(second.more);
    EXPECT_EQ(world.create_entity(),EntityId{7});ASSERT_EQ(HistoryPersistence::publish(archive,world,random,time),HistoryStatus::success);
    auto saved=bytes();ASSERT_EQ(recover(),HistoryStatus::success);EXPECT_EQ(bytes(),saved);EXPECT_EQ(archive.count(1,EntityId{2}),2);EXPECT_EQ(world.create_entity(),EntityId{8});
}
TEST_F(HistoryTest,EveryPrePublicationFaultPreservesRootAndAllMemory) {
    ASSERT_EQ(migrate(),HistoryStatus::success);const auto before=bytes();
    for(auto fault:{HistoryFault::before_segment,HistoryFault::after_segment,HistoryFault::after_checkpoint,HistoryFault::before_manifest,HistoryFault::before_publish}){
        EXPECT_EQ(consolidate(fault),HistoryStatus::injected_failure);EXPECT_EQ(bytes(),before);EXPECT_EQ(archive.generation(),1);
        ASSERT_EQ(recover(),HistoryStatus::success);EXPECT_EQ(bytes(),before);EXPECT_EQ(archive.archived_count(),0);
    }
    EXPECT_EQ(consolidate(),HistoryStatus::success);EXPECT_EQ(archive.generation(),2);
}
TEST_F(HistoryTest,LossAfterPointerPublicationRecoversWholeNewGeneration) {
    ASSERT_EQ(migrate(),HistoryStatus::success);const auto before=bytes();EXPECT_EQ(consolidate(HistoryFault::after_publish),HistoryStatus::injected_failure);
    EXPECT_EQ(bytes(),before);EXPECT_EQ(archive.generation(),1);EXPECT_EQ(HistoryPersistence::publish(archive,world,random,time),HistoryStatus::conflict);
    ASSERT_EQ(recover(),HistoryStatus::success);EXPECT_EQ(archive.generation(),2);EXPECT_EQ(archive.archived_count(),2);EXPECT_EQ(archive.count(1,EntityId{2}),2);
}
TEST_F(HistoryTest,ReintroducedPayloadOrIdentityRollbackCannotPublish) {
    ASSERT_EQ(migrate(),HistoryStatus::success);const auto before=snapshot();ASSERT_EQ(consolidate(),HistoryStatus::success);
    auto corrupt=before;corrupt.campaign.revision=world.revision();
    EXPECT_EQ(HistoryPersistence::validate_active(archive,corrupt),HistoryStatus::invalid_projection);
    auto bad=snapshot();bad.campaign.last_entity_id=5;EXPECT_EQ(HistoryPersistence::validate_active(archive,bad),HistoryStatus::invalid_projection);
    EXPECT_EQ(consolidate(),HistoryStatus::invalid_request);
}
TEST_F(HistoryTest,MissingTruncatedAndCorruptSegmentsAreErrorsNeverEmptyHistory) {
    ASSERT_EQ(migrate(),HistoryStatus::success);ASSERT_EQ(consolidate(),HistoryStatus::success);const auto good=bytes();
    const auto hash=archive.segments()[0].hash;const char* digits="0123456789abcdef";std::string name;for(auto b:hash){name+=digits[b>>4];name+=digits[b&15];}auto path=directory/(name+".obj");
    std::ifstream f(path,std::ios::binary);std::vector<std::uint8_t> original{std::istreambuf_iterator<char>(f),{}};f.close();
    for(unsigned mode=0;mode<3;++mode){if(mode==0)std::filesystem::remove(path);else{auto bad=original;if(mode==1)bad.resize(bad.size()-1);else bad[bad.size()/2]^=1;std::ofstream out(path,std::ios::binary);out.write(reinterpret_cast<const char*>(bad.data()),static_cast<std::streamsize>(bad.size()));}
        EXPECT_NE(recover(),HistoryStatus::success);EXPECT_EQ(bytes(),good);auto page=HistoryPersistence::query(archive,{},2);EXPECT_NE(page.status,HistoryStatus::success);EXPECT_TRUE(page.facts.empty());
        std::ofstream out(path,std::ios::binary);out.write(reinterpret_cast<const char*>(original.data()),static_cast<std::streamsize>(original.size()));out.close();ASSERT_EQ(recover(),HistoryStatus::success);}
}
TEST_F(HistoryTest,InjectedReadFailurePreservesDestinations) {
    ASSERT_EQ(migrate(),HistoryStatus::success);ASSERT_EQ(consolidate(),HistoryStatus::success);const auto good=bytes();
    EXPECT_EQ(recover(HistoryFault::read),HistoryStatus::injected_failure);EXPECT_EQ(bytes(),good);
    auto page=HistoryPersistence::query(archive,{},1,{},HistoryFault::read);EXPECT_EQ(page.status,HistoryStatus::injected_failure);EXPECT_TRUE(page.facts.empty());
}
TEST_F(HistoryTest,MigrationPreservesLegacyBytesAndRefusesExistingDestination) {
    const auto original=bytes();ASSERT_EQ(migrate(),HistoryStatus::success);EXPECT_EQ(bytes(),original);
    EXPECT_EQ(migrate(),HistoryStatus::conflict);EXPECT_EQ(bytes(),original);WorldState w;RandomStreams r(0);Timeline t;
    EXPECT_EQ(CampaignPersistence::load(original,w,r,t,{},catalog.catalog()),CampaignArchiveStatus::success);
    EXPECT_EQ(CampaignPersistence::serialize(CampaignPersistence::capture(w,r,t)).bytes,original);
}
TEST_F(HistoryTest,InvalidSelectionAndLiveContainmentPinArchive) {
    ASSERT_EQ(migrate(),HistoryStatus::success);auto before=bytes();
    const std::array duplicate{HistorySelection{EntityId{3},0,10},HistorySelection{EntityId{3},0,10}};
    EXPECT_EQ(HistoryPersistence::consolidate(archive,world,random,time,duplicate,90,catalog.catalog()),HistoryStatus::invalid_request);
    const std::array parent{HistorySelection{EntityId{1},0,10}};EXPECT_EQ(HistoryPersistence::consolidate(archive,world,random,time,parent,90,catalog.catalog()),HistoryStatus::dependency_pinned);
    EXPECT_EQ(bytes(),before);
}
TEST_F(HistoryTest,RulePredicatesCountExactlyAndAreVerifiedOnRecovery) {
    ASSERT_EQ(migrate(),HistoryStatus::success);ASSERT_EQ(consolidate(),HistoryStatus::success);
    const std::array more{HistorySelection{EntityId{5},1,30}};ASSERT_EQ(HistoryPersistence::consolidate(archive,world,random,time,more,95,catalog.catalog()),HistoryStatus::success);
    EXPECT_EQ(archive.count(1,EntityId{2}),3);ASSERT_EQ(recover(),HistoryStatus::success);EXPECT_EQ(archive.count(1,EntityId{2}),3);
    auto page=HistoryPersistence::query(archive,{},256);ASSERT_EQ(page.status,HistoryStatus::success);ASSERT_EQ(page.facts.size(),3);EXPECT_FALSE(page.more);
}
TEST_F(HistoryTest,StaleWriterCannotReplaceCommittedGeneration) {
    ASSERT_EQ(migrate(),HistoryStatus::success);auto stale=archive;ASSERT_EQ(consolidate(),HistoryStatus::success);
    EXPECT_EQ(HistoryPersistence::publish(stale,world,random,time),HistoryStatus::conflict);EXPECT_EQ(archive.generation(),2);
}
TEST_F(HistoryTest,PageAndRecordLimitsAreEnforced) {
    ASSERT_EQ(migrate(),HistoryStatus::success);ASSERT_EQ(consolidate(),HistoryStatus::success);
    EXPECT_EQ(HistoryPersistence::query(archive,{},0).status,HistoryStatus::invalid_request);
    EXPECT_EQ(HistoryPersistence::query(archive,{},257).status,HistoryStatus::invalid_request);
    auto limited=HistoryLimits{};limited.records.max_bytes=1;EXPECT_EQ(HistoryPersistence::query(archive,{},1,limited).status,HistoryStatus::limit_exceeded);
}
TEST_F(HistoryTest,UnknownManifestVersionWithValidHashFailsWithoutChangingLiveState) {
    ASSERT_EQ(migrate(),HistoryStatus::success);const auto before=bytes();forge_manifest([](auto& data){data[8]=99;});
    EXPECT_EQ(recover(),HistoryStatus::unsupported_version);EXPECT_EQ(bytes(),before);
}
TEST_F(HistoryTest,ForgedDescriptorChainIsRejectedEvenWithValidManifestHash) {
    ASSERT_EQ(migrate(),HistoryStatus::success);ASSERT_EQ(consolidate(),HistoryStatus::success);const auto before=bytes();
    // Independent wire offset: 116-byte fixed manifest prefix, descriptor previous hash at +80.
    forge_manifest([](auto& data){data.at(196)^=1;});EXPECT_EQ(recover(),HistoryStatus::integrity_error);EXPECT_EQ(bytes(),before);
}
TEST_F(HistoryTest,ForgedCountIsRejectedByOriginalFactRecomputation) {
    ASSERT_EQ(migrate(),HistoryStatus::success);ASSERT_EQ(consolidate(),HistoryStatus::success);const auto before=bytes();
    forge_manifest([](auto& data){data.at(data.size()-8)=3;});EXPECT_EQ(recover(),HistoryStatus::integrity_error);EXPECT_EQ(bytes(),before);
}
TEST_F(HistoryTest,ArchivedContainerAndPendingActionsCannotBecomeLive) {
    ASSERT_EQ(migrate(),HistoryStatus::success);ASSERT_EQ(consolidate(),HistoryStatus::success);auto before=snapshot();
    auto pending=before;pending.timeline.records.push_back({TimedAction{1,200,0,EntityId{3},RemoveContainmentAction{EntityId{3}}}});
    EXPECT_EQ(HistoryPersistence::validate_active(archive,pending),HistoryStatus::dependency_pinned);
    ASSERT_EQ(world.place_entity(EntityId{6},EntityId{3}),ContainmentChange::applied);
    EXPECT_EQ(HistoryPersistence::publish(archive,world,random,time),HistoryStatus::invalid_projection);
}
TEST_F(HistoryTest,ConsumerProjectionValidationPrecedesAnyPublication) {
    ASSERT_EQ(migrate(),HistoryStatus::success);const auto before=bytes();const std::array ids{HistorySelection{EntityId{3},0,20}};
    EXPECT_EQ(HistoryPersistence::consolidate(archive,world,random,time,ids,90,catalog.catalog(),{},HistoryFault::none,
        [](const auto&){return false;}),HistoryStatus::invalid_projection);
    EXPECT_EQ(bytes(),before);EXPECT_EQ(archive.generation(),1);ASSERT_EQ(recover(),HistoryStatus::success);EXPECT_EQ(bytes(),before);
}
}
