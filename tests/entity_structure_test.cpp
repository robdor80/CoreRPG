#include <corerpg/entity_structure.hpp>

#include <gtest/gtest.h>

TEST(EntityStructureContractTest, RelationKindsAreStrongStableFullWidthLabels) {
    EXPECT_FALSE(corerpg::RelationKind{}.is_valid());
    EXPECT_TRUE(corerpg::RelationKind{UINT64_MAX}.is_valid());
    EXPECT_EQ(corerpg::RelationKind{UINT64_MAX}.value(), UINT64_MAX);
    EXPECT_LT(corerpg::RelationKind{1}, corerpg::RelationKind{2});
}

TEST(EntityStructureContractTest, RelationOrderingAndPopulationHaveNoHumanFields) {
    const corerpg::EntityRelation a{corerpg::EntityId{1}, corerpg::RelationKind{2},
                                    corerpg::EntityId{3}};
    const corerpg::EntityRelation b{corerpg::EntityId{1}, corerpg::RelationKind{3},
                                    corerpg::EntityId{2}};
    EXPECT_LT(a, b);
    const corerpg::PopulationRecord reserve{corerpg::EntityId{99}, UINT64_MAX, UINT64_MAX};
    EXPECT_EQ(reserve.total, reserve.latent);
    EXPECT_EQ(reserve.group.value(), 99U);
}
