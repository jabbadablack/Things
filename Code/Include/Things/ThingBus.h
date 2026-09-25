#pragma once

#include <AzCore/Component/ComponentBus.h>
#include <AzCore/RTTI/RTTI.h>
#include <AzCore/std/containers/vector.h>
#include <Things/ThingsTypeIds.h>

namespace Things
{
    //! Events about one Thing, addressed by its entity id. Any part of the Thing can listen.
    class ThingNotifications : public AZ::ComponentBus
    {
    public:
        AZ_RTTI(ThingNotifications, ThingNotificationsTypeId);

        //! Destroys the handler.
        virtual ~ThingNotifications() = default;

        //! The Thing and everything its blueprint gives it have been built and activated.
        virtual void OnThingBuilt()
        {
        }

        //! The Thing got a new owner. Either id is invalid when the Thing is or was top-level.
        virtual void OnOwnerChanged([[maybe_unused]] AZ::EntityId oldOwner, [[maybe_unused]] AZ::EntityId newOwner)
        {
        }

        //! The Thing now owns another Thing.
        virtual void OnOwnedAdded([[maybe_unused]] AZ::EntityId owned)
        {
        }

        //! The Thing no longer owns another Thing.
        virtual void OnOwnedRemoved([[maybe_unused]] AZ::EntityId owned)
        {
        }

        //! The Thing is about to be destroyed; everything it owns is destroyed with it.
        virtual void OnThingDestroying()
        {
        }
    };

    //! Bus for ThingNotifications.
    using ThingNotificationBus = AZ::EBus<ThingNotifications>;

    //! A Thing's visible body, addressed by the Thing.
    class ThingBodyRequests : public AZ::ComponentBus
    {
    public:
        //! Destroys the handler.
        virtual ~ThingBodyRequests() = default;

        //! One body per Thing.
        static constexpr AZ::EBusHandlerPolicy HandlerPolicy = AZ::EBusHandlerPolicy::Single;

        //! The entities of the spawned body; empty until it has spawned and while it is out of the world.
        virtual AZStd::vector<AZ::EntityId> GetBodyEntities() const = 0;

        //! Whether the body is in the world: while the Thing is top-level, or always when it shows while owned.
        virtual bool IsInWorld() const = 0;
    };

    //! Bus for ThingBodyRequests.
    using ThingBodyRequestBus = AZ::EBus<ThingBodyRequests>;
} // namespace Things
