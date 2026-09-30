#pragma once

#include <AzCore/Component/ComponentBus.h>
#include <AzCore/RTTI/RTTI.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/string/string.h>
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

        //! A Thing somewhere below this one (at any depth) was added, taken away or moved, e.g. an item put into a bag
        //! held in a hand; changed is that Thing. The direct owner hears OnOwnedAdded or OnOwnedRemoved as well.
        virtual void OnTreeChanged([[maybe_unused]] AZ::EntityId changed)
        {
        }

        //! A Thing above this one got a new owner, so this Thing's ancestors changed, e.g. a torch in a severed hand.
        virtual void OnAncestryChanged()
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

        //! The entities of the body's current look; empty until it has spawned.
        virtual AZStd::vector<AZ::EntityId> GetBodyEntities() const = 0;

        //! Whether the current look belongs in the world: the default look while the Thing is top-level, a named look
        //! always (on its owner's socket while owned). Whether it shows also depends on SetShown.
        virtual bool IsInWorld() const = 0;

        //! Whether a look's prefab was asked to spawn and hasn't finished, e.g. for a loading screen to wait on.
        virtual bool IsSpawning() const = 0;

        //! Switches to a named look (e.g. "Held" for a torch in hand), or back to the default one with an empty name.
        virtual void SetLook(const AZStd::string& look) = 0;

        //! The current look's name; empty for the default one.
        virtual const AZStd::string& GetLook() const = 0;

        //! Shows or hides the body, e.g. while it is out of sight; hidden bodies' entities are deactivated, and so are
        //! the looks of everything the Thing owns.
        virtual void SetShown(bool shown) = 0;

        //! Whether the body shows: set shown, in the world, and, while owned, its owner's body shows too.
        virtual bool IsShown() const = 0;

        //! Shows or hides the entities of a part of the body by entity name (e.g. flames on a creature), in every look.
        virtual void SetPartShown(const AZStd::string& part, bool shown) = 0;

        //! The entity of the current look with a name (e.g. a hand socket), or an invalid id.
        virtual AZ::EntityId FindBodyEntity(const AZStd::string& name) const = 0;
    };

    //! Bus for ThingBodyRequests.
    using ThingBodyRequestBus = AZ::EBus<ThingBodyRequests>;

    //! Events about a Thing's body, addressed by the Thing, e.g. for the looks of what it owns to follow it.
    class ThingBodyNotifications : public AZ::ComponentBus
    {
    public:
        //! Destroys the handler.
        virtual ~ThingBodyNotifications() = default;

        //! A look of the body finished spawning.
        virtual void OnBodySpawned()
        {
        }

        //! The body started or stopped showing.
        virtual void OnShownChanged([[maybe_unused]] bool shown)
        {
        }
    };

    //! Bus for ThingBodyNotifications.
    using ThingBodyNotificationBus = AZ::EBus<ThingBodyNotifications>;
} // namespace Things
