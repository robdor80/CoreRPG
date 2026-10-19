#include <corerpg/inspector.hpp>
#include "entity_structure_helpers.hpp"
#include "../src/world_validation_internal.hpp"
#include <gtest/gtest.h>

#include <functional>
#include <limits>

namespace {
using namespace corerpg;
CampaignSnapshot diagnostic_fixture() {
    WorldState world;
    for (int i = 0; i < 6; ++i) static_cast<void>(world.create_entity());
    static_cast<void>(world.place_entity(EntityId{1}, EntityId{2}));
    static_cast<void>(world.place_entity(EntityId{2}, EntityId{3}));
    static_cast<void>(world.add_relation({EntityId{1}, RelationKind{1}, EntityId{3}}));
    static_cast<void>(world.add_relation({EntityId{2}, RelationKind{1}, EntityId{1}}));
    static_cast<void>(world.declare_population(EntityId{4}, 5));
    static_cast<void>(world.materialize_population(EntityId{4}, 1));
    static_cast<void>(world.learn_fact(EntityId{6}, {EntityId{1}, FactKind::existence, {}, {}}));
    static_cast<void>(world.learn_fact(EntityId{6}, {EntityId{2}, FactKind::existence, {}, {}}));
    static_cast<void>(world.learn_fact(EntityId{6}, {EntityId{1}, FactKind::containment, {}, {}}));
    const auto catalog = corerpg_test::structural_catalog();
    const auto definition = *DefinitionId::parse("example.structural:unit");
    static_cast<void>(world.bind_definition(EntityId{1}, *catalog.catalog(), definition));
    static_cast<void>(world.bind_definition(EntityId{2}, *catalog.catalog(), definition));
    RandomStreams random{42};
    static_cast<void>(random.stream(1).next());
    static_cast<void>(random.stream(2).next());
    return Persistence::capture(world, random);
}
TEST(InspectorDiagnosticTest, ValidEmptyLiveAndCandidateAndExplicitResourceLimits) {
    EXPECT_FALSE(InspectionDiagnostic{}.valid());
    EXPECT_EQ(InspectionDiagnostic{}.status, InspectionDiagnosticStatus::not_run);
    WorldState empty;
    EXPECT_TRUE(Inspector{empty}.diagnose().valid());
    const auto snapshot = diagnostic_fixture();
    ASSERT_EQ(Persistence::validate(snapshot), PersistenceStatus::success);
    EXPECT_TRUE(Inspector::diagnose_snapshot(snapshot).valid());
    EXPECT_EQ(Inspector::diagnose_snapshot(snapshot), Inspector::diagnose_snapshot(snapshot));
    EXPECT_EQ(Inspector::diagnose_snapshot(snapshot, {0, 0}).status,
              InspectionDiagnosticStatus::limit_exceeded);
    EXPECT_EQ(Inspector::diagnose_snapshot(snapshot, {1000, 9}).status,
              InspectionDiagnosticStatus::limit_exceeded); // 8 count bytes + 2 colors
    EXPECT_TRUE(Inspector::diagnose_snapshot(snapshot, {1000, 10}).valid());
    PersistenceLimits persistence_limits;
    persistence_limits.max_bytes = 0;
    const auto result = Inspector::diagnose_snapshot(snapshot, {}, persistence_limits);
    EXPECT_EQ(result.status, InspectionDiagnosticStatus::limit_exceeded);
    EXPECT_EQ(result.severity, InspectionSeverity::warning);
    EXPECT_EQ(result.code, PersistenceStatus::limit_exceeded);
    EXPECT_FALSE(result.valid());
}
TEST(InspectorDiagnosticTest, FirstFailureIsSharedWithPersistenceAndNeverRepairsCandidate) {
    const auto valid = diagnostic_fixture();
    using Corrupt = std::function<void(CampaignSnapshot&)>;
    const std::pair<PersistenceStatus, Corrupt> cases[]{
        {PersistenceStatus::unsupported_version, [](auto& s) { s.format_version = 99; }},
        {PersistenceStatus::unsupported_rng_policy, [](auto& s) { s.rng_policy = 99; }},
        {PersistenceStatus::unexpected_fields, [](auto& s) { s.format_version = 2; }},
        {PersistenceStatus::invalid_world, [](auto& s) { s.revision = 0; }},
        {PersistenceStatus::invalid_link, [](auto& s) { s.containment[0].container = EntityId{99}; }},
        {PersistenceStatus::noncanonical_links, [](auto& s) { std::swap(s.containment[0], s.containment[1]); }},
        {PersistenceStatus::cycle_detected, [](auto& s) { s.containment[1].container = EntityId{1}; }},
        {PersistenceStatus::noncanonical_streams, [](auto& s) { s.streams[1].id = s.streams[0].id; }},
        {PersistenceStatus::invalid_definition, [](auto& s) { s.definitions[0].definition_id = "invalid"; }},
        {PersistenceStatus::noncanonical_definitions, [](auto& s) { s.definitions[1].entity = EntityId{1}; }},
        {PersistenceStatus::invalid_relation, [](auto& s) { s.relations[0].target = {}; }},
        {PersistenceStatus::noncanonical_relations, [](auto& s) { s.relations[1] = s.relations[0]; }},
        {PersistenceStatus::invalid_population, [](auto& s) { s.populations[0].latent = 6; }},
        {PersistenceStatus::invalid_population, [](auto& s) { s.populations[0].latent = 5; }},
        {PersistenceStatus::noncanonical_populations, [](auto& s) {
            s.populations.push_back(s.populations[0]); s.revision += 1;
        }},
        {PersistenceStatus::invalid_membership, [](auto& s) { s.population_memberships[0].population = EntityId{3}; }},
        {PersistenceStatus::noncanonical_memberships, [](auto& s) { s.population_memberships.push_back(s.population_memberships[0]); }},
        {PersistenceStatus::invalid_knowledge, [](auto& s) { s.knowledge[0].observer = EntityId{99}; }},
        {PersistenceStatus::invalid_knowledge, [](auto& s) { s.knowledge.erase(s.knowledge.begin()); }},
        {PersistenceStatus::noncanonical_knowledge, [](auto& s) { std::swap(s.knowledge[0], s.knowledge[1]); }},
    };
    for (const auto& [expected, corrupt] : cases) {
        auto candidate = valid;
        corrupt(candidate);
        const auto before = candidate;
        const auto diagnostic = Inspector::diagnose_snapshot(candidate);
        EXPECT_EQ(diagnostic.code, expected);
        EXPECT_EQ(diagnostic.code, Persistence::validate(candidate));
        EXPECT_EQ(diagnostic.status, InspectionDiagnosticStatus::invariant_violation);
        EXPECT_EQ(diagnostic.severity, InspectionSeverity::error);
        EXPECT_EQ(diagnostic, Inspector::diagnose_snapshot(candidate));
        EXPECT_EQ(candidate, before);
    }
    auto multiple_errors = valid;
    multiple_errors.revision = 0;
    multiple_errors.containment[0].container = {};
    EXPECT_EQ(Inspector::diagnose_snapshot(multiple_errors).code, PersistenceStatus::invalid_world);
}
TEST(InspectorDiagnosticTest, LiveAdapterRejectsNullBindingAndMalformedTablesWithoutUnsafeLookup) {
    // Invalid live states cannot be constructed through WorldState's public mutators.
    // Exercise the exact borrowed adapter used by Inspector, without adding a backdoor.
    std::pair<EntityId, std::shared_ptr<const DefinitionId>> binding{EntityId{1}, {}};
    detail::WorldValidationView view{2, 10, {}, std::span{&binding, 1}, {}, {}, {}, {}};
    EXPECT_EQ(detail::validate_world_structure(view), PersistenceStatus::invalid_definition);
    const std::pair<EntityId, EntityId> cycle[]{{EntityId{1}, EntityId{2}}, {EntityId{2}, EntityId{1}}};
    view.definitions = {};
    view.containment = cycle;
    EXPECT_EQ(detail::validate_world_structure(view), PersistenceStatus::cycle_detected);
    const std::pair<EntityId, EntityId> unordered[]{{EntityId{2}, EntityId{1}}, {EntityId{1}, EntityId{2}}};
    view.containment = unordered;
    EXPECT_EQ(detail::validate_world_structure(view), PersistenceStatus::noncanonical_links);
}
TEST(InspectorDiagnosticTest, DeepContainmentAndHugeImplicitIdRangeUseOnlySparseScratch) {
    CampaignSnapshot candidate;
    candidate.format_version = 2;
    candidate.last_entity_id = std::numeric_limits<std::uint64_t>::max() - 10000;
    candidate.revision = std::numeric_limits<std::uint64_t>::max();
    for (std::uint64_t i = 1; i <= 4096; ++i)
        candidate.containment.push_back({EntityId{i}, EntityId{i + 1}});
    ASSERT_TRUE(Inspector::diagnose_snapshot(candidate, {4096, 4096}).valid());
    WorldState world;
    RandomStreams random{42};
    ASSERT_EQ(Persistence::restore(candidate, world, random), PersistenceStatus::success);
    const Inspector inspector{world};
    EXPECT_TRUE(inspector.diagnose({4096, 4096}).valid());
    EXPECT_EQ(inspector.diagnose({4096, 4095}).status, InspectionDiagnosticStatus::limit_exceeded);
    EXPECT_EQ(inspector.entities({candidate.last_entity_id - 1, 256}).count, 1U);
    candidate.containment.back().container = EntityId{1};
    EXPECT_EQ(Inspector::diagnose_snapshot(candidate).code, PersistenceStatus::cycle_detected);
}
} // namespace
