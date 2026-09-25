#pragma once

#include <AzCore/Asset/AssetCommon.h>
#include <AzCore/Component/Component.h>
#include <AzCore/std/smart_ptr/make_shared.h>
#include <AzCore/std/smart_ptr/shared_ptr.h>
#include <AzFramework/Spawnable/Spawnable.h>
#include <AzFramework/Spawnable/SpawnableEntitiesInterface.h>
#include <Things/ThingBus.h>
#include <Things/ThingsTypeIds.h>

namespace Things
{
    //! The part that gives a Thing a visible body: a prefab spawned as a child of the Thing's entity.
    //!
    //! Game logic never depends on the body; it spawns asynchronously and goes away with the Thing. An owned Thing is
    //! inside its owner (a carried sword, a learned skill), so its body leaves the world until it is top-level again,
    //! unless ShowWhenOwned is set.
    class ThingBodyComponent
        : public AZ::Component
        , public ThingBodyRequestBus::Handler
        , public ThingNotificationBus::Handler
    {
    public:
        AZ_COMPONENT(ThingBodyComponent, ThingBodyComponentTypeId);

        //! Reflects the prefab property.
        static void Reflect(AZ::ReflectContext* context);

        //! The body is parented to the Thing's transform.
        static void GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required);

        //! Provides the body service.
        static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided);

        //! A Thing has one body at most.
        static void GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible);

        //! The entities of the spawned body, once it has spawned; empty before that.
        AZStd::vector<AZ::EntityId> GetBodyEntities() const override;

        //! Whether the body is in the world.
        bool IsInWorld() const override;

        //! Brings the body into the world or takes it out.
        void OnOwnerChanged(AZ::EntityId oldOwner, AZ::EntityId newOwner) override;

    protected:
        //! Spawns the body when the Thing is in the world.
        void Activate() override;

        //! Despawns the body.
        void Deactivate() override;

    private:
        //! Spawns or despawns the body to match whether it belongs in the world.
        void UpdateBody();

        //! Loads the prefab and spawns it under the Thing.
        void SpawnBody();

        //! Releases the body.
        void DespawnBody();

        AZ::Data::Asset<AzFramework::Spawnable> m_prefab; //!< The body prefab.
        bool m_showWhenOwned = false; //!< Whether the body stays in the world while something owns the Thing.
        bool m_inWorld = false; //!< Whether the body is in the world now.
        AzFramework::EntitySpawnTicket m_ticket; //!< Keeps the body spawned; releasing it despawns the body.
        AZStd::shared_ptr<AZStd::vector<AZ::EntityId>> m_bodyEntities; //!< The spawned entities, shared with the spawn callback.
    };
} // namespace Things
