// Cross-repository integration only. No Nimroel policy enters CoreRPG libraries.
#include <corerpg/content_catalog.hpp>
#include <corerpg/content_json/content_document_decoder.hpp>
#include <corerpg/content_json/content_pack_loader.hpp>
#include <corerpg/context_builder.hpp>
#include <corerpg/persistence.hpp>
#include <corerpg/simulation.hpp>
#include <corerpg/inspector.hpp>
#include "../examples/external_content_restore.hpp"

#include <gtest/gtest.h>
#include <jsoncons/json.hpp>

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>

namespace {
using namespace corerpg;
namespace cj = corerpg::content_json;
const std::filesystem::path pack_root{CORERPG_NIMROEL_PACK_DIR};
constexpr std::size_t document_limit = 1024 * 1024;
constexpr std::string_view prefix = "nimroel.treskal.villages:";

void require(bool value) {
    if (!value) throw std::runtime_error("invalid prevalidated Nimroel integration fixture");
}

std::string bytes(const std::filesystem::path& path) {
    std::ifstream stream{path, std::ios::binary};
    require(stream.good());
    std::string text(document_limit + 1, '\0');
    stream.read(text.data(), static_cast<std::streamsize>(text.size()));
    text.resize(static_cast<std::size_t>(stream.gcount()));
    require(!stream.bad() && text.size() <= document_limit);
    return text;
}

ContentPackInput memory_pack() {
    // The transport provides owned bytes. Decoder/Core APIs below do not need paths.
    auto document = cj::read_json_document(bytes(pack_root / "manifest.json"), {document_limit});
    require(document.succeeded());
    const auto manifest = cj::decode_content_pack_manifest(*document.document());
    require(manifest.succeeded());
    std::vector<ContentDefinitionInput> definitions;
    for (const auto& locator : manifest.decoded_manifest()->resource_locators()) {
        auto resource = cj::read_json_document(bytes(pack_root / locator), {document_limit});
        require(resource.succeeded());
        const auto decoded = cj::decode_resource_definition(*resource.document());
        require(decoded.succeeded());
        definitions.push_back(*decoded.definition());
    }
    return {manifest.decoded_manifest()->manifest(), std::move(definitions)};
}

DefinitionId definition(std::string_view local) {
    return *DefinitionId::parse(std::string{prefix} + std::string{local});
}

// Detached technical recipe, not a procedural generator or another state engine.
struct Fixture {
    WorldState world;
    RandomStreams random;
    EntityId village, smith, tavern, external, tool, first_observer, second_observer;
    RelationKind service, connection, association;
    std::uint64_t stream;

