#pragma once

#include <AzCore/Asset/AssetCommon.h>
#include <AzCore/Component/Component.h>
#include <AzCore/std/smart_ptr/make_shared.h>
#include <AzCore/std/smart_ptr/shared_ptr.h>
#include <AzFramework/Spawnable/Spawnable.h>
#include <AzFramework/Spawnable/SpawnableEntitiesInterface.h>
#include <Things/ThingsTypeIds.h>

namespace Things
{
    //! The part that gives a Thing a visible body: a prefab spawned as a child of the Thing's entity.
    //!
    //! Game logic never depends on the body; it spawns asynchronously and goes away with the Thing.
    class ThingBodyComponent : public AZ::Component
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
        const AZStd::vector<AZ::EntityId>& GetBodyEntities() const;

    protected:
        //! Loads the prefab and spawns it under the Thing.
        void Activate() override;

        //! Despawns the body.
        void Deactivate() override;

    private:
        AZ::Data::Asset<AzFramework::Spawnable> m_prefab; //!< The body prefab.
        AzFramework::EntitySpawnTicket m_ticket; //!< Keeps the body spawned; releasing it despawns the body.
        AZStd::shared_ptr<AZStd::vector<AZ::EntityId>> m_bodyEntities; //!< The spawned entities, shared with the spawn callback.
    };
} // namespace Things
