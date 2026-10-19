#include <corerpg/inspector.hpp>
#include <corerpg/persistence.hpp>
#include <gtest/gtest.h>
#include "entity_structure_helpers.hpp"

#include <limits>
#include <type_traits>

namespace {
using namespace corerpg;
static_assert(!std::is_constructible_v<Inspector, WorldState&&>);
static_assert(!std::is_constructible_v<Inspector, const WorldState&&>);
static_assert(std::is_same_v<decltype(InspectionPage<EntityRelation>{}.records),
                             std::span<const EntityRelation>>);

TEST(InspectorTest, EmptyAndMissingEntities) {
    WorldState world;
    const Inspector inspector{world};
    EXPECT_EQ(inspector.metadata(), InspectionMetadata{});
    EXPECT_EQ(inspector.entities().count, 0U);
    EXPECT_FALSE(inspector.entities().first.is_valid());
    EXPECT_TRUE(inspector.containment().records.empty());
    EXPECT_TRUE(inspector.relations().records.empty());
    EXPECT_TRUE(inspector.populations().records.empty());
    EXPECT_TRUE(inspector.memberships().records.empty());
    EXPECT_FALSE(inspector.entity({}));
    EXPECT_FALSE(inspector.entity(EntityId{1}));
    EXPECT_EQ(inspector.knowledge(EntityId{1}).progress.status, InspectionStatus::entity_not_found);
    EXPECT_EQ(inspector.relations_from(EntityId{1}).progress.status, InspectionStatus::entity_not_found);
}
TEST(InspectorTest, EntitiesDefinitionsNestedContainmentAndDirectedTypedRelations) {
    WorldState world;
    const auto a = world.create_entity(), b = world.create_entity(), c = world.create_entity();
    const auto catalog = corerpg_test::structural_catalog();
    const auto id = *DefinitionId::parse("example.structural:unit");
    ASSERT_EQ(world.bind_definition(a, *catalog.catalog(), id), DefinitionBindingChange::applied);
    ASSERT_EQ(world.place_entity(a, b), ContainmentChange::applied);
    ASSERT_EQ(world.place_entity(b, c), ContainmentChange::applied);
    ASSERT_EQ(world.place_entity(c, a), ContainmentChange::cycle_detected);
    ASSERT_EQ(world.add_relation({a, RelationKind{2}, b}), RelationChange::applied);
    ASSERT_EQ(world.add_relation({a, RelationKind{1}, c}), RelationChange::applied);
    ASSERT_EQ(world.add_relation({b, RelationKind{2}, a}), RelationChange::applied);
    const Inspector inspector{world};
    ASSERT_TRUE(inspector.entity(a));
    EXPECT_EQ(inspector.entity(a)->containment.container, b);
    EXPECT_EQ(inspector.entity(b)->containment.container, c);
    EXPECT_EQ(inspector.entity(c)->containment.status, ContainmentStatus::uncontained);
    EXPECT_EQ(*inspector.entity(a)->definition, id);
    EXPECT_EQ(inspector.definition(a, *catalog.catalog()), catalog.catalog()->find(id));
    EXPECT_EQ(inspector.definition(b, *catalog.catalog()), nullptr);
    EXPECT_EQ(inspector.definition(EntityId{99}, *catalog.catalog()), nullptr);
    ASSERT_EQ(inspector.containment().records.size(), 2U);
    EXPECT_EQ(inspector.containment().records[0], (Inspector::ContainmentLink{a, b}));
    ASSERT_EQ(inspector.relations_from(a).records.size(), 2U);
    EXPECT_EQ(inspector.relations_from(a).records[0].kind, RelationKind{1});
    ASSERT_EQ(inspector.relations_from(a, {}, RelationKind{2}).records.size(), 1U);
    EXPECT_EQ(inspector.relations_from(a, {}, RelationKind{2}).records[0].target, b);
    EXPECT_TRUE(inspector.relations_from(c, {}, RelationKind{2}).records.empty());
    EXPECT_TRUE(inspector.relations_from(a, {}, RelationKind{}).records.empty());
    ASSERT_EQ(inspector.relations_from(b).records.size(), 1U);
    EXPECT_EQ(inspector.relations_from(b).records[0].target, a);
}
TEST(InspectorTest, PopulationReserveMaterializationAndImmutableOrigin) {
    WorldState world;
    const auto group = world.create_entity();
    ASSERT_EQ(world.declare_population(group, 10), PopulationChange::applied);
    const auto batch = world.materialize_population(group, 3);
    ASSERT_EQ(batch.status, MaterializationStatus::applied);
    const Inspector inspector{world};
    ASSERT_EQ(inspector.populations().records.size(), 1U);
    EXPECT_EQ(inspector.populations().records[0], (PopulationRecord{group, 10, 7}));
    EXPECT_EQ(inspector.entity(batch.first)->population_origin, group);
    EXPECT_FALSE(inspector.entity(group)->population_origin);
    EXPECT_EQ(inspector.memberships({1, 1}).records[0].entity, EntityId{batch.first.value() + 1});
    EXPECT_EQ(inspector.metadata().memberships, 3U);
}
TEST(InspectorTest, PageBoundariesRevisionChangesAndRepeatability) {
    WorldState world;
    for (int i = 0; i < 300; ++i) ASSERT_TRUE(world.create_entity().is_valid());
    const Inspector inspector{world};
    const auto revision = world.revision();
    const auto first = inspector.entities({0, 256, revision});
    EXPECT_EQ(first, inspector.entities({0, 256, revision}));
    EXPECT_EQ(first.first, EntityId{1});
    EXPECT_EQ(first.count, 256U);
    EXPECT_EQ(first.progress.next_offset, 256U);
    const auto second = inspector.entities({*first.progress.next_offset, 256, revision});
    EXPECT_EQ(second.first, EntityId{257});
    EXPECT_EQ(second.count, 44U);
    EXPECT_FALSE(second.progress.next_offset);
    EXPECT_EQ(inspector.entities({300, 1}).count, 0U);
    EXPECT_EQ(inspector.entities({std::numeric_limits<std::uint64_t>::max(), 1}).count, 0U);
    EXPECT_EQ(inspector.entities({0, 0}).progress.status, InspectionStatus::invalid_limit);
    EXPECT_EQ(inspector.entities({0, 257}).progress.status, InspectionStatus::invalid_limit);
    ASSERT_TRUE(world.create_entity().is_valid());
    EXPECT_EQ(inspector.entities({256, 256, revision}).progress.status, InspectionStatus::stale_revision);
    EXPECT_EQ(inspector.relations({0, 1, revision}).progress.status, InspectionStatus::stale_revision);
    EXPECT_EQ(inspector.containment({0, 1, revision}).progress.status, InspectionStatus::stale_revision);
    EXPECT_EQ(inspector.populations({0, 1, revision}).progress.status, InspectionStatus::stale_revision);
    EXPECT_EQ(inspector.memberships({0, 1, revision}).progress.status, InspectionStatus::stale_revision);
    EXPECT_EQ(inspector.knowledge(EntityId{1}, {0, 1, revision}).progress.status, InspectionStatus::stale_revision);
    EXPECT_EQ(inspector.relations_from(EntityId{1}, {0, 1, revision}).progress.status, InspectionStatus::stale_revision);
    EXPECT_EQ(inspector.entities({256, 256}).count, 45U);
}
} // namespace