    Fixture(const ContentCatalog& catalog, const jsoncons::json& config)
        : random{std::stoull(config.at("world_seed").as<std::string>())},
          service{config.at("relation_kinds").at("service").as<std::uint64_t>()},
          connection{config.at("relation_kinds").at("external_connection").as<std::uint64_t>()},
          association{config.at("relation_kinds").at("population_association").as<std::uint64_t>()},
          stream{std::stoull(config.at("rng_probe_stream").as<std::string>())} {
        village = world.create_entity();
        smith = world.create_entity();  // service node, not building/professional
        tavern = world.create_entity(); // service node, no separate building implied
        external = world.create_entity(); // regional boundary, not an authored town
        tool = world.create_entity(); // category binding only, no TOOL/COND state
        require(world.bind_definition(village, catalog, definition("models/v1")) == DefinitionBindingChange::applied);
        require(world.bind_definition(smith, catalog, definition("capabilities/basic-blacksmithing")) == DefinitionBindingChange::applied);
        require(world.bind_definition(tavern, catalog, definition("capabilities/local-tavern")) == DefinitionBindingChange::applied);
        require(world.bind_definition(tool, catalog, definition("objects/ordinary-agricultural-tool")) == DefinitionBindingChange::applied);
        require(world.place_entity(smith, village) == ContainmentChange::applied);
        require(world.place_entity(tavern, village) == ContainmentChange::applied);
        require(world.place_entity(tool, smith) == ContainmentChange::applied);
        require(world.add_relation({village, service, smith}) == RelationChange::applied);
        require(world.add_relation({village, service, tavern}) == RelationChange::applied);
        require(world.add_relation({village, connection, external}) == RelationChange::applied);
        require(world.declare_population(village, config.at("population").as<std::uint64_t>()) == PopulationChange::applied);
        const auto issued = world.materialize_population(village, config.at("materialized").as<std::uint64_t>(),
                                                        config.at("max_batch").as<std::uint64_t>());
        require(issued.status == MaterializationStatus::applied && issued.count == 2);
        first_observer = issued.first;
        second_observer = EntityId{issued.first.value() + 1};
        for (const auto member : {first_observer, second_observer})
            require(world.add_relation({member, association, village}) == RelationChange::applied);
    }
};

class NimroelPackTest : public testing::Test {
  protected:
    jsoncons::json config = jsoncons::json::parse(bytes(pack_root / "integration-fixture.json"));
    CatalogBuildResult built = ContentCatalog::create({memory_pack()});
    std::unique_ptr<Fixture> fixture;
    void SetUp() override {
        ASSERT_TRUE(built.succeeded());
        fixture = std::make_unique<Fixture>(*built.catalog(), config);
    }
};

TEST_F(NimroelPackTest, CatalogIsDeclarativeAndMemoryMatchesFilesystem) {
    WorldState empty;
    const auto physical = cj::load_content_pack(pack_root);
    ASSERT_TRUE(physical.succeeded());
    const auto catalog = ContentCatalog::create({*physical.pack()});
    ASSERT_TRUE(catalog.succeeded());
    ASSERT_EQ(catalog.catalog()->size(), 4U);
    for (std::size_t i = 0; i < 4; ++i) {
        EXPECT_EQ(catalog.catalog()->definitions()[i].id(), built.catalog()->definitions()[i].id());
        EXPECT_EQ(catalog.catalog()->definitions()[i].kind(), built.catalog()->definitions()[i].kind());
    }
    EXPECT_EQ(empty.entity_count(), 0U);
    EXPECT_EQ(empty.revision(), 0U);
    EXPECT_TRUE(empty.populations().empty());
    const auto manifest = memory_pack().manifest();
    EXPECT_EQ(manifest.pack_id().value(), "nimroel.treskal.villages");
    EXPECT_TRUE(manifest.dependencies().empty());
}

TEST_F(NimroelPackTest, RealCatalogRejectsDuplicatesMissingAndWrongTypedReferences) {
    const auto pack = memory_pack();
    auto definitions = pack.definitions();
    definitions.push_back(definitions.front());
    auto duplicate = ContentCatalog::create({ContentPackInput{pack.manifest(), definitions}});
    EXPECT_FALSE(duplicate.succeeded());
    EXPECT_EQ(duplicate.catalog(), nullptr);
    for (const auto target : {definition("missing"), definition("capabilities/local-tavern")}) {
        auto defs = pack.definitions();
        defs.emplace_back(ResourceDefinition{definition("tests/reference"), *ResourceKind::parse("probe")},
                          std::vector<DefinitionReference>{{target, *ResourceKind::parse("object_category")}});
        const auto result = ContentCatalog::create({ContentPackInput{pack.manifest(), defs}});
        EXPECT_FALSE(result.succeeded());
        EXPECT_EQ(result.catalog(), nullptr);
    }
    const ContentManifest incompatible{pack.manifest().pack_id(), pack.manifest().pack_version(),
        *SemanticVersion::parse("0.2.0"), pack.manifest().minimum_core_version()};
    EXPECT_FALSE(ContentCatalog::create({ContentPackInput{incompatible, pack.definitions()}}).succeeded());
}

TEST_F(NimroelPackTest, RealReaderAndDecoderRejectCorruptOrRichEnvelope) {
    const auto text = bytes(pack_root / "resources/models/v1.json");
    EXPECT_FALSE(cj::read_json_document(text, {text.size() - 1}).succeeded());
    EXPECT_FALSE(cj::read_json_document(std::string{"\xef\xbb\xbf"} + text, {document_limit}).succeeded());
    EXPECT_FALSE(cj::read_json_document("{\"id\":1,\"id\":2}", {document_limit}).succeeded());
    auto rich = jsoncons::json::parse(text);
    rich["data"] = jsoncons::json::object();
    auto document = cj::read_json_document(rich.to_string(), {document_limit});
    ASSERT_TRUE(document.succeeded());
    EXPECT_FALSE(cj::decode_resource_definition(*document.document()).succeeded());
}

TEST_F(NimroelPackTest, LogicalReserveBindingsContainmentAndRelationsUseRealWorld) {
    const auto& f = *fixture;
    EXPECT_EQ(f.world.entity_count(), 7U);
    ASSERT_NE(f.world.definition_of(f.village), nullptr);
    EXPECT_EQ(f.world.definition_of(f.village)->value(), "nimroel.treskal.villages:models/v1");
    EXPECT_EQ(f.world.container_of(f.tool).container, f.smith);
    EXPECT_TRUE(f.world.has_relation({f.village, f.service, f.smith}));
    EXPECT_TRUE(f.world.has_relation({f.village, f.connection, f.external}));
    ASSERT_TRUE(f.world.population_of(f.village));
    EXPECT_EQ(f.world.population_of(f.village)->total, 50U);
    EXPECT_EQ(f.world.population_of(f.village)->latent, 48U);
    EXPECT_EQ(f.world.population_memberships().size(), 2U);
    EXPECT_EQ(f.world.population_origin(f.first_observer), f.village);
    EXPECT_EQ(f.world.definition_of(f.first_observer), nullptr); // no invented profession/name
    EXPECT_EQ(f.world.container_of(f.first_observer).status, ContainmentStatus::uncontained);
}

TEST_F(NimroelPackTest, RejectionsAndSimulationLimitsPreserveAllState) {
    auto& f = *fixture;
    const auto before = Persistence::capture(f.world, f.random);
    EXPECT_EQ(f.world.bind_definition(f.village, *built.catalog(), definition("missing")), DefinitionBindingChange::definition_not_found);
    EXPECT_EQ(f.world.bind_definition(f.village, *built.catalog(), definition("capabilities/local-tavern")), DefinitionBindingChange::definition_conflict);
    EXPECT_EQ(f.world.declare_population(f.village, 900), PopulationChange::population_conflict);
    EXPECT_EQ(f.world.materialize_population(f.village, 3, 2).status, MaterializationStatus::batch_limit_exceeded);
    EXPECT_EQ(f.world.materialize_population(f.village, 49, 49).status, MaterializationStatus::insufficient_latent);
    EXPECT_EQ(ActionEngine::process(f.world, f.random, {f.first_observer, PlaceEntityAction{f.village, f.smith}}).rejection,
              ActionRejection::cycle_detected);
    const std::array<ActionRequest, 1> requests{{{f.first_observer, PlaceEntityAction{f.tool, f.tavern}}}};
    std::array<ActionResult, 1> results;
    EXPECT_EQ(Simulation::step(f.world, f.random, requests, results, {0}).status, SimulationStatus::action_limit_exceeded);
    EXPECT_EQ(Persistence::capture(f.world, f.random), before);
    const auto step = Simulation::step(f.world, f.random, requests, results);
    EXPECT_EQ(step.status, SimulationStatus::completed);
    EXPECT_EQ(step.processed, 1U);
    EXPECT_EQ(results[0].outcome, ActionOutcome::accepted);
    EXPECT_EQ(f.world.container_of(f.tool).container, f.tavern); // structural move, not repair/custody
    EXPECT_EQ(f.world.revision(), before.revision + 1);
}

TEST_F(NimroelPackTest, EquivalentInputsAndReversedDefinitionOrderAreDeterministic) {
    const auto pack = memory_pack();
    auto reversed = pack.definitions();
    std::reverse(reversed.begin(), reversed.end());
    const auto other_catalog = ContentCatalog::create({ContentPackInput{pack.manifest(), reversed}});
    ASSERT_TRUE(other_catalog.succeeded());
    Fixture other{*other_catalog.catalog(), config};
    EXPECT_EQ(Persistence::capture(other.world, other.random), Persistence::capture(fixture->world, fixture->random));
    EXPECT_EQ(Persistence::serialize(Persistence::capture(other.world, other.random)).bytes,
              Persistence::serialize(Persistence::capture(fixture->world, fixture->random)).bytes);
}
void remember(Fixture& f) {
    for (const auto entity : {f.village, f.smith, f.tool, f.first_observer})
        require(f.world.learn_fact(f.first_observer, {entity}) == KnowledgeChange::applied);
    for (const auto entity : {f.village, f.tavern})
        require(f.world.learn_fact(f.second_observer, {entity}) == KnowledgeChange::applied);
    for (const auto entity : {f.village, f.smith, f.tool})
        require(f.world.learn_fact(f.first_observer, {entity, FactKind::definition}) == KnowledgeChange::applied);
    for (const auto key : {FactKey{f.tool, FactKind::containment}, FactKey{f.village, FactKind::population},
                          FactKey{f.smith, FactKind::population}, // known absence
                          FactKey{f.village, FactKind::relation, f.service, f.smith},
                          FactKey{f.first_observer, FactKind::population_origin}})
        require(f.world.learn_fact(f.first_observer, key) == KnowledgeChange::applied);
}

// Only a detached authorized view crosses this endpoint, never a privileged handle.
ContextResult observer_context(const ObserverView& view, const ContextRequest& request,
                               std::span<VisibleFact> output, ContextLimits limits = {}) noexcept {
    return ContextBuilder::build(view, request, output, limits);
}
static_assert(!std::is_constructible_v<ObserverView, const WorldState&>);
static_assert(!std::is_invocable_v<decltype(&observer_context), const ContentCatalog&,
                                 const ContextRequest&, std::span<VisibleFact>, ContextLimits>);
static_assert(!std::is_invocable_v<decltype(&observer_context), const Inspector&,
                                 const ContextRequest&, std::span<VisibleFact>, ContextLimits>);

TEST_F(NimroelPackTest, PartialKnowledgeStaleMemoriesAndGrantsRemainSeparate) {
    auto& f = *fixture;
    remember(f);
    const auto first = Visibility::project(f.world, f.first_observer);
    const auto second = Visibility::project(f.world, f.second_observer);
    EXPECT_TRUE(first.knows(f.smith));
    EXPECT_FALSE(second.knows(f.smith));
    EXPECT_FALSE(first.knows(f.tavern));
    EXPECT_FALSE(first.knows(f.external));
    EXPECT_EQ(second.find({f.smith, FactKind::population}), nullptr);
    ASSERT_NE(first.find({f.smith, FactKind::population}), nullptr);
    EXPECT_FALSE(first.find({f.smith, FactKind::population})->present);
    ASSERT_EQ(ActionEngine::process(f.world, f.random, {f.first_observer,
        PlaceEntityAction{f.tool, f.tavern}}).outcome, ActionOutcome::accepted);
    ASSERT_EQ(f.world.materialize_population(f.village, 1, 1).status, MaterializationStatus::applied);
    const auto old = Visibility::project(f.world, f.first_observer);
    EXPECT_EQ(old.find({f.tool, FactKind::containment})->reference, f.smith);
    EXPECT_EQ(old.find({f.village, FactKind::population})->latent, 48U);
    EXPECT_EQ(f.world.population_of(f.village)->latent, 47U);
    const auto snapshot = Persistence::capture(f.world, f.random);
    const std::array grants{FactKey{f.tool, FactKind::containment}, FactKey{f.tavern},
                           FactKey{f.village, FactKind::population}};
    const auto current = Visibility::project(f.world, f.first_observer, grants);
    EXPECT_EQ(current.find({f.tool, FactKind::containment})->reference, f.tavern);
    EXPECT_EQ(current.find({f.tool, FactKind::containment})->provenance(), FactProvenance::temporary_grant);
    EXPECT_EQ(current.find({f.village, FactKind::population})->latent, 47U);
    EXPECT_EQ(Visibility::project(f.world, f.first_observer), old);
    EXPECT_EQ(Persistence::capture(f.world, f.random), snapshot);
    EXPECT_FALSE(Visibility::project(f.world, f.first_observer).knows(f.tavern));
    EXPECT_EQ(first.find({f.tool, FactKind::containment})->reference, f.smith);
}

TEST_F(NimroelPackTest, ContextLimitsOrderAndHiddenQueriesUseAuthorizedViewOnly) {
    auto& f = *fixture;
    remember(f);
    const auto before = Persistence::capture(f.world, f.random);
    const auto view = Visibility::project(f.world, f.first_observer);
    std::array<VisibleFact, 32> storage;
    const auto complete = observer_context(view, {}, storage);
    ASSERT_EQ(complete.status, ContextStatus::complete);
    ASSERT_EQ(complete.emitted, view.facts().size());
    EXPECT_TRUE(std::equal(view.facts().begin(), view.facts().end(), storage.begin()));
    for (std::size_t i = 1; i < complete.emitted; ++i) EXPECT_LT(storage[i-1].key, storage[i].key);
    const auto limited = observer_context(view, {}, storage, {4096, 1, 65536});
    EXPECT_EQ(limited.status, ContextStatus::selection_limit);
    EXPECT_EQ(limited.emitted, 1U);
    EXPECT_EQ(storage[0], view.facts()[0]);
    EXPECT_EQ(observer_context(view, {}, storage, {0, 128, 65536}).status, ContextStatus::examination_limit);
    EXPECT_EQ(observer_context(view, {}, storage, {4096, 128, 58}).status, ContextStatus::content_limit);
    EXPECT_EQ(observer_context(view, {}, std::span<VisibleFact>{}).status, ContextStatus::output_capacity);
    ContextRequest filter;
    filter.subject = f.tool;
    filter.kind = FactKind::definition;
    const auto selected = observer_context(view, filter, storage);
    ASSERT_EQ(selected.emitted, 1U);
    EXPECT_EQ(storage[0].definition_id(), "nimroel.treskal.villages:objects/ordinary-agricultural-tool");
    for (const auto hidden : {f.external, f.tavern, EntityId{}, EntityId{999}}) {
        filter.subject = hidden;
        const auto empty = observer_context(view, filter, storage, {0, 0, 0});
        EXPECT_EQ(empty.status, ContextStatus::complete);
        EXPECT_EQ(empty.examined, 0U);
        EXPECT_EQ(empty.emitted, 0U);
    }
    const Inspector inspector{f.world};
    EXPECT_NE(inspector.definition(f.tavern, *built.catalog()), nullptr);
    EXPECT_TRUE(inspector.diagnose().valid());
    EXPECT_EQ(Visibility::project(f.world, f.first_observer), view);
    EXPECT_EQ(Persistence::capture(f.world, f.random), before);
}

corerpg_example::ExternalSaveMetadata installed_identity() {
    const auto lock = jsoncons::json::parse(bytes(pack_root / "content-lock.json"));
    return {1, {{lock.at("pack_id").as<std::string>(), lock.at("pack_version").as<std::string>(),
                 lock.at("definition_set_sha256").as<std::string>(), lock.at("release_sha256").as<std::string>()}},
            lock.at("files").at("integration-fixture.json").as<std::string>()};
}
TEST_F(NimroelPackTest, RealSaveRestoresEveryTableRngKnowledgeAndNextIdentity) {
    auto& f = *fixture;
    remember(f);
    ASSERT_EQ(ActionEngine::process(f.world, f.random, {f.first_observer,
        PlaceEntityAction{f.tool, f.tavern}}).outcome, ActionOutcome::accepted);
    ASSERT_EQ(ActionEngine::process(f.world, f.random, {f.first_observer,
        StreamRollAction{f.stream, 1, 100}}).outcome, ActionOutcome::accepted);
    const auto snapshot = Persistence::capture(f.world, f.random);
    ASSERT_EQ(snapshot.format_version, 3U);
    const auto serialized = Persistence::serialize(snapshot);
    ASSERT_EQ(serialized.status, PersistenceStatus::success);
    const auto decoded = Persistence::deserialize(serialized.bytes);
    ASSERT_EQ(decoded.status, PersistenceStatus::success);
    EXPECT_EQ(*decoded.snapshot, snapshot);
    EXPECT_EQ(Persistence::serialize(*decoded.snapshot).bytes, serialized.bytes);
    const std::array grants{FactKey{f.tavern}};
    EXPECT_TRUE(Visibility::project(f.world, f.first_observer, grants).knows(f.tavern));
    WorldState restored;
    RandomStreams restored_rng{999};
    const auto metadata = installed_identity();
    const auto result = corerpg_example::restore_external_save(serialized.bytes, metadata, metadata,
        *built.catalog(), restored, restored_rng);
    ASSERT_EQ(result.status, corerpg_example::ExternalRestoreStatus::success);
    EXPECT_EQ(Persistence::capture(restored, restored_rng), snapshot);
    EXPECT_EQ(restored.revision(), snapshot.revision);
    EXPECT_EQ(restored.container_of(f.tool).container, f.tavern);
    const auto remembered = Visibility::project(restored, f.first_observer);
    EXPECT_FALSE(remembered.knows(f.tavern));
    EXPECT_EQ(remembered.find({f.tool, FactKind::containment})->reference, f.smith);
    EXPECT_NE(remembered, Visibility::project(restored, f.second_observer));
    EXPECT_EQ(restored_rng.stream(f.stream).next(), f.random.stream(f.stream).next());
    EXPECT_EQ(restored.create_entity(), f.world.create_entity());
    EXPECT_EQ(restored.entity_count(), snapshot.last_entity_id + 1);
    const auto issued = restored.materialize_population(f.village, 1, 1);
    const auto expected = f.world.materialize_population(f.village, 1, 1);
    EXPECT_EQ(issued.first, expected.first);
    EXPECT_EQ(issued.count, 1U);
    EXPECT_EQ(Persistence::capture(restored, restored_rng), Persistence::capture(f.world, f.random));
}

TEST_F(NimroelPackTest, MissingDefinitionOrCorruptSaveNeverPublishesPartialWorld) {
    auto& f = *fixture;
    remember(f);
    auto serialized = Persistence::serialize(Persistence::capture(f.world, f.random));
    WorldState destination;
    RandomStreams random{7};
    static_cast<void>(destination.create_entity());
    static_cast<void>(random.stream(9).next());
    const auto original = Persistence::capture(destination, random);
    const auto metadata = installed_identity();
    const auto pack = memory_pack();
    std::vector<ContentDefinitionInput> only_model{
        ContentDefinitionInput{ResourceDefinition{definition("models/v1"), *ResourceKind::parse("settlement_model")}}};
    const auto missing = ContentCatalog::create({ContentPackInput{pack.manifest(), only_model}});
    ASSERT_TRUE(missing.succeeded());
    const auto result = corerpg_example::restore_external_save(serialized.bytes, metadata, metadata,
        *missing.catalog(), destination, random);
    EXPECT_EQ(result.status, corerpg_example::ExternalRestoreStatus::invalid_save);
    EXPECT_EQ(result.persistence_status, PersistenceStatus::definition_not_found);
    EXPECT_EQ(Persistence::capture(destination, random), original);
    EXPECT_EQ(Persistence::load(serialized.bytes, destination, random), PersistenceStatus::catalog_required);
    serialized.bytes[0] ^= 1U;
    EXPECT_EQ(corerpg_example::restore_external_save(serialized.bytes, metadata, metadata,
        *built.catalog(), destination, random).status, corerpg_example::ExternalRestoreStatus::invalid_save);
    EXPECT_EQ(Persistence::capture(destination, random), original);
}

TEST_F(NimroelPackTest, ExternalMetadataRejectsVersionsFingerprintsAndGeneratorContextBeforeCore) {
    auto& f = *fixture;
    const auto serialized = Persistence::serialize(Persistence::capture(f.world, f.random));
    const auto installed = installed_identity();
    const auto before = Persistence::capture(f.world, f.random);
    for (int axis = 0; axis < 8; ++axis) {
        auto saved = installed;
        switch (axis) {
        case 0: saved.releases[0].pack_version = "0.1.1"; break;
        case 1: saved.releases[0].definition_set_sha256 = std::string(64, 'a'); break;
        case 2: saved.releases[0].release_sha256 = std::string(64, 'b'); break;
        case 3: saved.generation_context_sha256 = std::string(64, 'c'); break;
        case 4: saved.releases.clear(); break;
        case 5: saved.releases.push_back(saved.releases[0]); break;
        case 6: saved.format_version = 2; break;
        case 7: saved.releases[0].pack_id = "other.game"; break;
        }
        const auto result = corerpg_example::restore_external_save(serialized.bytes, saved, installed,
            *built.catalog(), f.world, f.random);
        EXPECT_EQ(result.status, corerpg_example::ExternalRestoreStatus::incompatible_content);
        EXPECT_FALSE(result.persistence_status);
        EXPECT_EQ(Persistence::capture(f.world, f.random), before);
    }
}
} // namespace
