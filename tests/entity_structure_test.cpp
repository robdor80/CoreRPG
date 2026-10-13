#include <corerpg/entity_structure.hpp>
#include <corerpg/persistence.hpp>

#include "entity_structure_helpers.hpp"

#include <gtest/gtest.h>
#include <array>
#include <vector>

TEST(EntityStructureContractTest, RelationKindsAreStrongStableFullWidthLabels) {
    EXPECT_FALSE(corerpg::RelationKind{}.is_valid());
    EXPECT_TRUE(corerpg::RelationKind{UINT64_MAX}.is_valid());
    EXPECT_EQ(corerpg::RelationKind{UINT64_MAX}.value(), UINT64_MAX);
    EXPECT_LT(corerpg::RelationKind{1}, corerpg::RelationKind{2});
}

TEST(EntityStructureContractTest, RelationOrderingAndPopulationHaveNoHumanFields) {
    const corerpg::EntityRelation a{corerpg::EntityId{1}, corerpg::RelationKind{2},
                                    corerpg::EntityId{3}};
    const corerpg::EntityRelation b{corerpg::EntityId{1}, corerpg::RelationKind{3},
                                    corerpg::EntityId{2}};
    EXPECT_LT(a, b);
    const corerpg::PopulationRecord reserve{corerpg::EntityId{99}, UINT64_MAX, UINT64_MAX};
    EXPECT_EQ(reserve.total, reserve.latent);
    EXPECT_EQ(reserve.group.value(), 99U);
}

TEST(EntityAssociationTest, DefinitionsAreValidatedImmutableAndDoNotChangeIdentity) {
    auto build = corerpg_test::structural_catalog();
    ASSERT_TRUE(build.succeeded());
    const auto& catalog = *build.catalog();
    const auto first = *corerpg::DefinitionId::parse("example.structural:unit");
    const auto second = *corerpg::DefinitionId::parse("example.structural:collective");
    const auto missing = *corerpg::DefinitionId::parse("example.structural:missing");
    corerpg::WorldState world;
    const auto entity = world.create_entity();
    EXPECT_EQ(world.definition_of(entity), nullptr);
    EXPECT_EQ(world.bind_definition({}, catalog, first),
              corerpg::DefinitionBindingChange::entity_not_found);
    EXPECT_EQ(world.bind_definition(entity, catalog, missing),
              corerpg::DefinitionBindingChange::definition_not_found);
    EXPECT_EQ(world.revision(), 1U);
    EXPECT_EQ(world.bind_definition(entity, catalog, first),
              corerpg::DefinitionBindingChange::applied);
    ASSERT_NE(world.definition_of(entity), nullptr);
    EXPECT_EQ(*world.definition_of(entity), first);
    EXPECT_EQ(world.bind_definition(entity, catalog, first),
              corerpg::DefinitionBindingChange::no_change);
    EXPECT_EQ(world.bind_definition(entity, catalog, second),
              corerpg::DefinitionBindingChange::definition_conflict);
    EXPECT_EQ(world.revision(), 2U);
    EXPECT_TRUE(world.contains(entity));
    EXPECT_EQ(world.create_entity(), corerpg::EntityId{2});
}

TEST(EntityAssociationTest, RelationsHaveCanonicalOrderIndependentOfInsertion) {
    corerpg::WorldState first;
    corerpg::WorldState second;
    for (int i = 0; i < 4; ++i) {
        EXPECT_EQ(first.create_entity(), second.create_entity());
    }
    using corerpg::EntityId;
    using corerpg::RelationKind;
    const std::array edges{
        corerpg::EntityRelation{EntityId{3}, RelationKind{1}, EntityId{4}},
        corerpg::EntityRelation{EntityId{1}, RelationKind{UINT64_MAX}, EntityId{3}},
        corerpg::EntityRelation{EntityId{1}, RelationKind{1}, EntityId{4}},
        corerpg::EntityRelation{EntityId{4}, RelationKind{1}, EntityId{3}},
        corerpg::EntityRelation{EntityId{1}, RelationKind{1}, EntityId{1}}};
    for (const auto& edge : edges) {
        EXPECT_EQ(first.add_relation(edge), corerpg::RelationChange::applied);
    }
    for (auto it = edges.rbegin(); it != edges.rend(); ++it) {
        EXPECT_EQ(second.add_relation(*it), corerpg::RelationChange::applied);
    }
    EXPECT_EQ(std::vector(first.relations().begin(), first.relations().end()),
              std::vector(second.relations().begin(), second.relations().end()));
    EXPECT_EQ(first.revision(), 9U);
    EXPECT_EQ(first.relations_from(EntityId{1}).size(), 3U);
    EXPECT_TRUE(first.relations_from(EntityId{99}).empty());
    // Social/organizational cycles and self edges are independent of containment.
    EXPECT_EQ(first.container_of(EntityId{3}).status, corerpg::ContainmentStatus::uncontained);
    EXPECT_EQ(first.place_entity(EntityId{3}, EntityId{3}),
              corerpg::ContainmentChange::self_containment);
}

