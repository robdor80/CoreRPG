#include <corerpg/context_builder.hpp>
#include <corerpg/persistence.hpp>
#include "entity_structure_helpers.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <limits>
#include <type_traits>

namespace {
using namespace corerpg;
constexpr EntityId observer{1}, subject{2}, other{3};
static_assert(noexcept(ContextBuilder::build(std::declval<const ObserverView&>(), {}, {})));

class ContextBuilderTest : public ::testing::Test {
  protected:
    WorldState world;
    ObserverView view;
    std::array<VisibleFact, 16> output;
    void SetUp() override {
        for (int i = 0; i < 3; ++i) ASSERT_TRUE(world.create_entity().is_valid());
        ASSERT_EQ(world.learn_fact(observer, {other}), KnowledgeChange::applied);
        ASSERT_EQ(world.learn_fact(observer, {subject}), KnowledgeChange::applied);
        // Deliberately learn in noncanonical order.
        ASSERT_EQ(world.learn_fact(observer, {subject, FactKind::relation, RelationKind{9}, other}), KnowledgeChange::applied);
        ASSERT_EQ(world.learn_fact(observer, {subject, FactKind::containment}), KnowledgeChange::applied);
        ASSERT_EQ(world.learn_fact(observer, {subject, FactKind::definition}), KnowledgeChange::applied);
        view = Visibility::project(world, observer);
    }
};

TEST_F(ContextBuilderTest, CompleteCanonicalSelectionMatchesEveryAuthorizedValue) {
    const auto result = ContextBuilder::build(view, {}, output);
    EXPECT_EQ(result, (ContextResult{1, ContextStatus::complete, 5, 5, 5 * 59}));
    EXPECT_TRUE(std::equal(view.facts().begin(), view.facts().end(), output.begin()));
    for (std::size_t i = 1; i < result.emitted; ++i) EXPECT_LT(output[i - 1].key, output[i].key);
    EXPECT_FALSE(output[1].present); // known definition absence survives
    EXPECT_EQ(output[4].key.subject, other);
}

TEST_F(ContextBuilderTest, ExplicitFiltersAreConjunctiveAndPreserveCanonicalOrder) {
    EXPECT_EQ(ContextBuilder::build(view, {1, subject}, output).emitted, 4U);
    const auto result = ContextBuilder::build(view, {1, subject, FactKind::relation}, output);
    EXPECT_EQ(result, (ContextResult{1, ContextStatus::complete, 4, 1, 59}));
    EXPECT_EQ(output[0].key, (FactKey{subject, FactKind::relation, RelationKind{9}, other}));
    EXPECT_FALSE(output[0].present);
    EXPECT_EQ(ContextBuilder::build(view, {1, {}, FactKind::existence}, output).emitted, 2U);
    EXPECT_EQ(ContextBuilder::build(view, {1, other, FactKind::relation}, output).emitted, 0U);
}

TEST_F(ContextBuilderTest, ExaminationBudgetBoundsWorkEvenWhenNothingMatches) {
    const auto result = ContextBuilder::build(view, {1, {}, FactKind::population}, output, {2, 16, 4096});
    EXPECT_EQ(result, (ContextResult{1, ContextStatus::examination_limit, 2, 0, 0}));
    EXPECT_EQ(ContextBuilder::build(view, {}, output, {0, 16, 4096}).status, ContextStatus::examination_limit);
    EXPECT_EQ(ContextBuilder::build(view, {}, output, {5, 16, 4096}).status, ContextStatus::complete);
}

TEST_F(ContextBuilderTest, SubjectBudgetsCountOnlyItsRecordsIncludingKindMismatches) {
    static_cast<void>(ContextBuilder::build(view, {}, output));
    const auto original = output;
    EXPECT_EQ(ContextBuilder::build(view, {1, subject, FactKind::relation}, output, {3, 0, 0}),
              (ContextResult{1, ContextStatus::examination_limit, 3, 0, 0}));
    EXPECT_EQ(output, original);
    EXPECT_EQ(ContextBuilder::build(view, {1, subject}, output, {0, 16, 4096}),
              (ContextResult{1, ContextStatus::examination_limit, 0, 0, 0}));
    EXPECT_EQ(output, original);
    EXPECT_EQ(ContextBuilder::build(view, {1, subject}, output, {4, 4, 236}),
              (ContextResult{1, ContextStatus::complete, 4, 4, 236}));
    EXPECT_EQ(output[4], original[4]); // adjacent subject is neither counted nor written
    for (const auto status : {ContextStatus::selection_limit, ContextStatus::output_capacity,
                              ContextStatus::content_limit}) {
        output = original;
        auto limits = ContextLimits{16, 16, 4096};
        std::span<VisibleFact> storage{output};
        if (status == ContextStatus::selection_limit) limits.max_selected = 1;
        if (status == ContextStatus::output_capacity) storage = storage.first(1);
        if (status == ContextStatus::content_limit) limits.max_content_bytes = 59;
        EXPECT_EQ(ContextBuilder::build(view, {1, subject}, storage, limits),
                  (ContextResult{1, status, 2, 1, 59}));
        EXPECT_TRUE(std::equal(output.begin() + 1, output.end(), original.begin() + 1));
    }
}

TEST_F(ContextBuilderTest, SubjectAndKindKeepRelationKindAndTargetOrder) {
    for (const auto key : {FactKey{subject, FactKind::relation, RelationKind{9}, subject},
                           FactKey{subject, FactKind::relation, RelationKind{2}, other}})
        ASSERT_EQ(world.learn_fact(observer, key), KnowledgeChange::applied);
    view = Visibility::project(world, observer);
    const auto result = ContextBuilder::build(view, {1, subject, FactKind::relation}, output);
    ASSERT_EQ(result, (ContextResult{1, ContextStatus::complete, 6, 3, 177}));
    EXPECT_EQ(output[0].key, (FactKey{subject, FactKind::relation, RelationKind{2}, other}));
    EXPECT_EQ(output[1].key, (FactKey{subject, FactKind::relation, RelationKind{9}, subject}));
    EXPECT_EQ(output[2].key, (FactKey{subject, FactKind::relation, RelationKind{9}, other}));
}

TEST(ContextBuilderScaleTest, DefaultLimitsReachSubjectAfterMoreThan4096AuthorizedRecords) {
    WorldState world;
    const auto actor = world.create_entity();
    for (std::size_t i = 0; i < 4097; ++i) {
        const auto id = world.create_entity();
        ASSERT_EQ(world.learn_fact(actor, {id}), KnowledgeChange::applied);
    }
    const auto target = world.create_entity();
    for (const auto kind : {FactKind::existence, FactKind::definition, FactKind::containment})
        ASSERT_EQ(world.learn_fact(actor, {target, kind}), KnowledgeChange::applied);
    const auto following = world.create_entity();
    ASSERT_EQ(world.learn_fact(actor, {following}), KnowledgeChange::applied);
    const auto view = Visibility::project(world, actor);
    std::array<VisibleFact, 4> output;
    output.back().key.subject = EntityId{UINT64_MAX};
    const auto tail = output.back();
    EXPECT_EQ(ContextBuilder::build(view, {1, target}, output),
              (ContextResult{1, ContextStatus::complete, 3, 3, 177}));
    EXPECT_TRUE(std::equal(view.facts().begin() + 4097, view.facts().begin() + 4100, output.begin()));
    EXPECT_EQ(output.back(), tail);
}

TEST(ContextBuilderScaleTest, SingleSubjectStillHonorsDefaultAndMaximumExaminationBudgets) {
    WorldState world;
    const auto actor = world.create_entity();
    const auto target = world.create_entity();
    ASSERT_EQ(world.learn_fact(actor, {target}), KnowledgeChange::applied);
    for (std::uint64_t i = 1; i <= ContextLimits::examination_ceiling; ++i)
        ASSERT_EQ(world.learn_fact(actor, {target, FactKind::relation, RelationKind{i}, target}), KnowledgeChange::applied);
    const auto view = Visibility::project(world, actor);
    std::array<VisibleFact, 1> output;
    output[0].key.subject = EntityId{UINT64_MAX};
    const auto original = output;
    // No population matches: selection cannot mask exhaustion of examination.
    const ContextRequest request{1, target, FactKind::population};
    EXPECT_EQ(ContextBuilder::build(view, request, output),
              (ContextResult{1, ContextStatus::examination_limit, 4096, 0, 0}));
    EXPECT_EQ(ContextBuilder::build(view, request, output, {65536, 256, 1048576}),
              (ContextResult{1, ContextStatus::examination_limit, 65536, 0, 0}));
    EXPECT_EQ(output, original);
}

TEST_F(ContextBuilderTest, SelectionAndCapacityReportTruncationOnlyWhenAnotherMatchIsBlocked) {
    EXPECT_EQ(ContextBuilder::build(view, {}, output, {16, 2, 4096}),
              (ContextResult{1, ContextStatus::selection_limit, 3, 2, 118}));
    EXPECT_EQ(ContextBuilder::build(view, {}, std::span{output}.first(1)),
              (ContextResult{1, ContextStatus::output_capacity, 2, 1, 59}));
    EXPECT_EQ(ContextBuilder::build(view, {}, {}, {16, 16, 4096}).status, ContextStatus::output_capacity);
    EXPECT_EQ(ContextBuilder::build(view, {}, output, {16, 0, 4096}).status, ContextStatus::selection_limit);
    EXPECT_EQ(ContextBuilder::build(view, {}, std::span{output}.first(5), {16, 5, 295}).status,
              ContextStatus::complete);
    EXPECT_EQ(ContextBuilder::build(view, {1, {}, FactKind::population}, {}, {16, 0, 0}).status,
              ContextStatus::complete); // no matching record needs output
}

TEST_F(ContextBuilderTest, ContentUsesLogicalFixedWidthsAndActualDefinitionBytes) {
    auto catalog = corerpg_test::structural_catalog();
    ASSERT_EQ(world.bind_definition(subject, *catalog.catalog(), *DefinitionId::parse("example.structural:unit")), DefinitionBindingChange::applied);
    const std::array grants{FactKey{subject, FactKind::definition}};
    view = Visibility::project(world, observer, grants);
    constexpr std::size_t text_bytes = std::string_view{"example.structural:unit"}.size();
    const ContextRequest request{1, subject, FactKind::definition};
    EXPECT_EQ(ContextBuilder::build(view, request, output, {16, 16, 59 + text_bytes - 1}).status,
              ContextStatus::content_limit);
    EXPECT_EQ(ContextBuilder::build(view, request, output, {16, 16, 59 + text_bytes}),
              (ContextResult{1, ContextStatus::complete, 4, 1, 59 + text_bytes}));
    EXPECT_EQ(output[0].definition_id(), "example.structural:unit");
    EXPECT_EQ(output[0].provenance(), FactProvenance::temporary_grant);
    EXPECT_EQ(ContextBuilder::build(view, {}, output, {16, 16, 0}).status, ContextStatus::content_limit);
    EXPECT_EQ(ContextBuilder::build(view, {}, output, {16, 16, 58}).status, ContextStatus::content_limit);
}

TEST_F(ContextBuilderTest, ValidationRejectsBeforeWritingAndHasStablePrecedence) {
    static_cast<void>(ContextBuilder::build(view, {}, output));
    const auto before = output;
    constexpr auto maximum = std::numeric_limits<std::size_t>::max();
    EXPECT_EQ(ContextBuilder::build(view, {0, {}, static_cast<FactKind>(255)}, output, {maximum, maximum, maximum}),
              (ContextResult{1, ContextStatus::unsupported_version, 0, 0, 0}));
    EXPECT_EQ(ContextBuilder::build(view, {1, {}, static_cast<FactKind>(0)}, output).status, ContextStatus::invalid_request);
    for (const auto limits : {ContextLimits{maximum, 1, 59}, ContextLimits{1, maximum, 59}, ContextLimits{1, 1, maximum},
                            ContextLimits{65537, 1, 59}, ContextLimits{1, 257, 59}, ContextLimits{1, 1, 1048577}}) {
        EXPECT_EQ(ContextBuilder::build(view, {}, output, limits), (ContextResult{1, ContextStatus::invalid_limits, 0, 0, 0}));
    }
    EXPECT_EQ(output, before);
    EXPECT_EQ(ContextBuilder::build(view, {}, output, {65536, 256, 1048576}).status, ContextStatus::complete);
}

TEST_F(ContextBuilderTest, OnlyReportedPrefixChangesAndEmptyViewIsCompleteAtZeroLimits) {
    static_cast<void>(ContextBuilder::build(view, {}, output));
    const auto tail = output[1];
    EXPECT_EQ(ContextBuilder::build(view, {1, other}, output).emitted, 1U);
    EXPECT_EQ(output[1], tail);
    const auto before = output;
    EXPECT_EQ(ContextBuilder::build(ObserverView{}, {}, output, {0, 0, 0}), ContextResult{});
    EXPECT_EQ(output, before);
}

TEST_F(ContextBuilderTest, RepeatedBuildsAreIdenticalAndDoNotMutateWorldKnowledgeOrRng) {
    RandomStreams random{123};
    static_cast<void>(random.stream(7).next());
    const auto before = Persistence::capture(world, random);
    const auto original_view = view;
    for (const auto request : {ContextRequest{}, ContextRequest{1, subject, FactKind::relation}}) {
        const auto expected = ContextBuilder::build(view, request, output);
        const auto facts = output;
        for (int i = 0; i < 100; ++i) {
            EXPECT_EQ(ContextBuilder::build(view, request, output), expected);
            EXPECT_EQ(view, original_view);
            EXPECT_EQ(output, facts);
            EXPECT_EQ(Persistence::capture(world, random), before);
        }
    }
}
} // namespace
