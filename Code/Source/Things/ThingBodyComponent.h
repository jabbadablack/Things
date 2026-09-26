#pragma once

#include <AzCore/Asset/AssetCommon.h>
#include <AzCore/Component/Component.h>
#include <AzCore/std/containers/map.h>
#include <AzCore/std/containers/unordered_map.h>
#include <AzCore/std/containers/unordered_set.h>
#include <AzCore/std/smart_ptr/make_shared.h>
#include <AzCore/std/smart_ptr/shared_ptr.h>
#include <AzFramework/Spawnable/Spawnable.h>
#include <AzFramework/Spawnable/SpawnableEntitiesInterface.h>
#include <Things/ThingBus.h>
#include <Things/ThingsTypeIds.h>

namespace Things
{
    //! A named look of a Thing's body, e.g. a torch held in a hand.
    struct BodyLook
    {
        AZ_TYPE_INFO(BodyLook, BodyLookTypeId);

        //! Reflects the look.
        static void Reflect(AZ::ReflectContext* context);

        AZ::Data::Asset<AzFramework::Spawnable> m_prefab; //!< The look's prefab.
        AZStd::string m_socket; //!< While the Thing is owned, the look sits on the entity of this name in its owner's body.
    };

    //! The part that gives a Thing a visible body: prefabs spawned under the Thing's entity.
    //!
    //! Game logic never depends on the body. Every look (the default Prefab and the named Looks) spawns when the Thing
    //! activates, deactivated, and after that the body only activates and deactivates entities, so picking things up
    //! and putting them down spawns nothing. The default look shows while the Thing is top-level: an owned Thing is
    //! inside its owner (a carried sword, a learned skill). A named look shows whether owned or not; while owned it
    //! sits on its Socket entity in the owner's body and shows only while the owner's body does. HiddenParts names body
    //! entities that start hidden, e.g. flames shown only while burning.
    class ThingBodyComponent
        : public AZ::Component
        , public ThingBodyRequestBus::Handler
        , public ThingNotificationBus::Handler
        , public ThingBodyNotificationBus::Handler
    {
    public:
        AZ_COMPONENT(ThingBodyComponent, ThingBodyComponentTypeId);

        //! Reflects the prefab, looks and hidden parts.
        static void Reflect(AZ::ReflectContext* context);

        //! The body is parented to the Thing's transform.
        static void GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required);

        //! Provides the body service.
        static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided);

        //! A Thing has one body at most.
        static void GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible);

        // ThingBodyRequests...
        AZStd::vector<AZ::EntityId> GetBodyEntities() const override;
        bool IsInWorld() const override;
        bool IsSpawning() const override;
        void SetLook(const AZStd::string& look) override;
        const AZStd::string& GetLook() const override;
        void SetShown(bool shown) override;
        bool IsShown() const override;
        void SetPartShown(const AZStd::string& part, bool shown) override;
        AZ::EntityId FindBodyEntity(const AZStd::string& name) const override;

        //! Follows the new owner's body.
        void OnOwnerChanged(AZ::EntityId oldOwner, AZ::EntityId newOwner) override;

        //! The owner's body spawned: sits the current look on its socket.
        void OnBodySpawned() override;

        //! The owner's body showed or hid: so does the current look.
        void OnShownChanged(bool shown) override;

    protected:
        //! Spawns every look.
        void Activate() override;

        //! Despawns every look.
        void Deactivate() override;

    private:
        //! A spawned look.
        struct Spawned
        {
            AzFramework::EntitySpawnTicket m_ticket; //!< Keeps the look spawned.
            AZStd::shared_ptr<AZStd::vector<AZ::EntityId>> m_entities; //!< Its entities once spawned; empty before.
        };

        //! Spawns a look's prefab under the Thing, deactivated.
        void Spawn(const AZStd::string& look, const AZ::Data::Asset<AzFramework::Spawnable>& prefab);

        //! Activates the entities that should show and deactivates the rest, and sits the current look where it belongs.
        void UpdateBody();

        //! The Thing's owner, or an invalid id while top-level.
        AZ::EntityId GetOwner() const;

        //! Whether the current look should show now.
        bool IsLookShowing() const;

        //! Activates or deactivates an entity, remembering what it is.
        void SetEntityActive(AZ::EntityId entity, bool active);

        AZ::Data::Asset<AzFramework::Spawnable> m_prefab; //!< The default look's prefab.
        AZStd::map<AZStd::string, BodyLook> m_looks; //!< Named looks, by name.
        AZStd::vector<AZStd::string> m_hiddenParts; //!< Entities of the body, by name, that start hidden.

        AZStd::string m_look; //!< The current look's name; empty for the default one.
        bool m_shown = true; //!< Whether the body was asked to show.
        AZStd::unordered_set<AZStd::string> m_hiddenNow; //!< The parts hidden now.
        AZStd::unordered_map<AZStd::string, Spawned> m_spawned; //!< Every look's spawn, by name.
        AZStd::unordered_map<AZ::EntityId, bool> m_active; //!< Whether each body entity is active.
        AZ::EntityId m_following; //!< The owner whose body notifications the Thing listens to.
        AZ::EntityId m_socket; //!< The socket entity the current look sits on, if any.
        bool m_lastShown = false; //!< Whether the body showed at the last update, to tell the owned when it changes.
    };
} // namespace Things
