#include <AzCore/Component/Entity.h>
#include <AzFramework/Entity/GameEntityContextBus.h>
#include <AzTest/AzTest.h>
#include <Mods/ModDataSystemComponent.h>
#include <Things/ModDataBus.h>
#include <Things/Testing/ThingsTestFixture.h>
#include <Things/ThingComponent.h>
#include <Things/ThingSystemBus.h>
#include <Things/ThingSystemComponent.h>
#include <Things/ThingsTypeIds.h>

namespace Things::Testing
{
    TEST(ThingsTypeIdTests, ComponentTypeIdsMatchDeclaredIds)
    {
        EXPECT_EQ(azrtti_typeid<ThingSystemComponent>(), AZ::TypeId(ThingSystemComponentTypeId));
        EXPECT_EQ(azrtti_typeid<ModDataSystemComponent>(), AZ::TypeId(ModDataSystemComponentTypeId));
        EXPECT_EQ(azrtti_typeid<ThingComponent>(), AZ::TypeId(ThingComponentTypeId));
    }

    //! Tests of the test application itself.
    using ThingsApplicationTests = ThingsTestFixture;

    TEST_F(ThingsApplicationTests, StartsWithTheThingsInterfaces)
    {
        EXPECT_NE(ThingSystemInterface::Get(), nullptr);
        EXPECT_NE(ModDataInterface::Get(), nullptr);
    }

    TEST_F(ThingsApplicationTests, GameEntitiesCanBeCreatedAndActivated)
    {
        AZ::Entity* entity = nullptr;
        AzFramework::GameEntityContextRequestBus::BroadcastResult(
            entity, &AzFramework::GameEntityContextRequests::CreateGameEntity, "Test");
        ASSERT_NE(entity, nullptr);

        EXPECT_EQ(entity->GetState(), AZ::Entity::State::Init) << "the game entity context initializes new entities";
        AzFramework::GameEntityContextRequestBus::Broadcast(&AzFramework::GameEntityContextRequests::ActivateGameEntity, entity->GetId());
        EXPECT_EQ(entity->GetState(), AZ::Entity::State::Active);
    }
} // namespace Things::Testing

AZ_UNIT_TEST_HOOK(DEFAULT_UNIT_TEST_ENV);
