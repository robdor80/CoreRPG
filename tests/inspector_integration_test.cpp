#include <corerpg/inspector.hpp>
#include <corerpg/simulation.hpp>
#include <corerpg/visibility.hpp>
#include "entity_structure_helpers.hpp"
#include <gtest/gtest.h>

#include <array>
#include <type_traits>

namespace {
using namespace corerpg;
constexpr EntityId actor{1}, unit{2}, old_container{3}, destination{4}, secret{5}, observer{6}, other{7}, group{8};
constexpr RelationKind secret_kind{17};
static_assert(!std::is_convertible_v<Inspector, ObserverView>);
static_assert(!std::is_constructible_v<Inspector, const ObserverView&>);
static_assert(!std::is_convertible_v<InspectionDiagnostic, ObserverView>);
static_assert(!std::is_convertible_v<InspectionPage<KnowledgeRecord>, ObserverView>);

void prepare(WorldState& world, const ContentCatalog& catalog) {
    for (int i = 0; i < 8; ++i) static_cast<void>(world.create_entity());
    static_cast<void>(world.bind_definition(secret, catalog, *DefinitionId::parse("example.structural:unit")));
    static_cast<void>(world.place_entity(unit, old_container));
    static_cast<void>(world.add_relation({secret, secret_kind, unit}));
    static_cast<void>(world.declare_population(group, 10));
    for (const auto id : {unit, old_container, destination, group})
        static_cast<void>(world.learn_fact(observer, {id, FactKind::existence, {}, {}}));
    static_cast<void>(world.learn_fact(observer, {unit, FactKind::containment, {}, {}}));
    static_cast<void>(world.learn_fact(observer, {group, FactKind::population, {}, {}}));
    static_cast<void>(world.learn_fact(other, {secret, FactKind::existence, {}, {}}));
    static_cast<void>(world.learn_fact(other, {secret, FactKind::definition, {}, {}}));
}
TEST(InspectorIntegrationTest, PrivilegedInspectionCannotRefreshMemoriesOrRevealSecretsToObserverView) {
    const auto catalog = corerpg_test::structural_catalog();
    WorldState world;
    prepare(world, *catalog.catalog());
    RandomStreams random{42};
    const auto remembered = Visibility::project(world, observer);
    const auto uninformed = Visibility::project(world, actor);
    const auto batch = world.materialize_population(group, 2);
    ASSERT_EQ(batch.status, MaterializationStatus::applied);
    const std::array requests{
        ActionRequest{actor, PlaceEntityAction{unit, destination}},
        ActionRequest{actor, AddRelationAction{{unit, secret_kind, secret}}},
        ActionRequest{actor, StreamRollAction{17, 1, 6}},
        ActionRequest{actor, PlaceEntityAction{secret, secret}}, // privileged rejection
    };
    std::array<ActionResult, requests.size()> results;
    const auto progress = Simulation::step(world, random, requests, results);
    ASSERT_EQ(progress.processed, requests.size());
    const auto results_before = results;
    const auto before = Persistence::capture(world, random);
    const auto bytes_before = Persistence::serialize(before);
    const Inspector inspector{world};
    ASSERT_TRUE(inspector.entity(secret));
    EXPECT_NE(inspector.entity(secret)->definition, nullptr);
    EXPECT_NE(inspector.definition(secret, *catalog.catalog()), nullptr);
    EXPECT_EQ(inspector.entity(unit)->containment.container, destination);
    EXPECT_EQ(inspector.entity(batch.first)->population_origin, group);
    ASSERT_EQ(inspector.relations_from(unit).records.size(), 1U);
    EXPECT_EQ(inspector.relations_from(unit).records[0].target, secret);
    ASSERT_EQ(inspector.knowledge(other).records.size(), 2U);
    EXPECT_EQ(inspector.knowledge(other).records[1].fact.definition_id(), "example.structural:unit");
    EXPECT_EQ(inspector.metadata().entities, 10U);
    EXPECT_EQ(results.back().rejection, ActionRejection::self_containment);
    EXPECT_TRUE(inspector.diagnose().valid()); // old containment and reserve are valid history
    EXPECT_EQ(inspector.diagnose(), inspector.diagnose());
    EXPECT_EQ(Persistence::capture(world, random), before);
    EXPECT_EQ(Persistence::serialize(Persistence::capture(world, random)).bytes, bytes_before.bytes);
    EXPECT_EQ(results, results_before);
    const auto projected = Visibility::project(world, observer);
    EXPECT_EQ(projected, remembered);
    EXPECT_EQ(Visibility::project(world, actor), uninformed);
    EXPECT_EQ(projected.find({unit, FactKind::containment, {}, {}})->reference, old_container);
    EXPECT_EQ(projected.find({group, FactKind::population, {}, {}})->latent, 10U);
    for (const auto hidden : {secret, batch.first, EntityId{99}, EntityId{}}) {
        EXPECT_FALSE(projected.knows(hidden));
        EXPECT_EQ(projected.find({hidden, FactKind::existence, {}, {}}), nullptr);
        EXPECT_EQ(projected.find({hidden, FactKind::definition, {}, {}}), nullptr);
    }
    EXPECT_EQ(projected.find({unit, FactKind::relation, secret_kind, secret}), nullptr);
    // Explicit authorized existence grants still reveal no definition/relation automatically.
    const std::array grants{FactKey{secret, FactKind::existence, {}, {}}};
    const auto granted = Visibility::project(world, observer, grants);
    EXPECT_TRUE(granted.knows(secret));
    EXPECT_EQ(granted.find({secret, FactKind::definition, {}, {}}), nullptr);
    EXPECT_EQ(granted.find({secret, FactKind::relation, secret_kind, unit}), nullptr);
}
TEST(InspectorIntegrationTest, InspectionBetweenActionsPreservesResultsOrderRngAndSnapshotsV1V2V3) {
    const auto catalog = corerpg_test::structural_catalog();
    WorldState seed;
    prepare(seed, *catalog.catalog());
    RandomStreams seed_random{123};
    const auto initial = Persistence::capture(seed, seed_random);
    WorldState baseline, inspected;
    RandomStreams baseline_random{0}, inspected_random{0};
    ASSERT_EQ(Persistence::restore(initial, baseline, baseline_random, {}, catalog.catalog()), PersistenceStatus::success);
    ASSERT_EQ(Persistence::restore(initial, inspected, inspected_random, {}, catalog.catalog()), PersistenceStatus::success);
    const std::array requests{
        ActionRequest{actor, PlaceEntityAction{unit, destination}},
        ActionRequest{actor, StreamRollAction{2, 1, 6}},
        ActionRequest{actor, PlaceEntityAction{unit, destination}},
        ActionRequest{actor, AddRelationAction{{secret, secret_kind, unit}}},
        ActionRequest{actor, StreamRollAction{2, 0, 0}},
        ActionRequest{actor, RemoveContainmentAction{unit}},
    };
    std::array<ActionResult, requests.size()> expected, actual;
    ASSERT_EQ(Simulation::step(baseline, baseline_random, requests, expected).status, SimulationStatus::completed);
    const Inspector inspector{inspected};
    for (std::size_t i = 0; i < requests.size(); ++i) {
        const auto before = Persistence::capture(inspected, inspected_random);
        EXPECT_TRUE(inspector.diagnose().valid());
        static_cast<void>(inspector.metadata());
        static_cast<void>(inspector.entity(unit));
        static_cast<void>(inspector.entities());
        static_cast<void>(inspector.containment());
        static_cast<void>(inspector.relations());
        static_cast<void>(inspector.relations_from(secret));
        static_cast<void>(inspector.populations());
        static_cast<void>(inspector.memberships());
        static_cast<void>(inspector.knowledge(observer));
        EXPECT_EQ(Persistence::capture(inspected, inspected_random), before);
        actual[i] = ActionEngine::process(inspected, inspected_random, requests[i]);
    }
    EXPECT_EQ(actual, expected);
    EXPECT_EQ(Persistence::capture(inspected, inspected_random), Persistence::capture(baseline, baseline_random));
    EXPECT_EQ(inspected_random.stream(2).next(), baseline_random.stream(2).next());
    // Explicit v1/v2/v3 snapshots retain their own format/bytes around inspection and load.
    for (const auto version : {1U, 2U, 3U}) {
        CampaignSnapshot snapshot;
        snapshot.format_version = version;
        snapshot.last_entity_id = 3;
        snapshot.revision = 3;
        const auto bytes = Persistence::serialize(snapshot);
        ASSERT_EQ(bytes.status, PersistenceStatus::success);
        WorldState loaded;
        RandomStreams random{0};
        ASSERT_EQ(Persistence::load(bytes.bytes, loaded, random), PersistenceStatus::success);
        EXPECT_TRUE(Inspector{loaded}.diagnose().valid());
        EXPECT_EQ(Inspector{loaded}.entities().count, 3U);
        EXPECT_TRUE(Inspector::diagnose_snapshot(snapshot).valid());
        EXPECT_EQ(Persistence::serialize(snapshot).bytes, bytes.bytes);
        EXPECT_EQ(Persistence::deserialize(bytes.bytes).snapshot, snapshot);
    }
}
TEST(InspectorIntegrationTest, OnlyDefinitiveSimulationPrefixIsInspectedAndWorldReplacementRestartsPaging) {
    WorldState world;
    const auto a = world.create_entity(), b = world.create_entity();
    RandomStreams random{7};
    const Inspector inspector{world};
    const auto saved = Persistence::capture(world, random);
    const std::array requests{ActionRequest{a, PlaceEntityAction{a, b}}};
    std::array<ActionResult, 1> results;
    results[0].revision_before = 999;
    auto progress = Simulation::step(world, random, requests, std::span<ActionResult>{});
    EXPECT_EQ(progress.status, SimulationStatus::insufficient_output);
    EXPECT_EQ(progress.processed, 0U);
    EXPECT_EQ(results[0].revision_before, 999U);
    EXPECT_EQ(inspector.entity(a)->containment.status, ContainmentStatus::uncontained);
    progress = Simulation::step(world, random, requests, results);
    ASSERT_EQ(progress.processed, 1U);
    EXPECT_EQ(inspector.metadata().revision, results[0].revision_after);
    EXPECT_EQ(inspector.entity(a)->containment.container, b);
    // No borrowed page survives restore. Inspector itself keeps referring to this object.
    ASSERT_EQ(Persistence::restore(saved, world, random), PersistenceStatus::success);
    EXPECT_EQ(inspector.entity(a)->containment.status, ContainmentStatus::uncontained);
    EXPECT_EQ(inspector.entities({0, 1, results[0].revision_after}).progress.status,
              InspectionStatus::stale_revision);
}
} // namespace
