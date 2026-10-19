#include <corerpg/context_builder.hpp>
#include <corerpg/persistence.hpp>
#include "entity_structure_helpers.hpp"
#include "../examples/authorized_context_host.hpp"

#include <gtest/gtest.h>
#include <algorithm>
#include <array>
#include <limits>

namespace {
using namespace corerpg;
constexpr EntityId observer{1}, subject{2}, hidden{3};
WorldState fixture() {
    WorldState world;
    for (int i = 0; i < 3; ++i) static_cast<void>(world.create_entity());
    static_cast<void>(world.learn_fact(observer, {subject}));
    static_cast<void>(world.learn_fact(observer, {subject, FactKind::containment}));
    return world;
}

TEST(ContextSecurityTest, DifferentHiddenWorldsAndRepeatedProbesProduceIdenticalContexts) {
    auto a = fixture();
    auto b = a;
    ASSERT_EQ(b.place_entity(subject, hidden), ContainmentChange::applied);
    ASSERT_EQ(b.declare_population(hidden, UINT64_MAX), PopulationChange::applied);
    ASSERT_EQ(b.add_relation({subject, RelationKind{42}, hidden}), RelationChange::applied);
    for (int i = 0; i < 100; ++i) ASSERT_TRUE(b.create_entity().is_valid());
    const auto av = Visibility::project(a, observer);
    const auto bv = Visibility::project(b, observer);
    ASSERT_EQ(av, bv);
    std::array<VisibleFact, 8> left, right;
    EXPECT_EQ(ContextBuilder::build(av, {}, left), ContextBuilder::build(bv, {}, right));
    EXPECT_EQ(left, right);
    const auto original = left;
    for (const auto id : {EntityId{}, hidden, EntityId{99}, EntityId{999}, EntityId{UINT64_MAX}}) {
        for (int attempt = 0; attempt < 16; ++attempt) {
            // Hidden, nonexistent, and invalid IDs return exactly the same envelope.
            EXPECT_EQ(ContextBuilder::build(av, {1, id}, left), (ContextResult{1, ContextStatus::complete, 0, 0, 0}));
            EXPECT_EQ(ContextBuilder::build(bv, {1, id}, right), (ContextResult{1, ContextStatus::complete, 0, 0, 0}));
            EXPECT_EQ(left, original);
            EXPECT_EQ(right, original); // no buffer writes on empty selection
            for (const auto limits : {ContextLimits{0, 0, 0}, ContextLimits{1, 1, 59}}) {
                EXPECT_EQ(ContextBuilder::build(av, {1, id, FactKind::relation}, left, limits), ContextResult{});
                EXPECT_EQ(ContextBuilder::build(bv, {1, id, FactKind::relation}, right, limits), ContextResult{});
                EXPECT_EQ(left, original);
                EXPECT_EQ(right, original);
            }
        }
    }
    for (const auto limit : {ContextLimits{0, 0, 0}, ContextLimits{1, 1, 59}, ContextLimits{8, 1, 59}}) {
        EXPECT_EQ(ContextBuilder::build(av, {}, left, limit), ContextBuilder::build(bv, {}, right, limit));
        EXPECT_EQ(left, right);
    }
}

TEST(ContextSecurityTest, KnownAbsenceUnknownAndStaleMemoryAreNotConflatedWithCurrentGrant) {
    auto world = fixture();
    const auto remembered = Visibility::project(world, observer);
    ASSERT_EQ(world.place_entity(subject, hidden), ContainmentChange::applied);
    std::array<VisibleFact, 8> output;
    const ContextRequest request{1, subject, FactKind::containment};
    auto result = ContextBuilder::build(remembered, request, output);
    ASSERT_EQ(result.emitted, 1U);
    EXPECT_FALSE(output[0].present);
    EXPECT_EQ(output[0].provenance(), FactProvenance::remembered);
    EXPECT_EQ(ContextBuilder::build(remembered, {1, subject, FactKind::population}, output).emitted, 0U);
    const std::array unsafe{FactKey{subject, FactKind::containment}};
    EXPECT_EQ(Visibility::project(world, observer, unsafe), remembered); // no hidden reference
    const std::array grants{FactKey{subject, FactKind::containment}, FactKey{hidden}};
    const auto current = Visibility::project(world, observer, grants);
    result = ContextBuilder::build(current, request, output);
    ASSERT_EQ(result.emitted, 1U);
    EXPECT_TRUE(output[0].present);
    EXPECT_EQ(output[0].reference, hidden);
    EXPECT_EQ(output[0].provenance(), FactProvenance::temporary_grant);
    EXPECT_EQ(ContextBuilder::build(Visibility::project(world, observer), request, output).emitted, 1U);
    EXPECT_FALSE(output[0].present);
    EXPECT_EQ(output[0].provenance(), FactProvenance::remembered);
}

TEST(ContextSecurityTest, ObserverIsolationRevocationAndSurvivingOwnedText) {
    std::array<VisibleFact, 8> delivered;
    ContextResult result;
    {
        auto world = fixture();
        auto catalog = corerpg_test::structural_catalog();
        ASSERT_EQ(world.bind_definition(subject, *catalog.catalog(), *DefinitionId::parse("example.structural:unit")), DefinitionBindingChange::applied);
        const std::array grants{FactKey{subject, FactKind::definition}};
        auto view = Visibility::project(world, observer, grants);
        auto copy = view;
        view = ObserverView{};
        result = ContextBuilder::build(copy, {}, delivered);
        ASSERT_EQ(result.emitted, 3U);
        EXPECT_EQ(ContextBuilder::build(Visibility::project(world, hidden), {}, {}).emitted, 0U);
        std::array<VisibleFact, 8> regenerated;
        const auto new_result = ContextBuilder::build(Visibility::project(world, observer), {}, regenerated);
        EXPECT_EQ(new_result.emitted, 2U); // grant removed, definition not persisted
        ASSERT_EQ(world.revoke_knowledge(observer), KnowledgeChange::applied);
        EXPECT_EQ(ContextBuilder::build(Visibility::project(world, observer), {}, regenerated).emitted, 0U);
        EXPECT_EQ(ContextBuilder::build(copy, {}, regenerated), result); // old authorized copies survive
    }
    EXPECT_EQ(delivered[1].definition_id(), "example.structural:unit");
    EXPECT_EQ(delivered[1].provenance(), FactProvenance::temporary_grant);
    const auto copy = delivered;
    delivered = {};
    EXPECT_EQ(copy[1].definition_id(), "example.structural:unit");
}

TEST(ContextSecurityTest, EverySupportedKindAndMaximumScalarsHaveStableTypedRepresentation) {
    auto world = fixture();
    ASSERT_EQ(world.learn_fact(observer, {hidden}), KnowledgeChange::applied);
    ASSERT_EQ(world.declare_population(hidden, UINT64_MAX), PopulationChange::applied);
    const auto batch = world.materialize_population(hidden, 1);
    ASSERT_EQ(batch.status, MaterializationStatus::applied);
    const EntityId emitted{4};
    ASSERT_EQ(world.learn_fact(observer, {emitted}), KnowledgeChange::applied);
    ASSERT_EQ(world.learn_fact(observer, {hidden, FactKind::population}), KnowledgeChange::applied);
    ASSERT_EQ(world.learn_fact(observer, {emitted, FactKind::population_origin}), KnowledgeChange::applied);
    ASSERT_EQ(world.add_relation({subject, RelationKind{UINT64_MAX}, hidden}), RelationChange::applied);
    ASSERT_EQ(world.learn_fact(observer, {subject, FactKind::relation, RelationKind{UINT64_MAX}, hidden}), KnowledgeChange::applied);
    ASSERT_EQ(world.learn_fact(observer, {subject, FactKind::definition}), KnowledgeChange::applied);
    std::array<VisibleFact, 16> output;
    const auto view = Visibility::project(world, observer);
    const auto result = ContextBuilder::build(view, {}, output);
    ASSERT_EQ(result, (ContextResult{1, ContextStatus::complete, 8, 8, 472}));
    const std::array keys{FactKey{subject}, FactKey{subject, FactKind::definition},
        FactKey{subject, FactKind::containment}, FactKey{subject, FactKind::relation, RelationKind{UINT64_MAX}, hidden},
        FactKey{hidden}, FactKey{hidden, FactKind::population}, FactKey{emitted}, FactKey{emitted, FactKind::population_origin}};
    // There are eight facts: explicit literal expectation guards platform ordering/accounting.
    // The assertions below are indexed by the canonical expected sequence.
    for (std::size_t i = 0; i < keys.size(); ++i) EXPECT_EQ(output[i].key, keys[i]);
    EXPECT_EQ(output[3].key.relation_kind.value(), UINT64_MAX);
    EXPECT_EQ(output[5].total, UINT64_MAX);
    EXPECT_EQ(output[5].latent, UINT64_MAX - 1);
    EXPECT_EQ(output[7].reference, hidden);
    EXPECT_FALSE(output[1].present);
    EXPECT_FALSE(output[2].present);
    for (std::size_t i = 0; i < result.emitted; ++i)
        EXPECT_EQ(output[i].provenance(), FactProvenance::remembered);
}

TEST(ContextSecurityTest, DirectedLookupReachesSubjectBeyondTheExaminationCeiling) {
    WorldState world;
    const auto actor = world.create_entity();
    for (std::size_t i = 0; i < ContextLimits::examination_ceiling + 2; ++i) {
        const auto entity = world.create_entity();
        ASSERT_EQ(world.learn_fact(actor, {entity}), KnowledgeChange::applied);
    }
    const auto view = Visibility::project(world, actor);
    std::array<VisibleFact, 256> output;
    EXPECT_EQ(ContextBuilder::build(view, {}, output, {65536, 256, 1048576}),
              (ContextResult{1, ContextStatus::selection_limit, 257, 256, 15104}));
    EXPECT_EQ(ContextBuilder::build(view, {1, {}, FactKind::population}, output, {65536, 256, 1048576}),
              (ContextResult{1, ContextStatus::examination_limit, 65536, 0, 0}));
    const auto original = output;
    const EntityId last{ContextLimits::examination_ceiling + 3};
    // The last subject follows 65,537 authorized records.
    EXPECT_EQ(ContextBuilder::build(view, {1, last}, output),
              (ContextResult{1, ContextStatus::complete, 1, 1, 59}));
    EXPECT_EQ(output[0], view.facts().back());
    EXPECT_TRUE(std::equal(output.begin() + 1, output.end(), original.begin() + 1));
    const auto before_empty = output;
    EXPECT_EQ(ContextBuilder::build(view, {1, EntityId{UINT64_MAX}}, output, {65536, 256, 1048576}), ContextResult{});
    EXPECT_EQ(output, before_empty);
}

TEST(ContextSecurityTest, HistoricalFormatsKeepMemoriesAndNeverPersistGrantsOrContext) {
    auto world = fixture();
    RandomStreams random{99};
    static_cast<void>(random.stream(7).next());
    const auto before = Persistence::capture(world, random);
    const auto bytes = Persistence::serialize(before).bytes;
    const std::array grants{FactKey{hidden}};
    std::array<VisibleFact, 16> output;
    EXPECT_EQ(ContextBuilder::build(Visibility::project(world, observer, grants), {}, output).emitted, 3U);
    EXPECT_EQ(Persistence::serialize(Persistence::capture(world, random)).bytes, bytes);
    WorldState restored;
    RandomStreams rr{0};
    ASSERT_EQ(Persistence::load(bytes, restored, rr), PersistenceStatus::success);
    EXPECT_EQ(Visibility::project(restored, observer), Visibility::project(world, observer));
    EXPECT_FALSE(Visibility::project(restored, observer).knows(hidden));
    auto legacy = before;
    legacy.knowledge.clear();
    for (const auto version : {save_format_version, save_format_version_v2}) {
        legacy.format_version = version;
        const auto encoded = Persistence::serialize(legacy);
        ASSERT_EQ(encoded.status, PersistenceStatus::success);
        ASSERT_EQ(Persistence::load(encoded.bytes, restored, rr), PersistenceStatus::success);
        EXPECT_EQ(ContextBuilder::build(Visibility::project(restored, observer), {}, output), ContextResult{});
    }
}

TEST(ContextHostSecurityTest, DomainDenialUnknownReferencesAndSessionReplacementNeverExecute) {
    auto world = fixture();
    RandomStreams random{5};
    ASSERT_EQ(world.learn_fact(observer, {observer}), KnowledgeChange::applied);
    const auto view = Visibility::project(world, observer);
    const auto before = Persistence::capture(world, random);
    const example_host::IssuedContext issued{9, 10, observer, world.revision()};
    const example_host::PlacementProposal proposal{9, observer, subject};
    for (const auto session : {example_host::AuthenticatedSession{observer, 10, false},
                              example_host::AuthenticatedSession{observer, 11, true},
                              example_host::AuthenticatedSession{subject, 10, true}}) {
        const auto result = example_host::evaluate(world, random, session, issued, view, proposal);
        EXPECT_EQ(result.status, example_host::Status::denied);
        EXPECT_FALSE(result.technical);
        EXPECT_EQ(Persistence::capture(world, random), before);
    }
    const example_host::AuthenticatedSession session{observer, 10, true};
    for (const auto bad : {example_host::PlacementProposal{8, observer, subject},
                          example_host::PlacementProposal{9, subject, subject},
                          example_host::PlacementProposal{9, observer, hidden},
                          example_host::PlacementProposal{9, observer, {}},
                          example_host::PlacementProposal{9, observer, EntityId{999}}}) {
        EXPECT_EQ(example_host::evaluate(world, random, session, issued, view, bad).status, example_host::Status::denied);
        EXPECT_EQ(Persistence::capture(world, random), before);
    }
    // Same revision in a restored campaign is insufficient: the Host epoch changed.
    ASSERT_EQ(Persistence::restore(before, world, random), PersistenceStatus::success);
    EXPECT_EQ(example_host::evaluate(world, random, {observer, 12, true}, issued, view, proposal).status,
              example_host::Status::denied);
    EXPECT_EQ(Persistence::capture(world, random), before);
}

TEST(ContextHostSecurityTest, CoreRevalidatesStalenessConditionsAndStructuralValidityAfterHostAuthorization) {
    auto world = fixture();
    RandomStreams random{5};
    ASSERT_EQ(world.learn_fact(observer, {observer}), KnowledgeChange::applied);
    const auto view = Visibility::project(world, observer);
    const example_host::AuthenticatedSession session{observer, 10, true};
    const example_host::IssuedContext issued{9, 10, observer, world.revision()};
    ASSERT_TRUE(world.create_entity().is_valid()); // older context revision
    const auto before = Persistence::capture(world, random);
    const auto stale = example_host::evaluate(world, random, session, issued, view, {9, observer, subject});
    ASSERT_TRUE(stale.technical);
    EXPECT_EQ(stale.technical->rejection, ActionRejection::stale_revision);
    EXPECT_EQ(Persistence::capture(world, random), before);
    auto fresh = issued;
    fresh.revision = world.revision();
    const auto invalid = example_host::evaluate(world, random, session, fresh, view, {9, observer, observer});
    ASSERT_TRUE(invalid.technical);
    EXPECT_EQ(invalid.technical->rejection, ActionRejection::self_containment);
    const std::array checks{RuleCheck{RuleCheckKind::direct_container_is, observer, subject}};
    EXPECT_EQ(ActionEngine::process(world, random,
        {observer, PlaceEntityAction{observer, subject}, world.revision(), checks}).rejection,
        ActionRejection::precondition_failed);
    EXPECT_EQ(Persistence::capture(world, random), before);
    const auto accepted = example_host::evaluate(world, random, session, fresh, view, {9, observer, subject});
    ASSERT_TRUE(accepted.technical);
    EXPECT_EQ(accepted.technical->outcome, ActionOutcome::accepted);
    EXPECT_EQ(world.container_of(observer).container, subject);
    EXPECT_EQ(Persistence::capture(world, random).streams, before.streams);
}
TEST(ContextSecurityTest, AdversarialEnumAndVersionValuesRejectWithoutWriting) {
    const auto world = fixture();
    const auto view = Visibility::project(world, observer);
    std::array<VisibleFact, 8> output;
    static_cast<void>(ContextBuilder::build(view, {}, output));
    const auto original = output;
    for (unsigned int raw = 0; raw <= 255; ++raw) {
        const auto result = ContextBuilder::build(view, {1, {}, static_cast<FactKind>(raw)}, output);
        if (raw < 1 || raw > 6) {
            EXPECT_EQ(result, (ContextResult{1, ContextStatus::invalid_request, 0, 0, 0}));
            EXPECT_EQ(output, original);
        }
        output = original;
    }
    EXPECT_EQ(ContextBuilder::build(view, {UINT32_MAX}, output).status, ContextStatus::unsupported_version);
    EXPECT_EQ(output, original);
}

TEST(ContextSecurityTest, OversizedAuthorizedTextCannotOverflowOrBypassTheContentCeiling) {
    constexpr auto ceiling = ContextLimits::content_ceiling;
    for (const auto length : {ceiling - context_fact_fixed_bytes, ceiling - context_fact_fixed_bytes + 1}) {
        const std::string text = "example.large:" + std::string(length - 14, 'a');
        ASSERT_EQ(text.size(), length);
        auto id = DefinitionId::parse(text);
        ASSERT_TRUE(id);
        const auto version = *SemanticVersion::parse("0.1.0");
        const auto catalog = ContentCatalog::create({ContentPackInput{
            ContentManifest{*PackId::parse("example.large"), version, version, version},
            {ContentDefinitionInput{ResourceDefinition{*id, *ResourceKind::parse("structural")}}}}});
        ASSERT_NE(catalog.catalog(), nullptr);
        auto world = fixture();
        ASSERT_EQ(world.bind_definition(subject, *catalog.catalog(), *id), DefinitionBindingChange::applied);
        ASSERT_EQ(world.learn_fact(observer, {subject, FactKind::definition}), KnowledgeChange::applied);
        const auto view = Visibility::project(world, observer);
        std::array<VisibleFact, 1> output;
        const auto result = ContextBuilder::build(view, {1, subject, FactKind::definition}, output, {65536, 256, ceiling});
        if (length == ceiling - context_fact_fixed_bytes) {
            EXPECT_EQ(result.status, ContextStatus::complete);
            EXPECT_EQ(result.content_bytes, ceiling);
            EXPECT_EQ(result.emitted, 1U);
            EXPECT_EQ(output[0].definition_id(), text);
        } else {
            EXPECT_EQ(result.status, ContextStatus::content_limit);
            EXPECT_EQ(result.content_bytes, 0U);
            EXPECT_EQ(result.emitted, 0U);
            EXPECT_TRUE(output[0].definition_id().empty());
        }
    }
}
} // namespace
