#include <corerpg/persistence.hpp>
#include <corerpg/visibility.hpp>
#include "../src/persistence_internal.hpp"
#include "entity_structure_helpers.hpp"
#include <algorithm>
#include <array>
#include <tuple>
#include <gtest/gtest.h>

namespace {
using namespace corerpg;
constexpr EntityId observer{1}, second{2}, subject{3}, group{4};

WorldState populated_world(const ContentCatalog& catalog) {
    WorldState world;
    for (int i = 0; i < 4; ++i) static_cast<void>(world.create_entity());
    EXPECT_EQ(world.bind_definition(subject, catalog, *DefinitionId::parse("example.structural:unit")), DefinitionBindingChange::applied);
    EXPECT_EQ(world.place_entity(subject, group), ContainmentChange::applied);
    EXPECT_EQ(world.declare_population(group, 50), PopulationChange::applied);
    const auto member = world.materialize_population(group, 1).first;
    EXPECT_EQ(world.add_relation({subject, RelationKind{9}, group}), RelationChange::applied);
    for (auto who : {observer, second})
        for (auto entity : {subject, group, member}) EXPECT_EQ(world.learn_fact(who, {entity}), KnowledgeChange::applied);
    for (auto kind : {FactKind::definition, FactKind::containment})
        EXPECT_EQ(world.learn_fact(observer, {subject, kind}), KnowledgeChange::applied);
    EXPECT_EQ(world.learn_fact(observer, {subject, FactKind::relation, RelationKind{9}, group}), KnowledgeChange::applied);
    EXPECT_EQ(world.learn_fact(observer, {group, FactKind::population}), KnowledgeChange::applied);
    EXPECT_EQ(world.learn_fact(observer, {member, FactKind::population_origin}), KnowledgeChange::applied);
    EXPECT_EQ(world.learn_fact(second, {subject, FactKind::population}), KnowledgeChange::applied);
    return world;
}

void crc(std::vector<std::uint8_t>& bytes) {
    bytes.resize(bytes.size() - 4);
    detail::append_integer(bytes, detail::save_checksum(bytes), 4);
}
void write(std::vector<std::uint8_t>& bytes, std::size_t offset, std::uint64_t value, unsigned size) {
    for (unsigned i = 0; i < size; ++i) { bytes[offset + i] = static_cast<std::uint8_t>(value); value >>= 8; }
}

TEST(KnowledgePersistenceTest, IndependentGoldenV3HasExactBytesAndKnownAbsence) {
    // Generated independently using Python struct little-endian and bitwise CRC32C.
    constexpr std::string_view hex =
        "43525047534156450300000001000000020000000000000004000000000000000807060504030201"
        "00000000000000000100000000000000000000000000000000000000000000000000000000000000"
        "00000000000000000200000000000000ffffffffffffffff2a000000000000000100000000000000"
        "02000000000000000100000000000000000000000000000000010000000000000000000000000000"
        "00000000000000000000000000000100000000000000020000000000000002000000000000000000"
        "00000000000000000000000000000000000000000000000000000000000000000000000096e6da89";
    std::vector<std::uint8_t> golden;
    const auto digit = [](char c) { return c <= '9' ? c - '0' : c - 'a' + 10; };
    for (std::size_t i = 0; i < hex.size(); i += 2)
        golden.push_back(static_cast<std::uint8_t>(digit(hex[i]) * 16 + digit(hex[i + 1])));
    WorldState world;
    ASSERT_EQ(world.create_entity(), observer);
    ASSERT_EQ(world.create_entity(), second);
    ASSERT_EQ(world.learn_fact(observer, {second}), KnowledgeChange::applied);
    ASSERT_EQ(world.learn_fact(observer, {second, FactKind::definition}), KnowledgeChange::applied);
    RandomStreams random{0x0102030405060708ULL};
    auto snapshot = Persistence::capture(world, random);
    snapshot.streams = {{UINT64_MAX, 42}};
    ASSERT_EQ(Persistence::serialize(snapshot).status, PersistenceStatus::success);
    EXPECT_EQ(Persistence::serialize(snapshot).bytes, golden);
    const auto decoded = Persistence::deserialize(golden);
    ASSERT_EQ(decoded.status, PersistenceStatus::success);
    EXPECT_EQ(*decoded.snapshot, snapshot);
    EXPECT_EQ(Persistence::serialize(*decoded.snapshot).bytes, golden);
}

TEST(KnowledgePersistenceTest, AllFactKindsRoundTripWithoutRefreshingMemories) {
    auto catalog = corerpg_test::structural_catalog();
    auto world = populated_world(*catalog.catalog());
    ASSERT_EQ(world.materialize_population(group, 1).status, MaterializationStatus::applied);
    ASSERT_EQ(world.remove_containment(subject), ContainmentChange::applied);
    ASSERT_EQ(world.remove_relation({subject, RelationKind{9}, group}), RelationChange::applied);
    RandomStreams random{456};
    static_cast<void>(random.stream(UINT64_MAX).next());
    const auto snapshot = Persistence::capture(world, random);
    ASSERT_EQ(snapshot.format_version, 3U);
    const auto encoded = Persistence::serialize(snapshot);
    ASSERT_EQ(encoded.status, PersistenceStatus::success);
    WorldState loaded;
    RandomStreams restored{0};
    ASSERT_EQ(Persistence::load(encoded.bytes, loaded, restored, {}, catalog.catalog()), PersistenceStatus::success);
    EXPECT_EQ(Persistence::capture(loaded, restored), snapshot);
    EXPECT_EQ(Visibility::project(loaded, observer), Visibility::project(world, observer));
    EXPECT_NE(Visibility::project(loaded, observer), Visibility::project(loaded, second));
    EXPECT_EQ(Visibility::project(loaded, observer).find({group, FactKind::population})->latent, 49U);
    EXPECT_EQ(loaded.population_of(group)->latent, 48U);
}

TEST(KnowledgePersistenceTest, LegacyLoadsClearKnowledgeAndNeverInferIt) {
    auto catalog = corerpg_test::structural_catalog();
    auto world = populated_world(*catalog.catalog());
    RandomStreams random{9};
    for (auto version : {1U, 2U}) {
        CampaignSnapshot legacy;
        legacy.format_version = version;
        legacy.last_entity_id = 4;
        legacy.revision = 4;
        const auto bytes = Persistence::serialize(legacy).bytes;
        ASSERT_EQ(Persistence::load(bytes, world, random), PersistenceStatus::success);
        EXPECT_TRUE(world.knowledge_of(observer).empty());
        EXPECT_TRUE(Visibility::project(world, observer).facts().empty());
        const auto decoded = Persistence::deserialize(bytes);
        ASSERT_EQ(decoded.status, PersistenceStatus::success);
        EXPECT_EQ(Persistence::serialize(*decoded.snapshot).bytes, bytes);
        world = populated_world(*catalog.catalog());
    }
}

TEST(KnowledgePersistenceTest, RejectsDowngradeDuplicatesOrderAndUnknownReferencesAtomically) {
    auto catalog = corerpg_test::structural_catalog();
    auto world = populated_world(*catalog.catalog());
    RandomStreams random{67};
    const auto original = Persistence::capture(world, random);
    const auto reject = [&](CampaignSnapshot candidate, PersistenceStatus expected) {
        EXPECT_EQ(Persistence::restore(candidate, world, random, {}, catalog.catalog()), expected);
        EXPECT_EQ(Persistence::capture(world, random), original);
    };
    for (auto version : {1U, 2U}) { auto s = original; s.format_version = version; reject(s, PersistenceStatus::unexpected_fields); }
    auto s = original;
    s.knowledge.insert(s.knowledge.begin(), s.knowledge.front());
    ++s.revision;
    reject(s, PersistenceStatus::noncanonical_knowledge);
    s = original;
    std::swap(s.knowledge.front(), s.knowledge.back());
    reject(s, PersistenceStatus::noncanonical_knowledge);
    s = original;
    s.knowledge.front().observer = {};
    reject(s, PersistenceStatus::invalid_knowledge);
    s = original;
    s.knowledge.erase(s.knowledge.begin()); // subject existence removed, other facts remain
    reject(s, PersistenceStatus::invalid_knowledge);
    s = original;
    s.knowledge.front().fact.total = 1; // unused data must be zero
    reject(s, PersistenceStatus::invalid_knowledge);
}

TEST(KnowledgePersistenceTest, LimitsAndRevisionExhaustionAreCheckedBeforeMutation) {
    WorldState world;
    ASSERT_EQ(world.create_entity(), observer);
    ASSERT_EQ(world.learn_fact(observer, {observer}), KnowledgeChange::applied);
    RandomStreams random{0};
    auto s = Persistence::capture(world, random);
    PersistenceLimits limits;
    limits.max_knowledge = 0;
    EXPECT_EQ(Persistence::serialize(s, limits).status, PersistenceStatus::limit_exceeded);
    const auto bytes = Persistence::serialize(s).bytes;
    EXPECT_EQ(Persistence::deserialize(bytes, limits).status, PersistenceStatus::limit_exceeded);
    limits = {};
    limits.max_bytes = bytes.size() - 1;
    EXPECT_EQ(Persistence::serialize(s, limits).status, PersistenceStatus::limit_exceeded);
    s.revision = UINT64_MAX;
    ASSERT_EQ(Persistence::restore(s, world, random), PersistenceStatus::success);
    EXPECT_EQ(world.learn_fact(observer, {observer}), KnowledgeChange::no_change);
    EXPECT_EQ(world.learn_fact(observer, {observer, FactKind::definition}), KnowledgeChange::revision_exhausted);
    EXPECT_EQ(world.revoke_fact(observer, {observer}), KnowledgeChange::revision_exhausted);
    EXPECT_EQ(world.revoke_knowledge(observer), KnowledgeChange::revision_exhausted);
    EXPECT_EQ(Persistence::capture(world, random), s);
}

TEST(KnowledgePersistenceTest, InvalidPayloadsAndTextLimitsAreRejectedWithoutNormalizing) {
    auto catalog = corerpg_test::structural_catalog();
    const auto world = populated_world(*catalog.catalog());
    const auto original = Persistence::capture(world, RandomStreams{0});
    for (auto kind : {FactKind::existence, FactKind::definition, FactKind::containment,
                      FactKind::relation, FactKind::population, FactKind::population_origin}) {
        auto s = original;
        const auto it = std::find_if(s.knowledge.begin(), s.knowledge.end(), [kind](const auto& r) {
            return r.observer == observer && r.fact.key.kind == kind;
        });
        ASSERT_NE(it, s.knowledge.end());
        switch (kind) {
        case FactKind::existence: it->fact.present = false; break;
        case FactKind::definition: it->fact.definition = std::make_shared<const std::string>("bad id"); break;
        case FactKind::containment: it->fact.reference = it->fact.key.subject; break;
        case FactKind::relation: it->fact.key.relation_kind = {}; break;
        case FactKind::population: it->fact.latent = it->fact.total + 1; break;
        case FactKind::population_origin: it->fact.reference = it->fact.key.subject; break;
        }
        EXPECT_EQ(Persistence::validate(s), PersistenceStatus::invalid_knowledge) << static_cast<int>(kind);
    }
    auto limits = PersistenceLimits{};
    limits.max_definition_bytes = 1;
    auto s = original;
    s.definitions.clear();
    EXPECT_EQ(Persistence::serialize(s, limits).status, PersistenceStatus::limit_exceeded);
    const auto bytes = Persistence::serialize(s).bytes;
    EXPECT_EQ(Persistence::deserialize(bytes, limits).status, PersistenceStatus::limit_exceeded);
    s = original;
    s.revision = s.last_entity_id;
    EXPECT_EQ(Persistence::validate(s), PersistenceStatus::invalid_world);
}

TEST(KnowledgePersistenceTest, EveryTruncationAndSingleByteCorruptionPreservesDestinations) {
    auto catalog = corerpg_test::structural_catalog();
    auto world = populated_world(*catalog.catalog());
    RandomStreams random{99};
    const auto snapshot = Persistence::capture(world, random);
    const auto bytes = Persistence::serialize(snapshot).bytes;
    ASSERT_FALSE(bytes.empty());
    for (std::size_t i = 0; i < bytes.size(); ++i) {
        EXPECT_NE(Persistence::load(std::span{bytes}.first(i), world, random, {}, catalog.catalog()), PersistenceStatus::success) << i;
        auto corrupt = bytes;
        corrupt[i] ^= 0x80;
        EXPECT_NE(Persistence::load(corrupt, world, random, {}, catalog.catalog()), PersistenceStatus::success) << i;
    }
    EXPECT_EQ(Persistence::capture(world, random), snapshot);
}

TEST(KnowledgePersistenceTest, RecomputedCrcCannotHideMalformedCountsFlagsOrLengths) {
    WorldState world;
    ASSERT_EQ(world.create_entity(), observer);
    ASSERT_EQ(world.learn_fact(observer, {observer}), KnowledgeChange::applied);
    auto bytes = Persistence::serialize(Persistence::capture(world, RandomStreams{0})).bytes;
    ASSERT_EQ(bytes.size(), 162U);
    for (const auto [offset, value, width] : std::array<std::tuple<std::size_t, std::uint64_t, unsigned>, 5>{
        {{88, UINT64_MAX, 8}, {112, 255, 1}, {129, 2, 1}, {154, UINT32_MAX, 4}, {96, 0, 8}}}) {
        auto corrupt = bytes;
        write(corrupt, offset, value, width);
        crc(corrupt);
        EXPECT_NE(Persistence::deserialize(corrupt).status, PersistenceStatus::success) << offset;
    }
}

TEST(KnowledgePersistenceTest, MissingCatalogForRememberedDefinitionIsAtomic) {
    auto catalog = corerpg_test::structural_catalog();
    auto world = populated_world(*catalog.catalog());
    RandomStreams random{1};
    auto s = Persistence::capture(world, random);
    s.definitions.clear(); // even remembered-only definitions require a catalog
    const auto original = Persistence::capture(world, random);
    EXPECT_EQ(Persistence::restore(s, world, random), PersistenceStatus::catalog_required);
    const auto empty = ContentCatalog::create({});
    EXPECT_EQ(Persistence::restore(s, world, random, {}, empty.catalog()), PersistenceStatus::definition_not_found);
    EXPECT_EQ(Persistence::capture(world, random), original);
}

TEST(KnowledgePersistenceTest, RestoringClonesCallerOwnedMutableStringAliases) {
    auto catalog = corerpg_test::structural_catalog();
    auto world = populated_world(*catalog.catalog());
    RandomStreams random{12};
    auto snapshot = Persistence::capture(world, random);
    auto alias = std::make_shared<std::string>("example.structural:unit");
    for (auto& record : snapshot.knowledge)
        if (record.fact.key.kind == FactKind::definition && record.fact.present) record.fact.definition = alias;
    ASSERT_EQ(Persistence::restore(snapshot, world, random, {}, catalog.catalog()), PersistenceStatus::success);
    const auto before = Persistence::capture(world, random);
    *alias = "changed.after:restore";
    EXPECT_EQ(Persistence::capture(world, random), before);
    EXPECT_EQ(Visibility::project(world, observer).find({subject, FactKind::definition})->definition_id(), "example.structural:unit");
}

TEST(KnowledgePersistenceTest, ContinuityPreservesRngAndBothObserversOverOneHundredSteps) {
    auto catalog = corerpg_test::structural_catalog();
    auto uninterrupted = populated_world(*catalog.catalog());
    RandomStreams first{1234};
    static_cast<void>(first.stream(7).next());
    WorldState restored;
    RandomStreams second_rng{0};
    const auto bytes = Persistence::serialize(Persistence::capture(uninterrupted, first)).bytes;
    ASSERT_EQ(Persistence::load(bytes, restored, second_rng, {}, catalog.catalog()), PersistenceStatus::success);
    for (int step = 0; step < 100; ++step) {
        const auto one = uninterrupted.create_entity();
        const auto two = restored.create_entity();
        ASSERT_EQ(one, two);
        const auto who = step % 2 ? observer : second;
        ASSERT_EQ(uninterrupted.learn_fact(who, {one}), restored.learn_fact(who, {two}));
        ASSERT_EQ(uninterrupted.learn_fact(who, {one, FactKind::population}), restored.learn_fact(who, {two, FactKind::population}));
        if (step % 3 == 0) ASSERT_EQ(uninterrupted.revoke_fact(who, {one}), restored.revoke_fact(who, {two}));
        EXPECT_EQ(first.stream(7).next(), second_rng.stream(7).next());
        EXPECT_EQ(first.stream(static_cast<std::uint64_t>(step)).next(), second_rng.stream(static_cast<std::uint64_t>(step)).next());
        const auto snapshot = Persistence::capture(uninterrupted, first);
        EXPECT_EQ(snapshot, Persistence::capture(restored, second_rng));
        EXPECT_EQ(Persistence::serialize(snapshot).bytes, Persistence::serialize(Persistence::capture(restored, second_rng)).bytes);
        EXPECT_EQ(Visibility::project(uninterrupted, who), Visibility::project(restored, who));
    }
}
} // namespace