TEST(EntityAssociationTest, InvalidEdgesDuplicatesAndRemovalsPreserveOtherState) {
    using corerpg::EntityId;
    using corerpg::RelationKind;
    corerpg::WorldState world;
    static_cast<void>(world.create_entity());
    static_cast<void>(world.create_entity());
    const corerpg::EntityRelation edge{EntityId{1}, RelationKind{7}, EntityId{2}};
    EXPECT_EQ(world.add_relation({{}, RelationKind{7}, EntityId{2}}),
              corerpg::RelationChange::source_not_found);
    EXPECT_EQ(world.add_relation({EntityId{1}, RelationKind{7}, EntityId{3}}),
              corerpg::RelationChange::target_not_found);
    EXPECT_EQ(world.add_relation({EntityId{1}, {}, EntityId{2}}),
              corerpg::RelationChange::invalid_kind);
    EXPECT_EQ(world.revision(), 2U);
    EXPECT_EQ(world.add_relation(edge), corerpg::RelationChange::applied);
    EXPECT_EQ(world.add_relation(edge), corerpg::RelationChange::no_change);
    EXPECT_TRUE(world.has_relation(edge));
    EXPECT_EQ(world.revision(), 3U);
    EXPECT_EQ(world.remove_relation(edge), corerpg::RelationChange::applied);
    EXPECT_EQ(world.remove_relation(edge), corerpg::RelationChange::no_change);
    EXPECT_FALSE(world.has_relation(edge));
    EXPECT_EQ(world.revision(), 4U);
    EXPECT_EQ(world.entity_count(), 2U);
    EXPECT_TRUE(world.contains(edge.source));
    EXPECT_TRUE(world.contains(edge.target));
}

TEST(EntityAssociationTest, CopiesAndAssignmentsHaveIndependentAssociations) {
    auto build = corerpg_test::structural_catalog();
    const auto definition = *corerpg::DefinitionId::parse("example.structural:unit");
    corerpg::WorldState world;
    const auto entity = world.create_entity();
    const auto other = world.create_entity();
    ASSERT_EQ(world.bind_definition(entity, *build.catalog(), definition),
              corerpg::DefinitionBindingChange::applied);
    const corerpg::EntityRelation edge{entity, corerpg::RelationKind{1}, other};
    ASSERT_EQ(world.add_relation(edge), corerpg::RelationChange::applied);
    auto copy = world;
    corerpg::WorldState assigned;
    assigned = world;
    EXPECT_EQ(copy.remove_relation(edge), corerpg::RelationChange::applied);
    EXPECT_EQ(assigned.bind_definition(other, *build.catalog(), definition),
              corerpg::DefinitionBindingChange::applied);
    EXPECT_TRUE(world.has_relation(edge));
    EXPECT_EQ(world.definition_of(other), nullptr);
    EXPECT_EQ(*copy.definition_of(entity), definition);
    EXPECT_EQ(world.revision(), 4U);
}

TEST(EntityAssociationTest, SnapshotNeverSilentlyDiscardsNewAuthoritativeFields) {
    corerpg::WorldState world;
    const auto entity = world.create_entity();
    ASSERT_EQ(world.add_relation({entity, corerpg::RelationKind{1}, entity}),
              corerpg::RelationChange::applied);
    corerpg::RandomStreams random{123};
    auto snapshot = corerpg::Persistence::capture(world, random);
    EXPECT_EQ(snapshot.format_version, 2U);
    EXPECT_EQ(snapshot.relations.size(), 1U);
    snapshot.format_version = 1;
    EXPECT_EQ(corerpg::Persistence::serialize(snapshot).status,
              corerpg::PersistenceStatus::unexpected_fields);
    const auto empty_v1 = corerpg::CampaignSnapshot{};
    ASSERT_EQ(corerpg::Persistence::restore(empty_v1, world, random),
              corerpg::PersistenceStatus::success);
    EXPECT_TRUE(world.relations().empty());
}

TEST(EntityAssociationTest, NonAllocatingDefinitionValidationMatchesExistingGrammar) {
    auto build = corerpg_test::structural_catalog();
    for (const auto value :
         {"example.structural:unit", "example.structural:a/b", "bad", "example.structural:../x",
          "EXAMPLE.structural:unit", "example.structural:a//b"}) {
        EXPECT_EQ(corerpg::DefinitionId::is_valid_text(value),
                  corerpg::DefinitionId::parse(value).has_value());
    }
    EXPECT_NE(build.catalog()->find_by_value("example.structural:unit"), nullptr);
    EXPECT_EQ(build.catalog()->find_by_value("example.structural:missing"), nullptr);
    EXPECT_EQ(build.catalog()->find_by_value("bad"), nullptr);
}
