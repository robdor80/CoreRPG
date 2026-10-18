#include <corerpg/visibility.hpp>
#include <corerpg/persistence.hpp>
#include "entity_structure_helpers.hpp"
#include <array>
#include <type_traits>
#include <gtest/gtest.h>

namespace {
using namespace corerpg;
template <class T> concept ExposesOwnership = requires(const T& value) { value.definition.use_count(); };
static_assert(!ExposesOwnership<VisibleFact>);
static_assert(std::is_same_v<decltype(std::declval<const ObserverView&>().facts()), std::span<const VisibleFact>>);
constexpr EntityId observer{1}, target{2}, hidden{3};
WorldState fixture() {
    WorldState world;
    for (int i = 0; i < 3; ++i) static_cast<void>(world.create_entity());
    return world;
}
TEST(VisibilityTest, UnknownAndUninformedObserversHaveIdenticalEmptyViews) {
    const auto world = fixture();
    const auto empty = Visibility::project(world, observer);
    EXPECT_EQ(empty, Visibility::project(world, {}));
    EXPECT_EQ(empty, Visibility::project(world, EntityId{999}));
    EXPECT_TRUE(empty.facts().empty());
    for (auto id : {EntityId{}, observer, target, hidden, EntityId{999}}) {
        EXPECT_FALSE(empty.knows(id));
        EXPECT_EQ(empty.find({id}), nullptr);
    }
    EXPECT_FALSE(empty.next_entity_id());
}
TEST(VisibilityTest, HiddenMutationsDoNotChangeEnumerationFactsOrMetadata) {
    auto world = fixture();
    ASSERT_EQ(world.learn_fact(observer, {target}), KnowledgeChange::applied);
    const auto before = Visibility::project(world, observer);
    EXPECT_EQ(before.next_entity_id(), target);
    EXPECT_FALSE(before.next_entity_id(target));
    EXPECT_FALSE(before.next_entity_id(EntityId{UINT64_MAX}));
    EXPECT_EQ(before.facts().size(), 1U);
    for (auto kind : {FactKind::definition, FactKind::population, FactKind::containment})
        EXPECT_EQ(before.find({target, kind}), nullptr);
    ASSERT_EQ(world.declare_population(hidden, UINT64_MAX), PopulationChange::applied);
    ASSERT_TRUE(world.create_entity().is_valid());
    ASSERT_EQ(world.add_relation({hidden, RelationKind{7}, target}), RelationChange::applied);
    EXPECT_EQ(before, Visibility::project(world, observer));
}
TEST(VisibilityTest, TemporaryGrantsAreNonPersistentAndOrderIndependent) {
    auto world = fixture();
    ASSERT_EQ(world.place_entity(target, hidden), ContainmentChange::applied);
    const std::array grants{FactKey{target, FactKind::containment}, FactKey{hidden}, FactKey{target}};
    const auto snapshot = Persistence::capture(world, RandomStreams{78});
    const auto view = Visibility::project(world, observer, grants);
    ASSERT_NE(view.find(grants[0]), nullptr);
    EXPECT_EQ(view.find(grants[0])->reference, hidden);
    const std::array reordered{grants[2], grants[0], grants[1], grants[0]};
    EXPECT_EQ(view, Visibility::project(world, observer, reordered));
    EXPECT_TRUE(world.knowledge_of(observer).empty());
    EXPECT_TRUE(Visibility::project(world, observer).facts().empty());
    EXPECT_EQ(snapshot, Persistence::capture(world, RandomStreams{78}));
}
TEST(VisibilityTest, GrantsDoNotImplyExistenceOrRevealUnknownReferences) {
    auto world = fixture();
    ASSERT_EQ(world.place_entity(target, hidden), ContainmentChange::applied);
    ASSERT_EQ(world.add_relation({target, RelationKind{7}, hidden}), RelationChange::applied);
    const std::array grants{FactKey{target, FactKind::containment},
        FactKey{target, FactKind::relation, RelationKind{7}, hidden},
        FactKey{target, FactKind::definition}, FactKey{EntityId{999}},
        FactKey{target, static_cast<FactKind>(200)}};
    EXPECT_TRUE(Visibility::project(world, observer, grants).facts().empty());
    ASSERT_EQ(world.learn_fact(observer, {target}), KnowledgeChange::applied);
    const auto view = Visibility::project(world, observer, grants);
    EXPECT_EQ(view.facts().size(), 2U);
    EXPECT_FALSE(view.knows(hidden));
    EXPECT_EQ(view.find(grants[0]), nullptr);
    EXPECT_EQ(view.find(grants[1]), nullptr);
}
TEST(VisibilityTest, MemoriesOnlyRefreshWithExplicitAuthorization) {
    auto world = fixture();
    ASSERT_EQ(world.learn_fact(observer, {target}), KnowledgeChange::applied);
    const FactKey key{target, FactKind::population};
    ASSERT_EQ(world.learn_fact(observer, key), KnowledgeChange::applied);
    const auto before = Visibility::project(world, observer);
    ASSERT_EQ(world.declare_population(target, 99), PopulationChange::applied);
    EXPECT_EQ(before, Visibility::project(world, observer));
    const std::array grants{key};
    const auto temporary = Visibility::project(world, observer, grants);
    EXPECT_EQ(temporary.find(key)->total, 99U);
    EXPECT_EQ(before, Visibility::project(world, observer));
    EXPECT_FALSE(before.find(key)->present);
}
TEST(VisibilityTest, DetachedCopiesOutliveRevocationAndWorldDestruction) {
    ObserverView view;
    {
        auto world = fixture();
        auto catalog = corerpg_test::structural_catalog();
        const auto id = *DefinitionId::parse("example.structural:unit");
        ASSERT_EQ(world.bind_definition(target, *catalog.catalog(), id), DefinitionBindingChange::applied);
        ASSERT_EQ(world.learn_fact(observer, {target}), KnowledgeChange::applied);
        ASSERT_EQ(world.learn_fact(observer, {target, FactKind::definition}), KnowledgeChange::applied);
        view = Visibility::project(world, observer);
        ASSERT_EQ(world.revoke_knowledge(observer), KnowledgeChange::applied);
        EXPECT_TRUE(Visibility::project(world, observer).facts().empty());
    }
    auto copy = view;
    view = ObserverView{};
    ASSERT_NE(copy.find({target, FactKind::definition}), nullptr);
    EXPECT_EQ(copy.find({target, FactKind::definition})->definition_id(), "example.structural:unit");
}

TEST(VisibilityTest, ThreeObserversCanDetectIdentifyAndKnowAdditionalFactsIndependently) {
    // Common contract for an unknown contact, an identified individual, or a collective.
    auto world = fixture();
    const auto contact = world.create_entity();
    const auto collective = world.create_entity();
    auto catalog = corerpg_test::structural_catalog();
    ASSERT_EQ(world.bind_definition(contact, *catalog.catalog(), *DefinitionId::parse("example.structural:unit")), DefinitionBindingChange::applied);
    ASSERT_EQ(world.declare_population(collective, UINT64_MAX), PopulationChange::applied);
    ASSERT_EQ(world.add_relation({contact, RelationKind{8}, collective}), RelationChange::applied);
    for (auto who : {observer, target, hidden})
        ASSERT_EQ(world.learn_fact(who, {contact}), KnowledgeChange::applied);
    for (auto who : {target, hidden})
        ASSERT_EQ(world.learn_fact(who, {contact, FactKind::definition}), KnowledgeChange::applied);
    ASSERT_EQ(world.learn_fact(hidden, {collective}), KnowledgeChange::applied);
    ASSERT_EQ(world.learn_fact(hidden, {collective, FactKind::population}), KnowledgeChange::applied);
    const FactKey relation{contact, FactKind::relation, RelationKind{8}, collective};
    ASSERT_EQ(world.learn_fact(hidden, relation), KnowledgeChange::applied);
    const auto detected = Visibility::project(world, observer);
    const auto identified = Visibility::project(world, target);
    const auto informed = Visibility::project(world, hidden);
    EXPECT_TRUE(detected.knows(contact));
    EXPECT_EQ(detected.find({contact, FactKind::definition}), nullptr);
    EXPECT_FALSE(identified.knows(collective));
    EXPECT_EQ(identified.find(relation), nullptr);
    EXPECT_EQ(identified.find({contact, FactKind::definition})->definition_id(), "example.structural:unit");
    EXPECT_TRUE(informed.find(relation)->present);
    EXPECT_EQ(informed.find({collective, FactKind::population})->total, UINT64_MAX);
    ASSERT_EQ(world.revoke_knowledge(hidden), KnowledgeChange::applied);
    EXPECT_EQ(detected, Visibility::project(world, observer));
    EXPECT_EQ(identified, Visibility::project(world, target));
}
} // namespace
