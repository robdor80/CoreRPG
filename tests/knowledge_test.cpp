#include <corerpg/persistence.hpp>

#include "entity_structure_helpers.hpp"

#include <algorithm>
#include <gtest/gtest.h>

namespace {
using namespace corerpg;
constexpr EntityId a{1}, b{2}, subject{3}, container{4};
constexpr FactKey exists{subject};

WorldState world_fixture() {
    WorldState world;
    for (int i = 0; i < 4; ++i) static_cast<void>(world.create_entity());
    return world;
}

const KnownFact* fact(const WorldState& world, EntityId observer, FactKey key) {
    for (const auto& record : world.knowledge_of(observer))
        if (record.fact.key == key) return &record.fact;
    return nullptr;
}

TEST(KnowledgeTest, ExistenceDoesNotGrantAnyOtherFactOrOtherObserver) {
    auto world = world_fixture();
    EXPECT_EQ(world.learn_fact(a, exists), KnowledgeChange::applied);
    ASSERT_EQ(world.knowledge_of(a).size(), 1U);
    EXPECT_TRUE(world.knowledge_of(b).empty());
    EXPECT_TRUE(world.knowledge_of(subject).empty());
    EXPECT_EQ(fact(world, a, {subject, FactKind::definition}), nullptr);
    EXPECT_EQ(fact(world, a, {subject, FactKind::containment}), nullptr);
    EXPECT_EQ(world.learn_fact(a, exists), KnowledgeChange::no_change);
    EXPECT_EQ(world.revision(), 5U);
}

TEST(KnowledgeTest, ValidatesObserversSubjectsAndClosedKeys) {
    auto world = world_fixture();
    EXPECT_EQ(world.learn_fact({}, exists), KnowledgeChange::observer_not_found);
    EXPECT_EQ(world.learn_fact(a, {EntityId{99}}), KnowledgeChange::subject_not_found);
    EXPECT_EQ(world.learn_fact(a, {subject, static_cast<FactKind>(255)}), KnowledgeChange::invalid_fact);
    EXPECT_EQ(world.learn_fact(a, {subject, FactKind::existence, RelationKind{1}}), KnowledgeChange::invalid_fact);
    EXPECT_EQ(world.learn_fact(a, {subject, FactKind::relation, RelationKind{1}, EntityId{99}}), KnowledgeChange::invalid_fact);
    EXPECT_EQ(world.learn_fact(a, {subject, FactKind::population}), KnowledgeChange::subject_unknown);
    EXPECT_EQ(world.revision(), 4U);
}

TEST(KnowledgeTest, KnownAbsenceIsDifferentFromUnknownAndRequiresExplicitRefresh) {
    auto world = world_fixture();
    ASSERT_EQ(world.learn_fact(a, exists), KnowledgeChange::applied);
    const FactKey key{subject, FactKind::population};
    EXPECT_EQ(fact(world, a, key), nullptr);
    ASSERT_EQ(world.learn_fact(a, key), KnowledgeChange::applied);
    EXPECT_FALSE(fact(world, a, key)->present);
    ASSERT_EQ(world.declare_population(subject, 50), PopulationChange::applied);
    EXPECT_FALSE(fact(world, a, key)->present);
    EXPECT_EQ(world.learn_fact(a, key), KnowledgeChange::applied);
    EXPECT_TRUE(fact(world, a, key)->present);
    EXPECT_EQ(fact(world, a, key)->total, 50U);
    EXPECT_EQ(world.learn_fact(a, key), KnowledgeChange::no_change);
    EXPECT_EQ(world.revoke_fact(a, key), KnowledgeChange::applied);
    EXPECT_EQ(fact(world, a, key), nullptr);
    EXPECT_NE(fact(world, a, exists), nullptr);
}

TEST(KnowledgeTest, DefinitionIdentityIsExplicitAndImmutableByValue) {
    auto world = world_fixture();
    auto catalog = corerpg_test::structural_catalog();
    const auto id = *DefinitionId::parse("example.structural:unit");
    ASSERT_EQ(world.learn_fact(a, exists), KnowledgeChange::applied);
    const FactKey key{subject, FactKind::definition};
    ASSERT_EQ(world.learn_fact(a, key), KnowledgeChange::applied);
    ASSERT_EQ(world.bind_definition(subject, *catalog.catalog(), id), DefinitionBindingChange::applied);
    EXPECT_FALSE(fact(world, a, key)->present);
    EXPECT_EQ(world.learn_fact(a, key), KnowledgeChange::applied);
    EXPECT_EQ(fact(world, a, key)->definition_id(), id.value());
    EXPECT_EQ(world.learn_fact(a, key), KnowledgeChange::no_change);
    EXPECT_EQ(fact(world, b, key), nullptr);
}

TEST(KnowledgeTest, ReferencesRequireKnownExistenceAndRevocationCascadesOnlyForObserver) {
    auto world = world_fixture();
    ASSERT_EQ(world.place_entity(subject, container), ContainmentChange::applied);
    const FactKey key{subject, FactKind::containment};
    for (auto observer : {a, b}) {
        ASSERT_EQ(world.learn_fact(observer, exists), KnowledgeChange::applied);
        EXPECT_EQ(world.learn_fact(observer, key), KnowledgeChange::reference_unknown);
        ASSERT_EQ(world.learn_fact(observer, {container}), KnowledgeChange::applied);
        ASSERT_EQ(world.learn_fact(observer, key), KnowledgeChange::applied);
    }
    const auto revision = world.revision();
    EXPECT_EQ(world.revoke_fact(a, {container}), KnowledgeChange::applied);
    EXPECT_EQ(world.revision(), revision + 1);
    EXPECT_EQ(fact(world, a, key), nullptr);
    EXPECT_NE(fact(world, a, exists), nullptr);
    EXPECT_EQ(fact(world, b, key)->reference, container);
    EXPECT_EQ(world.container_of(subject).container, container);
}

TEST(KnowledgeTest, ExactRelationsIncludeKnownAbsenceWithoutInferringOtherEdges) {
    auto world = world_fixture();
    const FactKey key{subject, FactKind::relation, RelationKind{9}, container};
    ASSERT_EQ(world.learn_fact(a, exists), KnowledgeChange::applied);
    ASSERT_EQ(world.learn_fact(a, {container}), KnowledgeChange::applied);
    ASSERT_EQ(world.learn_fact(a, key), KnowledgeChange::applied);
    EXPECT_FALSE(fact(world, a, key)->present);
    ASSERT_EQ(world.add_relation({subject, RelationKind{9}, container}), RelationChange::applied);
    EXPECT_FALSE(fact(world, a, key)->present);
    ASSERT_EQ(world.learn_fact(a, key), KnowledgeChange::applied);
    EXPECT_TRUE(fact(world, a, key)->present);
    EXPECT_EQ(fact(world, a, {subject, FactKind::relation, RelationKind{10}, container}), nullptr);
    EXPECT_EQ(world.revoke_fact(a, {container}), KnowledgeChange::applied);
    EXPECT_EQ(fact(world, a, key), nullptr);
}

TEST(KnowledgeTest, PopulationOriginAndReserveAreIndependentRememberedFacts) {
    auto world = world_fixture();
    ASSERT_EQ(world.declare_population(subject, 8), PopulationChange::applied);
    const auto member = world.materialize_population(subject, 1).first;
    ASSERT_EQ(world.learn_fact(a, {member}), KnowledgeChange::applied);
    EXPECT_EQ(world.learn_fact(a, {member, FactKind::population_origin}), KnowledgeChange::reference_unknown);
    ASSERT_EQ(world.learn_fact(a, exists), KnowledgeChange::applied);
    ASSERT_EQ(world.learn_fact(a, {member, FactKind::population_origin}), KnowledgeChange::applied);
    EXPECT_EQ(fact(world, a, {member, FactKind::population_origin})->reference, subject);
    EXPECT_EQ(fact(world, a, {subject, FactKind::population}), nullptr);
    ASSERT_EQ(world.learn_fact(a, {subject, FactKind::population}), KnowledgeChange::applied);
    ASSERT_EQ(world.materialize_population(subject, 1).status, MaterializationStatus::applied);
    EXPECT_EQ(fact(world, a, {subject, FactKind::population})->latent, 7U);
}

TEST(KnowledgeTest, CopiesAndAssignmentsAreIndependentAndRevokeAllIsOneRevision) {
    auto world = world_fixture();
    ASSERT_EQ(world.learn_fact(a, exists), KnowledgeChange::applied);
    ASSERT_EQ(world.learn_fact(a, {subject, FactKind::population}), KnowledgeChange::applied);
    auto copy = world;
    WorldState assigned;
    assigned = world;
    const auto revision = world.revision();
    EXPECT_EQ(copy.revoke_knowledge(a), KnowledgeChange::applied);
    EXPECT_EQ(copy.revision(), revision + 1);
    EXPECT_TRUE(copy.knowledge_of(a).empty());
    EXPECT_EQ(copy.revoke_knowledge(a), KnowledgeChange::no_change);
    EXPECT_EQ(assigned.revoke_fact(a, exists), KnowledgeChange::applied);
    EXPECT_EQ(world.knowledge_of(a).size(), 2U);
}

TEST(KnowledgeTest, CanonicalOrderDoesNotDependOnInsertionOrderOrConsumeRng) {
    auto world = world_fixture();
    auto second = world;
    for (auto observer : {a, b}) {
        for (auto target : {subject, container}) ASSERT_EQ(world.learn_fact(observer, {target}), KnowledgeChange::applied);
    }
    for (auto observer : {b, a}) {
        for (auto target : {container, subject}) ASSERT_EQ(second.learn_fact(observer, {target}), KnowledgeChange::applied);
    }
    RandomStreams random{78};
    const auto before = Persistence::capture(world, random);
    EXPECT_EQ(before, Persistence::capture(second, random));
    for (int i = 0; i < 20; ++i) EXPECT_EQ(world.knowledge_of(a).size(), 2U);
    EXPECT_EQ(before, Persistence::capture(world, random));
}
} // namespace
