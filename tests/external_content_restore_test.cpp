#include "../examples/external_content_restore.hpp"
#include "entity_structure_helpers.hpp"
#include <gtest/gtest.h>

namespace {
using namespace corerpg;
using namespace corerpg_example;

ExternalSaveMetadata identity() {
    return {1, {{"example.structural", "1.0.0", std::string(64, 'a'), std::string(64, 'b')}},
            std::string(64, 'c')};
}

TEST(ExternalContentRestoreTest, SameDefinitionsDoNotAuthorizeChangedPublication) {
    const auto catalog = corerpg_test::structural_catalog();
    WorldState world;
    RandomStreams random{5};
    const auto entity = world.create_entity();
    ASSERT_EQ(world.bind_definition(entity, *catalog.catalog(),
        *DefinitionId::parse("example.structural:unit")), DefinitionBindingChange::applied);
    const auto original = Persistence::capture(world, random);
    const auto bytes = Persistence::serialize(original).bytes;
    const auto installed = identity();
    auto saved = installed;
    saved.releases[0].pack_version = "2.0.0";
    auto result = restore_external_save(bytes, saved, installed, *catalog.catalog(), world, random);
    EXPECT_EQ(result.status, ExternalRestoreStatus::incompatible_content);
    EXPECT_FALSE(result.persistence_status);
    EXPECT_EQ(Persistence::capture(world, random), original);
    saved = installed;
    saved.releases[0].release_sha256 = std::string(64, 'd');
    result = restore_external_save(bytes, saved, installed, *catalog.catalog(), world, random);
    EXPECT_EQ(result.status, ExternalRestoreStatus::incompatible_content);
    EXPECT_EQ(Persistence::capture(world, random), original);
}

TEST(ExternalContentRestoreTest, MatchingMetadataUsesRealAtomicPersistenceAndLimits) {
    const auto catalog = corerpg_test::structural_catalog();
    WorldState source;
    RandomStreams source_rng{17};
    const auto entity = source.create_entity();
    ASSERT_EQ(source.bind_definition(entity, *catalog.catalog(),
        *DefinitionId::parse("example.structural:unit")), DefinitionBindingChange::applied);
    static_cast<void>(source_rng.stream(3).next());
    const auto snapshot = Persistence::capture(source, source_rng);
    const auto bytes = Persistence::serialize(snapshot).bytes;
    WorldState destination;
    RandomStreams destination_rng{99};
    const auto initial = Persistence::capture(destination, destination_rng);
    const auto metadata = identity();
    PersistenceLimits limits;
    limits.max_bytes = 0;
    const auto rejected = restore_external_save(bytes, metadata, metadata, *catalog.catalog(),
        destination, destination_rng, limits);
    EXPECT_EQ(rejected.status, ExternalRestoreStatus::invalid_save);
    EXPECT_EQ(rejected.persistence_status, PersistenceStatus::limit_exceeded);
    EXPECT_EQ(Persistence::capture(destination, destination_rng), initial);
    const auto restored = restore_external_save(bytes, metadata, metadata, *catalog.catalog(),
        destination, destination_rng);
    EXPECT_EQ(restored.status, ExternalRestoreStatus::success);
    EXPECT_EQ(Persistence::capture(destination, destination_rng), snapshot);
}
} // namespace
