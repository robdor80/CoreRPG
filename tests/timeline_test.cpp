#include <corerpg/campaign_persistence.hpp>
#include <gtest/gtest.h>
#include <array>
#include <limits>

namespace {
using namespace corerpg;
constexpr EntityId ship{1}, station{2}, other{3};
class TimelineTest : public ::testing::Test {
protected:
    WorldState world;
    RandomStreams rng{2372};
    Timeline time;
    void SetUp() override { for(int i=0;i<3;++i) ASSERT_TRUE(world.create_entity().is_valid()); }
    std::vector<std::uint8_t> save() { return CampaignPersistence::serialize(CampaignPersistence::capture(world,rng,time)).bytes; }
    TimedAction place(std::uint64_t id,std::uint64_t due,std::uint64_t parent=0) {
        return {id,due,parent,ship,PlaceEntityAction{ship,station}};
    }
};
TEST_F(TimelineTest, OffscreenOrderAndIdempotency) {
    ASSERT_EQ(time.schedule(world,place(1,20)),TimelineStatus::completed);
    ASSERT_EQ(time.schedule(world,{2,10,0,other,PlaceEntityAction{other,station}}),TimelineStatus::completed);
    ASSERT_EQ(time.schedule(world,{3,20,1,ship,RemoveContainmentAction{ship}}),TimelineStatus::completed);
    EXPECT_EQ(time.advance(world,rng,30).processed,3U);
    EXPECT_EQ(time.now(),30U);
    EXPECT_EQ(world.container_of(ship).status,ContainmentStatus::uncontained);
    EXPECT_EQ(world.container_of(other).container,station);
    const auto before=save();
    EXPECT_EQ(time.advance(world,rng,30).processed,0U);
    EXPECT_EQ(time.schedule(world,place(1,20)),TimelineStatus::no_change);
    EXPECT_EQ(time.schedule(world,place(1,21)),TimelineStatus::key_conflict);
    EXPECT_EQ(before,save());
}
TEST_F(TimelineTest, BoundedResumeDoesNotAdvancePastUnprocessedActions) {
    ASSERT_EQ(time.schedule(world,place(1,10)),TimelineStatus::completed);
    ASSERT_EQ(time.schedule(world,{2,10,1,ship,StreamRollAction{7,0,100}}),TimelineStatus::completed);
    EXPECT_EQ(time.advance(world,rng,100,0).status,TimelineStatus::action_limit);
    EXPECT_EQ(time.now(),0U);
    EXPECT_EQ(time.advance(world,rng,100,1).status,TimelineStatus::action_limit);
    EXPECT_EQ(time.now(),10U);
    const auto mid=save();
    WorldState recovered; RandomStreams r{0}; Timeline t;
    ASSERT_EQ(CampaignPersistence::load(mid,recovered,r,t),CampaignArchiveStatus::success);
    EXPECT_EQ(t.advance(recovered,r,100).processed,1U);
    EXPECT_EQ(time.advance(world,rng,100).processed,1U);
    EXPECT_EQ(CampaignPersistence::serialize(CampaignPersistence::capture(recovered,r,t)).bytes,save());
}
TEST_F(TimelineTest, RejectedParentBlocksChildWithoutRollOrRetry) {
    ASSERT_EQ(time.schedule(world,{1,10,0,ship,PlaceEntityAction{ship,ship}}),TimelineStatus::completed);
    ASSERT_EQ(time.schedule(world,{2,10,1,ship,StreamRollAction{7,0,100}}),TimelineStatus::completed);
    EXPECT_EQ(time.advance(world,rng,10).processed,2U);
    EXPECT_EQ(time.records()[0].state,TimedState::rejected);
    EXPECT_EQ(time.records()[1].state,TimedState::causal_rejected);
    EXPECT_EQ(rng.stream_count(),0U);
    const auto before=save();
    EXPECT_EQ(time.advance(world,rng,10).processed,0U);
    EXPECT_EQ(before,save());
}
TEST_F(TimelineTest, InvalidSchedulesAndBackwardsTimePreserveState) {
    auto before=save();
    EXPECT_EQ(time.schedule(world,place(2,10)),TimelineStatus::invalid_request);
    EXPECT_EQ(time.schedule(world,place(1,10,1)),TimelineStatus::invalid_request);
    EXPECT_EQ(time.schedule(world,{1,10,0,EntityId{99},StreamRollAction{0,0,1}}),TimelineStatus::invalid_request);
    EXPECT_EQ(time.schedule(world,{1,10,0,ship,StreamRollAction{0,10,1}}),TimelineStatus::invalid_request);
    EXPECT_EQ(time.schedule(world,{1,10,0,ship,PlaceEntityAction{ship,EntityId{99}}}),TimelineStatus::invalid_request);
    EXPECT_EQ(before,save());
    ASSERT_EQ(time.schedule(world,place(1,20)),TimelineStatus::completed);
    EXPECT_EQ(time.schedule(world,place(2,19,1)),TimelineStatus::invalid_request);
    ASSERT_EQ(time.advance(world,rng,100).status,TimelineStatus::completed);
    before=save();
    EXPECT_EQ(time.schedule(world,place(2,99)),TimelineStatus::invalid_request);
    EXPECT_EQ(time.advance(world,rng,99).status,TimelineStatus::backwards_time);
    EXPECT_EQ(before,save());
}
TEST_F(TimelineTest, AllOperationTypesPersistAndExecute) {
    const EntityRelation edge{ship,RelationKind{7},other};
    const std::array<ActionOperation,5> ops{PlaceEntityAction{ship,station},RemoveContainmentAction{ship},
        AddRelationAction{edge},RemoveRelationAction{edge},StreamRollAction{9,1,6}};
    for(std::size_t i=0;i<ops.size();++i) ASSERT_EQ(time.schedule(world,{i+1,i+1,0,ship,ops[i]}),TimelineStatus::completed);
    const auto pending=save();
    WorldState recovered; RandomStreams r{0}; Timeline t;
    ASSERT_EQ(CampaignPersistence::load(pending,recovered,r,t),CampaignArchiveStatus::success);
    ASSERT_EQ(time.advance(world,rng,10).processed,5U);
    ASSERT_EQ(t.advance(recovered,r,10).processed,5U);
    const auto completed=save();
    EXPECT_EQ(CampaignPersistence::serialize(CampaignPersistence::capture(recovered,r,t)).bytes,completed);
    ASSERT_EQ(CampaignPersistence::load(completed,recovered,r,t),CampaignArchiveStatus::success);
    EXPECT_EQ(t.advance(recovered,r,10).processed,0U);
    EXPECT_EQ(world.container_of(ship).status,ContainmentStatus::uncontained);
    EXPECT_FALSE(world.has_relation(edge));
    EXPECT_TRUE(time.records().back().has_roll);
}
TEST_F(TimelineTest, KnowledgeAndFutureRngSurviveNewSession) {
    ASSERT_EQ(world.learn_fact(ship,{other}),KnowledgeChange::applied);
    ASSERT_EQ(time.schedule(world,{1,10,0,ship,StreamRollAction{9,0,UINT64_MAX}}),TimelineStatus::completed);
    ASSERT_EQ(time.advance(world,rng,10).processed,1U);
    const auto bytes=save();
    WorldState w; RandomStreams r{1}; Timeline t;
    ASSERT_EQ(CampaignPersistence::load(bytes,w,r,t),CampaignArchiveStatus::success);
    EXPECT_EQ(w.knowledge_of(ship).size(),1U);
    EXPECT_EQ(r.stream(9).next(),rng.stream(9).next());
    EXPECT_EQ(r.stream(123).next(),rng.stream(123).next());
}
TEST_F(TimelineTest, EveryTruncationCorruptionAndFutureVersionFailSafely) {
    ASSERT_EQ(time.schedule(world,place(1,10)),TimelineStatus::completed);
    const auto valid=save();
    for(std::size_t i=0;i<valid.size();++i) {
        EXPECT_NE(CampaignPersistence::load(std::span{valid}.first(i),world,rng,time),CampaignArchiveStatus::success) << i;
        auto corrupt=valid; corrupt[i]^=0x80;
        EXPECT_NE(CampaignPersistence::load(corrupt,world,rng,time),CampaignArchiveStatus::success) << i;
    }
    auto future=valid; future[8]=2;
    EXPECT_EQ(CampaignPersistence::load(future,world,rng,time),CampaignArchiveStatus::unsupported_version);
    EXPECT_EQ(valid,save());
    PersistenceLimits limits; limits.max_bytes=valid.size()-1;
    EXPECT_EQ(CampaignPersistence::deserialize(valid,limits).status,CampaignArchiveStatus::limit_exceeded);
}
TEST_F(TimelineTest, SemanticCandidatesCannotDestroyValidCampaign) {
    ASSERT_EQ(time.schedule(world,place(1,10)),TimelineStatus::completed);
    const auto valid=save();
    const auto original=CampaignPersistence::capture(world,rng,time);
    auto bad=original; bad.timeline.records[0].action.id=2;
    EXPECT_EQ(CampaignPersistence::restore(bad,world,rng,time),CampaignArchiveStatus::invalid_timeline);
    bad=original; bad.timeline.now=11;
    EXPECT_EQ(CampaignPersistence::restore(bad,world,rng,time),CampaignArchiveStatus::invalid_timeline);
    bad=original; bad.timeline.records[0].state=TimedState::causal_rejected;
    EXPECT_EQ(CampaignPersistence::restore(bad,world,rng,time),CampaignArchiveStatus::invalid_timeline);
    bad=original; bad.campaign.last_entity_id=0;
    EXPECT_NE(CampaignPersistence::restore(bad,world,rng,time),CampaignArchiveStatus::success);
    EXPECT_EQ(valid,save());
}
TEST_F(TimelineTest, MaximumTickDoesNotOverflowAndRecordBudgetIsEnforced) {
    ASSERT_EQ(time.schedule(world,place(1,UINT64_MAX)),TimelineStatus::completed);
    EXPECT_EQ(time.advance(world,rng,UINT64_MAX).processed,1U);
    EXPECT_EQ(time.now(),UINT64_MAX);
    for(std::size_t i=1;i<Timeline::max_records;++i)
        ASSERT_EQ(time.schedule(world,place(i+1,UINT64_MAX)),TimelineStatus::completed);
    EXPECT_EQ(time.schedule(world,place(Timeline::max_records+1,UINT64_MAX)),TimelineStatus::limit_exceeded);
}
TEST_F(TimelineTest, LegacyPersistenceStillRejectsTimelineArchiveAndKeepsWireFormat) {
    const auto legacy=Persistence::serialize(Persistence::capture(world,rng));
    EXPECT_EQ(Persistence::deserialize(legacy.bytes).status,PersistenceStatus::success);
    EXPECT_EQ(Persistence::deserialize(save()).status,PersistenceStatus::invalid_magic);
    EXPECT_EQ(CampaignPersistence::deserialize(legacy.bytes).status,CampaignArchiveStatus::invalid_magic);
}
TEST(TimelineWire, IndependentLittleEndianGoldenArchive) {
    // Independently constructed with Python struct.pack and bitwise CRC32C.
    constexpr std::string_view hex=
        "4352504754494d45010000003c00000000000000000000000000000001000000000000000100000000000000"
        "43525047534156450100000001000000010000000000000001000000000000002a00000000000000000000000000000000000000000000002c2db240"
        "010000000000000007000000000000000000000000000000010000000000000005000000000000000900000000000000"
        "01000000000000000600000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000b30f2cac";
    std::vector<std::uint8_t> golden;
    const auto digit=[](char c) { return c<='9' ? c-'0' : c-'a'+10; };
    for(std::size_t i=0;i<hex.size();i+=2) golden.push_back(static_cast<std::uint8_t>(digit(hex[i])*16+digit(hex[i+1])));
    WorldState world; RandomStreams rng{42}; Timeline time; const auto actor=world.create_entity();
    ASSERT_EQ(time.schedule(world,{1,7,0,actor,StreamRollAction{9,1,6}}),TimelineStatus::completed);
    const auto encoded=CampaignPersistence::serialize(CampaignPersistence::capture(world,rng,time));
    ASSERT_EQ(encoded.status,CampaignArchiveStatus::success);
    EXPECT_EQ(encoded.bytes,golden);
    const auto decoded=CampaignPersistence::deserialize(golden);
    ASSERT_EQ(decoded.status,CampaignArchiveStatus::success);
    EXPECT_EQ(CampaignPersistence::serialize(*decoded.snapshot).bytes,golden);
}
} // namespace
