#include <corerpg/persistence.hpp>
#include <corerpg/roll_action.hpp>

#include "../src/persistence_internal.hpp"
#include "entity_structure_helpers.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <string_view>

namespace {

using corerpg::CampaignSnapshot;
using corerpg::EntityId;
using corerpg::Persistence;
using corerpg::PersistenceStatus;
using corerpg::RelationKind;

CampaignSnapshot v2_sample() {
    CampaignSnapshot snapshot;
    snapshot.format_version = 2;
    snapshot.last_entity_id = 2;
    snapshot.revision = 5;
    snapshot.master_seed = 0x0102030405060708ULL;
    snapshot.streams = {{UINT64_MAX, 0}};
    snapshot.definitions.emplace_back(EntityId{2}, "example.structural:unit");
    snapshot.relations = {{EntityId{2}, RelationKind{7}, EntityId{1}}};
    snapshot.populations = {{EntityId{1}, 3, 2}};
    snapshot.population_memberships = {{EntityId{2}, EntityId{1}}};
    return snapshot;
}

std::vector<std::uint8_t> golden_v2() {
    constexpr std::string_view hex =
        "4352504753415645020000000100000002000000000000000500000000000000"
        "0807060504030201000000000000000001000000000000000100000000000000"
        "010000000000000001000000000000000100000000000000ffffffffffffffff"
        "00000000000000000200000000000000170000006578616d706c652e73747275"
        "63747572616c3a756e6974020000000000000007000000000000000100000000"
        "0000000100000000000000030000000000000002000000000000000200000000"
        "000000010000000000000042bb0856";
    const auto digit = [](char c) { return c <= '9' ? c - '0' : c - 'a' + 10; };
    std::vector<std::uint8_t> bytes;
    for (std::size_t i = 0; i < hex.size(); i += 2) {
        bytes.push_back(static_cast<std::uint8_t>(16 * digit(hex[i]) + digit(hex[i + 1])));
    }
    return bytes;
}

void update_crc(std::vector<std::uint8_t>& bytes) {
    bytes.resize(bytes.size() - 4);
    const auto crc = corerpg::detail::save_checksum(bytes);
    corerpg::detail::append_integer(bytes, crc, 4);
}

TEST(PopulationPersistenceTest, V2MatchesIndependentGoldenAndRestoresEveryField) {
    auto catalog = corerpg_test::structural_catalog();
    const auto snapshot = v2_sample();
    ASSERT_EQ(Persistence::validate(snapshot), PersistenceStatus::success);
    const auto bytes = Persistence::serialize(snapshot);
    ASSERT_EQ(bytes.status, PersistenceStatus::success);
    EXPECT_EQ(bytes.bytes, golden_v2());
    const auto decoded = Persistence::deserialize(golden_v2());
    ASSERT_EQ(decoded.status, PersistenceStatus::success);
    EXPECT_EQ(*decoded.snapshot, snapshot);
    corerpg::WorldState world;
    corerpg::RandomStreams random{0};
    ASSERT_EQ(Persistence::load(bytes.bytes, world, random, {}, catalog.catalog()),
              PersistenceStatus::success);
    EXPECT_EQ(Persistence::capture(world, random), snapshot);
    EXPECT_EQ(world.population_of(EntityId{1})->latent, 2U);
    EXPECT_EQ(world.population_origin(EntityId{2}), EntityId{1});
    ASSERT_NE(world.definition_of(EntityId{2}), nullptr);
    EXPECT_EQ(world.definition_of(EntityId{2})->value(), "example.structural:unit");
    EXPECT_TRUE(world.has_relation(snapshot.relations[0]));
    EXPECT_EQ(world.materialize_population(EntityId{1}, 1).first, EntityId{3});
    EXPECT_EQ(world.revision(), 6U);
    EXPECT_EQ(world.population_of(EntityId{1})->latent, 1U);
}

TEST(PopulationPersistenceTest, V1ReadingClearsAllNewStateAndReencodesExactly) {
    auto catalog = corerpg_test::structural_catalog();
    corerpg::WorldState world;
    corerpg::RandomStreams random{5};
    ASSERT_EQ(Persistence::restore(v2_sample(), world, random, {}, catalog.catalog()),
              PersistenceStatus::success);
    CampaignSnapshot legacy;
    legacy.last_entity_id = 3;
    legacy.revision = 4;
    legacy.containment = {{EntityId{1}, EntityId{3}}};
    const auto bytes = Persistence::serialize(legacy).bytes;
    ASSERT_EQ(Persistence::load(bytes, world, random), PersistenceStatus::success);
    EXPECT_EQ(Persistence::capture(world, random), legacy);
    EXPECT_TRUE(world.relations().empty());
    EXPECT_TRUE(world.populations().empty());
    EXPECT_TRUE(world.population_memberships().empty());
    EXPECT_EQ(world.definition_of(EntityId{2}), nullptr);
    EXPECT_EQ(Persistence::serialize(Persistence::capture(world, random)).bytes, bytes);
}

TEST(PopulationPersistenceTest, RequiredCatalogIsCheckedBeforePublishingEitherDestination) {
    corerpg::WorldState world;
    static_cast<void>(world.create_entity());
    corerpg::RandomStreams random{9};
    static_cast<void>(random.stream(9).next());
    const auto before = Persistence::capture(world, random);
    const auto* reference = random.find_stream(9);
    EXPECT_EQ(Persistence::load(golden_v2(), world, random), PersistenceStatus::catalog_required);
    const auto empty = corerpg::ContentCatalog::create({});
    EXPECT_EQ(Persistence::load(golden_v2(), world, random, {}, empty.catalog()),
              PersistenceStatus::definition_not_found);
    EXPECT_EQ(Persistence::capture(world, random), before);
    EXPECT_EQ(random.find_stream(9), reference);
}

TEST(PopulationPersistenceTest, NewFieldsCannotBeDowngradedIntoV1) {
    auto snapshot = v2_sample();
    snapshot.format_version = 1;
    EXPECT_EQ(Persistence::serialize(snapshot).status, PersistenceStatus::unexpected_fields);
    snapshot.format_version = 4; // v3 is supported by phase 4.9; retain unknown-version coverage.
    EXPECT_EQ(Persistence::validate(snapshot), PersistenceStatus::unsupported_version);
    corerpg::WorldState world;
    const auto entity = world.create_entity();
    const corerpg::EntityRelation self{entity, RelationKind{1}, entity};
    ASSERT_EQ(world.add_relation(self), corerpg::RelationChange::applied);
    ASSERT_EQ(world.remove_relation(self), corerpg::RelationChange::applied);
    corerpg::RandomStreams random{0};
    const auto historical = Persistence::capture(world, random);
    EXPECT_EQ(historical.format_version, 2U);
    EXPECT_TRUE(historical.relations.empty());
    ASSERT_EQ(Persistence::load(Persistence::serialize(historical).bytes, world, random),
              PersistenceStatus::success);
    EXPECT_EQ(world.revision(), 3U);

    CampaignSnapshot forced_empty;
    forced_empty.format_version = 2;
    const auto forced_bytes = Persistence::serialize(forced_empty).bytes;
    EXPECT_EQ(forced_bytes.size(), 92U);
    const auto decoded = Persistence::deserialize(forced_bytes);
    ASSERT_EQ(decoded.status, PersistenceStatus::success);
    EXPECT_EQ(*decoded.snapshot, forced_empty);
    EXPECT_EQ(Persistence::serialize(*decoded.snapshot).bytes, forced_bytes);
    ASSERT_EQ(Persistence::load(forced_bytes, world, random), PersistenceStatus::success);
    // Format is codec metadata, not a second world field. Recapture is lossless v1.
    EXPECT_EQ(Persistence::capture(world, random), CampaignSnapshot{});
}

TEST(PopulationPersistenceTest, ContinuationMatchesUninterruptedIdentityPopulationAndRng) {
    auto catalog = corerpg_test::structural_catalog();
    auto start = v2_sample();
    start.populations[0].total = 1'000;
    start.populations[0].latent = 999;
    corerpg::WorldState control;
    corerpg::WorldState loaded;
    corerpg::RandomStreams a{0};
    corerpg::RandomStreams b{0};
    ASSERT_EQ(Persistence::restore(start, control, a, {}, catalog.catalog()),
              PersistenceStatus::success);
    ASSERT_EQ(Persistence::load(Persistence::serialize(Persistence::capture(control, a)).bytes,
                                loaded, b, {}, catalog.catalog()),
              PersistenceStatus::success);
    const auto definition = *corerpg::DefinitionId::parse("example.structural:unit");
    for (int step = 0; step < 100; ++step) {
        const auto first = control.materialize_population(EntityId{1}, 2);
        const auto second = loaded.materialize_population(EntityId{1}, 2);
        ASSERT_EQ(first.status, corerpg::MaterializationStatus::applied);
        EXPECT_EQ(first.first, second.first);
        EXPECT_EQ(first.count, second.count);
        EXPECT_EQ(control.bind_definition(first.first, *catalog.catalog(), definition),
                  loaded.bind_definition(second.first, *catalog.catalog(), definition));
        const corerpg::EntityRelation relation{first.first, RelationKind{UINT64_MAX}, EntityId{1}};
        EXPECT_EQ(control.add_relation(relation), loaded.add_relation(relation));
        EXPECT_EQ(control.place_entity(first.first, EntityId{1}),
                  loaded.place_entity(second.first, EntityId{1}));
        for (const auto stream : std::array<std::uint64_t, 3>{0, UINT64_MAX, 77}) {
            const corerpg::RollAction action{first.first, 0, (1ULL << 63U)};
            const auto x = corerpg::resolve_roll(control, a.stream(stream), action);
            const auto y = corerpg::resolve_roll(loaded, b.stream(stream), action);
            EXPECT_EQ(x.status, y.status);
            EXPECT_EQ(x.value, y.value);
        }
        const auto x = Persistence::capture(control, a);
        const auto y = Persistence::capture(loaded, b);
        EXPECT_EQ(x, y);
        EXPECT_EQ(Persistence::serialize(x).bytes, Persistence::serialize(y).bytes);
    }
    EXPECT_EQ(control.population_of(EntityId{1})->latent, 799U);
    EXPECT_EQ(control.population_memberships().size(), 201U);
}

TEST(PopulationPersistenceTest, AllTruncationsAndSingleOctetCorruptionAreAtomic) {
    auto catalog = corerpg_test::structural_catalog();
    corerpg::WorldState world;
    corerpg::RandomStreams random{90};
    ASSERT_EQ(Persistence::restore(v2_sample(), world, random, {}, catalog.catalog()),
              PersistenceStatus::success);
    const auto before = Persistence::capture(world, random);
    const auto bytes = golden_v2();
    for (std::size_t size = 0; size < bytes.size(); ++size) {
        EXPECT_NE(
            Persistence::load(std::span{bytes}.first(size), world, random, {}, catalog.catalog()),
            PersistenceStatus::success);
        EXPECT_EQ(Persistence::capture(world, random), before);
    }
    for (std::size_t index = 0; index < bytes.size(); ++index) {
        auto corrupted = bytes;
        corrupted[index] ^= 1;
        EXPECT_NE(Persistence::load(corrupted, world, random, {}, catalog.catalog()),
                  PersistenceStatus::success);
        EXPECT_EQ(Persistence::capture(world, random), before);
    }
    auto extra = bytes;
    extra.insert(extra.end() - 4, 0);
    update_crc(extra);
    EXPECT_EQ(Persistence::load(extra, world, random, {}, catalog.catalog()),
              PersistenceStatus::invalid_size);
}

TEST(PopulationPersistenceTest, BadReferencesDuplicatesOrderingAndConservationAreRejected) {
    struct Invalid {
        CampaignSnapshot snapshot;
        PersistenceStatus status;
    };
    const auto make = [](auto change, PersistenceStatus status) {
        auto snapshot = v2_sample();
        snapshot.revision = 20; // Avoid masking structural errors with minimum revision.
        change(snapshot);
        return Invalid{std::move(snapshot), status};
    };
    const std::vector cases{
        make([](auto& s) { s.revision = 4; }, PersistenceStatus::invalid_world),
        make([](auto& s) { s.definitions[0].entity = {}; }, PersistenceStatus::invalid_definition),
        make([](auto& s) { s.definitions[0].definition_id = "invalid"; },
             PersistenceStatus::invalid_definition),
        make([](auto& s) { s.definitions.push_back(s.definitions[0]); },
             PersistenceStatus::noncanonical_definitions),
        make([](auto& s) { s.relations[0].source = {}; }, PersistenceStatus::invalid_relation),
        make([](auto& s) { s.relations[0].target = EntityId{3}; },
             PersistenceStatus::invalid_relation),
        make([](auto& s) { s.relations[0].kind = {}; }, PersistenceStatus::invalid_relation),
        make([](auto& s) { s.relations.push_back(s.relations[0]); },
             PersistenceStatus::noncanonical_relations),
        make([](auto& s) { s.populations[0].group = EntityId{3}; },
             PersistenceStatus::invalid_population),
        make([](auto& s) { s.populations[0].latent = 4; }, PersistenceStatus::invalid_population),
        make([](auto& s) { s.populations[0].latent = 0; }, PersistenceStatus::invalid_population),
        make([](auto& s) { s.populations.push_back(s.populations[0]); },
             PersistenceStatus::noncanonical_populations),
        make([](auto& s) { s.population_memberships[0].entity = EntityId{1}; },
             PersistenceStatus::invalid_membership),
        make([](auto& s) { s.population_memberships[0].population = {}; },
             PersistenceStatus::invalid_membership),
        make([](auto& s) { s.population_memberships.push_back(s.population_memberships[0]); },
             PersistenceStatus::noncanonical_memberships),
        make([](auto& s) { s.populations.clear(); }, PersistenceStatus::invalid_membership),
    };
    corerpg::WorldState world;
    corerpg::RandomStreams random{0};
    auto catalog = corerpg_test::structural_catalog();
    ASSERT_EQ(Persistence::restore(v2_sample(), world, random, {}, catalog.catalog()),
              PersistenceStatus::success);
    const auto before = Persistence::capture(world, random);
    for (const auto& test : cases) {
        EXPECT_EQ(Persistence::validate(test.snapshot), test.status);
        EXPECT_EQ(Persistence::restore(test.snapshot, world, random, {}, catalog.catalog()),
                  test.status);
        EXPECT_EQ(Persistence::capture(world, random), before);
    }
}

TEST(PopulationPersistenceTest, RecomputedChecksumStillRequiresValidLengthsAndInvariants) {
    auto invalid_text = golden_v2();
    invalid_text[116] = 'X'; // First canonical identifier byte after its prefix.
    update_crc(invalid_text);
    EXPECT_EQ(Persistence::deserialize(invalid_text).status, PersistenceStatus::invalid_definition);
    auto huge_length = golden_v2();
    std::fill(huge_length.begin() + 112, huge_length.begin() + 116, 0xFF);
    update_crc(huge_length);
    EXPECT_EQ(Persistence::deserialize(huge_length).status, PersistenceStatus::limit_exceeded);
    corerpg::PersistenceLimits permissive;
    permissive.max_definition_bytes = UINT64_MAX;
    EXPECT_EQ(Persistence::deserialize(huge_length, permissive).status,
              PersistenceStatus::invalid_size);
    auto unbalanced = golden_v2();
    unbalanced[179] = 1; // Population latent at offset 179, expected 2.
    update_crc(unbalanced);
    EXPECT_EQ(Persistence::deserialize(unbalanced).status, PersistenceStatus::invalid_population);
    auto huge_counts = golden_v2();
    std::fill(huge_counts.begin() + 40, huge_counts.begin() + 88, 0xFF);
    update_crc(huge_counts);
    permissive.max_links = permissive.max_streams = permissive.max_definitions = UINT64_MAX;
    permissive.max_relations = permissive.max_populations = permissive.max_memberships = UINT64_MAX;
    permissive.max_bytes = UINT64_MAX;
    EXPECT_EQ(Persistence::deserialize(huge_counts, permissive).status,
              PersistenceStatus::limit_exceeded);
}

TEST(PopulationPersistenceTest, LimitsAndNumericExhaustionPreserveBothValues) {
    const auto snapshot = v2_sample();
    const auto bytes = golden_v2();
    for (unsigned field = 0; field < 6; ++field) {
        corerpg::PersistenceLimits limits;
        if (field == 0) {
            limits.max_definitions = 0;
        }
        if (field == 1) {
            limits.max_relations = 0;
        }
        if (field == 2) {
            limits.max_populations = 0;
        }
        if (field == 3) {
            limits.max_memberships = 0;
        }
        if (field == 4) {
            limits.max_definition_bytes = 22;
        }
        if (field == 5) {
            limits.max_bytes = 206;
        }
        EXPECT_EQ(Persistence::serialize(snapshot, limits).status,
                  PersistenceStatus::limit_exceeded);
        EXPECT_EQ(Persistence::deserialize(bytes, limits).status,
                  PersistenceStatus::limit_exceeded);
    }
    CampaignSnapshot boundary;
    boundary.format_version = 2;
    boundary.last_entity_id = UINT64_MAX - 2;
    boundary.revision = UINT64_MAX - 1;
    boundary.populations = {{EntityId{1}, 5, 5}};
    corerpg::WorldState world;
    corerpg::RandomStreams random{0};
    ASSERT_EQ(Persistence::restore(boundary, world, random), PersistenceStatus::success);
    EXPECT_EQ(world.materialize_population(EntityId{1}, 1).first, EntityId{UINT64_MAX - 1});
    const auto before = Persistence::capture(world, random);
    EXPECT_EQ(world.materialize_population(EntityId{1}, 1).status,
              corerpg::MaterializationStatus::revision_exhausted);
    EXPECT_EQ(world.materialize_population(EntityId{1}, 2).status,
              corerpg::MaterializationStatus::identity_exhausted);
    EXPECT_EQ(world.declare_population(EntityId{1}, 5), corerpg::PopulationChange::no_change);
    EXPECT_EQ(world.declare_population(EntityId{2}, 1),
              corerpg::PopulationChange::revision_exhausted);
    EXPECT_EQ(world.add_relation({EntityId{1}, RelationKind{1}, EntityId{2}}),
              corerpg::RelationChange::revision_exhausted);
    EXPECT_EQ(Persistence::capture(world, random), before);
}

} // namespace
