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

namespace {
template <class Query, class T>
void verify_pages(Query query, std::span<const T> expected, std::uint64_t revision) {
    for (const std::uint32_t limit : {1U, 64U, 256U}) {
        std::uint64_t offset = 0;
        do {
            const auto result = query(InspectionRequest{offset, limit, revision});
            const auto repeated = query(InspectionRequest{offset, limit, revision});
            ASSERT_EQ(result.progress.status, InspectionStatus::success);
            EXPECT_EQ(result.progress.revision, revision);
            EXPECT_EQ(result.progress, repeated.progress);
            EXPECT_EQ(result.records.data(), expected.data() + offset);
            EXPECT_EQ(result.records.data(), repeated.records.data());
            EXPECT_EQ(result.records.size(), std::min<std::size_t>(limit, expected.size() - offset));
            for (std::size_t i = 0; i < result.records.size(); ++i)
                EXPECT_EQ(result.records[i], expected[static_cast<std::size_t>(offset) + i]);
            offset += result.records.size();
            EXPECT_EQ(result.progress.next_offset.has_value(), offset < expected.size());
            if (!result.progress.next_offset) break;
            EXPECT_EQ(*result.progress.next_offset, offset);
        } while (offset < expected.size());
        EXPECT_EQ(offset, expected.size());
    }
    for (const auto offset : {static_cast<std::uint64_t>(expected.size()), UINT64_MAX}) {
        const auto result = query(InspectionRequest{offset, 256});
        EXPECT_EQ(result.progress.status, InspectionStatus::success);
        EXPECT_TRUE(result.records.empty());
        EXPECT_FALSE(result.progress.next_offset);
    }
    for (const auto limit : {0U, 257U}) {
        const auto result = query(InspectionRequest{0, limit});
        EXPECT_EQ(result.progress.status, InspectionStatus::invalid_limit);
        EXPECT_TRUE(result.records.empty());
    }
    const auto stale = query(InspectionRequest{0, 64, revision - 1});
    EXPECT_EQ(stale.progress.status, InspectionStatus::stale_revision);
    EXPECT_TRUE(stale.records.empty());
}
TEST(InspectorTest, EverySparseTablePagesWithoutCopiesAndHasUniformBoundaryRules) {
    WorldState world;
    for (int i = 0; i < 301; ++i) ASSERT_TRUE(world.create_entity().is_valid());
    const EntityId source{1};
    for (std::uint64_t i = 2; i <= 301; ++i) {
        const EntityId id{i};
        ASSERT_EQ(world.place_entity(id, source), ContainmentChange::applied);
        ASSERT_EQ(world.add_relation({source, RelationKind{7}, id}), RelationChange::applied);
        ASSERT_EQ(world.declare_population(id, 1), PopulationChange::applied);
        ASSERT_EQ(world.materialize_population(id, 1).status, MaterializationStatus::applied);
        ASSERT_EQ(world.learn_fact(source, {id, FactKind::existence, {}, {}}), KnowledgeChange::applied);
    }
    const Inspector inspector{world};
    const auto revision = world.revision();
    const auto links = inspector.containment({0, 256}).records;
    // Containment has 300 entries; collect independent expected canonical pairs.
    std::vector<Inspector::ContainmentLink> expected_links;
    for (std::uint64_t i = 2; i <= 301; ++i) expected_links.emplace_back(EntityId{i}, source);
    // Check content separately; pointer identity is checked against the first borrowed page.
    for (std::size_t i = 0; i < links.size(); ++i) EXPECT_EQ(links[i], expected_links[i]);
    // For containment use the known full table only inside this technical test via pages.
    // No production API returns an unbounded span.
    const auto check_links = [&](InspectionRequest request) {
        return inspector.containment(request);
    };
    for (std::uint64_t offset = 0; offset < 300; offset += 64) {
        const auto result = check_links({offset, 64, revision});
        for (std::size_t i = 0; i < result.records.size(); ++i)
            EXPECT_EQ(result.records[i], expected_links[static_cast<std::size_t>(offset) + i]);
        if (offset < 256) {
            EXPECT_EQ(result.records.data(), links.data() + offset);
        }
        EXPECT_EQ(result.progress.next_offset.has_value(), offset + result.records.size() < 300);
    }
    verify_pages([&](auto r) { return inspector.relations(r); }, world.relations(), revision);
    verify_pages([&](auto r) { return inspector.relations_from(source, r); }, world.relations(), revision);
    verify_pages([&](auto r) { return inspector.relations_from(source, r, RelationKind{7}); }, world.relations(), revision);
    verify_pages([&](auto r) { return inspector.populations(r); }, world.populations(), revision);
    verify_pages([&](auto r) { return inspector.memberships(r); }, world.population_memberships(), revision);
    verify_pages([&](auto r) { return inspector.knowledge(source, r); }, world.knowledge_of(source), revision);
    EXPECT_EQ(inspector.containment({300, 1}).records.size(), 0U);
    EXPECT_TRUE(inspector.containment({UINT64_MAX, 1}).records.empty());
    EXPECT_EQ(inspector.containment({0, 0}).progress.status, InspectionStatus::invalid_limit);
    EXPECT_EQ(inspector.containment({0, 257}).progress.status, InspectionStatus::invalid_limit);
    EXPECT_TRUE(inspector.diagnose().valid());
}
TEST(InspectorTest, PageFailurePrecedenceAndWorldReplacementAreExplicit) {
    WorldState world;
    const Inspector inspector{world};
    EXPECT_EQ(inspector.knowledge(EntityId{99}, {0, 0, 1}).progress.status, InspectionStatus::stale_revision);
    EXPECT_EQ(inspector.knowledge(EntityId{99}, {0, 0}).progress.status, InspectionStatus::invalid_limit);
    EXPECT_EQ(inspector.knowledge(EntityId{99}).progress.status, InspectionStatus::entity_not_found);
    static_cast<void>(world.create_entity());
    EXPECT_EQ(inspector.relations_from(EntityId{1}).progress.status, InspectionStatus::success);
    EXPECT_TRUE(inspector.relations_from(EntityId{1}).records.empty());
    EXPECT_TRUE(inspector.knowledge(EntityId{1}).records.empty());
    const auto revision = world.revision();
    WorldState replacement;
    static_cast<void>(replacement.create_entity());
    world = std::move(replacement);
    // Equal revisions cannot identify replacement: Host must restart its own traversal/session.
    EXPECT_EQ(inspector.entities({0, 1, revision}).progress.status, InspectionStatus::success);
    EXPECT_EQ(inspector.metadata().entities, 1U);
}
} // namespace

namespace {
TEST(InspectorTest, LastPossibleEntityIdPagesWithoutOverflowOrDenseAllocation) {
    CampaignSnapshot candidate;
    candidate.last_entity_id = UINT64_MAX;
    candidate.revision = UINT64_MAX;
    WorldState world;
    RandomStreams random{0};
    ASSERT_EQ(Persistence::restore(candidate, world, random), PersistenceStatus::success);
    const Inspector inspector{world};
    const auto page = inspector.entities({UINT64_MAX - 1, 256, UINT64_MAX});
    EXPECT_EQ(page.first, EntityId{UINT64_MAX});
    EXPECT_EQ(page.count, 1U);
    EXPECT_FALSE(page.progress.next_offset);
    EXPECT_EQ(inspector.entities({UINT64_MAX, 256}).count, 0U);
    EXPECT_TRUE(inspector.entity(EntityId{UINT64_MAX}));
    EXPECT_TRUE(inspector.diagnose({0, 0}).valid());
    EXPECT_EQ(inspector.metadata().entities, UINT64_MAX);
}
} // namespace
