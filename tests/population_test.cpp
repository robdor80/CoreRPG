#include <corerpg/persistence.hpp>

#include <gtest/gtest.h>

#include <array>
#include <set>

namespace {

using corerpg::EntityId;
using corerpg::MaterializationStatus;
using corerpg::PopulationChange;

TEST(PopulationTest, DeclarationIsExplicitImmutableAndHasNoImplicitIndividuals) {
    corerpg::WorldState world;
    const auto group = world.create_entity();
    EXPECT_FALSE(world.population_of(group));
    EXPECT_FALSE(world.population_origin(group));
    EXPECT_EQ(world.declare_population({}, 30), PopulationChange::group_not_found);
    EXPECT_EQ(world.declare_population(group, 30), PopulationChange::applied);
    EXPECT_EQ(world.declare_population(group, 30), PopulationChange::no_change);
    EXPECT_EQ(world.declare_population(group, 31), PopulationChange::population_conflict);
    ASSERT_TRUE(world.population_of(group));
    EXPECT_EQ(world.population_of(group)->total, 30U);
    EXPECT_EQ(world.population_of(group)->latent, 30U);
    EXPECT_EQ(world.entity_count(), 1U);
    EXPECT_EQ(world.revision(), 2U);
    EXPECT_TRUE(world.population_memberships().empty());
}

TEST(PopulationTest, MaterializationConservesReserveAndEmitsStableContiguousIds) {
    corerpg::WorldState world;
    const auto group = world.create_entity();
    ASSERT_EQ(world.declare_population(group, 10), PopulationChange::applied);
    const auto result = world.materialize_population(group, 4);
    ASSERT_EQ(result.status, MaterializationStatus::applied);
    EXPECT_EQ(result.first, EntityId{2});
    EXPECT_EQ(result.count, 4U);
    EXPECT_EQ(world.revision(), 6U);
    EXPECT_EQ(world.entity_count(), 5U);
    EXPECT_EQ(world.population_of(group)->latent, 6U);
    EXPECT_EQ(world.population_of(group)->total, 10U);
    for (std::uint64_t id = 2; id < 6; ++id) {
        EXPECT_TRUE(world.contains(EntityId{id}));
        EXPECT_EQ(world.population_origin(EntityId{id}), group);
        EXPECT_EQ(world.definition_of(EntityId{id}), nullptr);
        EXPECT_EQ(world.container_of(EntityId{id}).status, corerpg::ContainmentStatus::uncontained);
    }
    // Repeated declaration cannot refill consumed slots or regenerate identities.
    EXPECT_EQ(world.declare_population(group, 10), PopulationChange::no_change);
    EXPECT_EQ(world.population_of(group)->latent, 6U);
    EXPECT_EQ(world.materialize_population(group, 6).first, EntityId{6});
    EXPECT_EQ(world.population_of(group)->latent, 0U);
    EXPECT_EQ(world.materialize_population(group, 1).status,
              MaterializationStatus::insufficient_latent);
    EXPECT_EQ(world.create_entity(), EntityId{12});
}

TEST(PopulationTest, InvalidBatchesAreRejectedAsWholeWithoutConsumingRng) {
    corerpg::WorldState world;
    const auto group = world.create_entity();
    const auto other = world.create_entity();
    ASSERT_EQ(world.declare_population(group, 300), PopulationChange::applied);
    corerpg::RandomStreams rng{987};
    static_cast<void>(rng.stream(7).next());
    const auto before = corerpg::Persistence::capture(world, rng);
    EXPECT_EQ(world.materialize_population({}, 1).status, MaterializationStatus::group_not_found);
    EXPECT_EQ(world.materialize_population(other, 1).status,
              MaterializationStatus::population_not_found);
    EXPECT_EQ(world.materialize_population(group, 257).status,
              MaterializationStatus::batch_limit_exceeded);
    EXPECT_EQ(world.materialize_population(group, 5, 4).status,
              MaterializationStatus::batch_limit_exceeded);
    EXPECT_EQ(world.materialize_population(group, 301, 400).status,
              MaterializationStatus::insufficient_latent);
    EXPECT_EQ(world.materialize_population(group, UINT64_MAX, UINT64_MAX).status,
              MaterializationStatus::batch_limit_exceeded);
    EXPECT_EQ(world.materialize_population(group, 0, 0).status, MaterializationStatus::no_change);
    EXPECT_EQ(corerpg::Persistence::capture(world, rng), before);
}

TEST(PopulationTest, MaximumReserveIsConstantSizeUntilExplicitIndividualization) {
    corerpg::WorldState world;
    const auto group = world.create_entity();
    ASSERT_EQ(world.declare_population(group, UINT64_MAX), PopulationChange::applied);
    EXPECT_EQ(world.entity_count(), 1U);
    EXPECT_EQ(world.populations().size(), 1U);
    EXPECT_TRUE(world.population_memberships().empty());
    EXPECT_EQ(world.materialize_population(group, 17).status, MaterializationStatus::applied);
    EXPECT_EQ(world.entity_count(), 18U);
    EXPECT_EQ(world.population_of(group)->latent, UINT64_MAX - 17U);
    EXPECT_EQ(world.population_memberships().size(), 17U);
}

TEST(PopulationTest, HostRepresentationsNeverControlExistenceOrIdentity) {
    corerpg::WorldState world;
    const auto group = world.create_entity();
    ASSERT_EQ(world.declare_population(group, 1'000), PopulationChange::applied);
    ASSERT_EQ(world.materialize_population(group, 100).status, MaterializationStatus::applied);
    corerpg::RandomStreams random{42};
    const auto before = corerpg::Persistence::capture(world, random);
    std::set<EntityId> visible{EntityId{2}, EntityId{15}, EntityId{101}};
    visible.clear(); // Only the Host unloads representations.
    EXPECT_EQ(world.entity_count(), 101U);
    EXPECT_TRUE(world.contains(EntityId{15}));
    EXPECT_EQ(world.population_origin(EntityId{15}), group);
    EXPECT_EQ(corerpg::Persistence::capture(world, random), before);
    visible.insert(EntityId{15});
    EXPECT_EQ(corerpg::Persistence::capture(world, random), before);
}

TEST(PopulationTest, CopiesAndMultipleNonHumanCompatibleGroupsRemainIndependent) {
    corerpg::WorldState first;
    corerpg::WorldState second;
    for (int i = 0; i < 3; ++i) {
        EXPECT_EQ(first.create_entity(), second.create_entity());
    }
    for (const auto group : std::array<EntityId, 3>{EntityId{1}, EntityId{2}, EntityId{3}}) {
        EXPECT_EQ(first.declare_population(group, 10), second.declare_population(group, 10));
        const auto a = first.materialize_population(group, 3);
        const auto b = second.materialize_population(group, 3);
        EXPECT_EQ(a.first, b.first);
        EXPECT_EQ(a.count, b.count);
    }
    // The same operations describe a roster, settlement reserve or non-human colony.
    corerpg::RandomStreams a{5};
    corerpg::RandomStreams b{5};
    EXPECT_EQ(corerpg::Persistence::capture(first, a), corerpg::Persistence::capture(second, b));
    auto copied = first;
    corerpg::WorldState assigned;
    assigned = first;
    EXPECT_EQ(copied.materialize_population(EntityId{1}, 2).status, MaterializationStatus::applied);
    EXPECT_EQ(assigned.materialize_population(EntityId{2}, 1).status,
              MaterializationStatus::applied);
    EXPECT_EQ(first.population_of(EntityId{1})->latent, 7U);
    EXPECT_EQ(first.population_of(EntityId{2})->latent, 7U);
    EXPECT_EQ(copied.population_of(EntityId{1})->latent, 5U);
    EXPECT_EQ(assigned.population_of(EntityId{2})->latent, 6U);
}

TEST(PopulationTest, NestedCollectivesHaveOriginsWithoutHumanOrSpatialAssumptions) {
    corerpg::WorldState world;
    const auto root = world.create_entity();
    ASSERT_EQ(world.declare_population(root, 1), PopulationChange::applied);
    const auto group = world.materialize_population(root, 1).first;
    ASSERT_EQ(world.declare_population(group, 3), PopulationChange::applied);
    const auto unit = world.materialize_population(group, 1).first;
    EXPECT_EQ(world.population_origin(group), root);
    EXPECT_EQ(world.population_origin(unit), group);
    EXPECT_EQ(world.container_of(unit).status, corerpg::ContainmentStatus::uncontained);
    EXPECT_EQ(world.add_relation({unit, corerpg::RelationKind{UINT64_MAX}, root}),
              corerpg::RelationChange::applied);
    EXPECT_EQ(world.population_of(root)->total, 1U);
    EXPECT_EQ(world.population_of(group)->total, 3U);
}

TEST(PopulationTest, NeutralRosterSettlementAndNonHumanColonyUseTheSameContracts) {
    struct Scenario {
        const char* name;
        std::uint64_t total;
        corerpg::RelationKind association;
    };
    for (const auto scenario : {Scenario{"logical roster", 512, corerpg::RelationKind{1}},
                                Scenario{"settlement reserve", 2'000, corerpg::RelationKind{2}},
                                Scenario{"non-human colony without homes or kinship", 1'000'000'000,
                                         corerpg::RelationKind{UINT64_MAX}}}) {
        SCOPED_TRACE(scenario.name);
        corerpg::WorldState world;
        const auto group = world.create_entity();
        const auto association_target = world.create_entity();
        ASSERT_EQ(world.declare_population(group, scenario.total), PopulationChange::applied);
        const auto issued = world.materialize_population(group, 32, 32);
        ASSERT_EQ(issued.status, MaterializationStatus::applied);
        EXPECT_EQ(issued.first, EntityId{3});
        for (std::uint64_t i = 0; i < issued.count; ++i) {
            const EntityId unit{issued.first.value() + i};
            EXPECT_EQ(world.add_relation({unit, scenario.association, association_target}),
                      corerpg::RelationChange::applied);
            EXPECT_EQ(world.population_origin(unit), group);
            EXPECT_EQ(world.container_of(unit).status, corerpg::ContainmentStatus::uncontained);
        }
        EXPECT_EQ(world.population_of(group)->latent, scenario.total - 32);
        EXPECT_EQ(world.entity_count(), 34U);
        EXPECT_EQ(world.population_memberships().size(), 32U);
        std::set<EntityId> visual{EntityId{3}, EntityId{4}};
        visual.clear();
        corerpg::RandomStreams random{42};
        corerpg::WorldState loaded;
        corerpg::RandomStreams resumed{0};
        const auto snapshot = corerpg::Persistence::capture(world, random);
        ASSERT_EQ(corerpg::Persistence::load(corerpg::Persistence::serialize(snapshot).bytes,
                                             loaded, resumed),
                  corerpg::PersistenceStatus::success);
        EXPECT_EQ(corerpg::Persistence::capture(loaded, resumed), snapshot);
    }
}

} // namespace
