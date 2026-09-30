#pragma once

#include <AzCore/Component/EntityId.h>
#include <AzCore/Interface/Interface.h>
#include <AzCore/RTTI/RTTI.h>
#include <AzCore/std/containers/unordered_map.h>
#include <AzCore/std/containers/vector.h>
#include <Things/ThingSystemBus.h>

namespace Things
{
    class ThingComponent;

    //! Every active Thing by entity id, and the ownership links between them.
    //! Things register themselves when they activate and unregister when they deactivate.
    class ThingRegistry
    {
    public:
        AZ_RTTI(ThingRegistry, "{FD03E8E0-2739-410F-AF47-3064B88CFF1F}");

        //! Destroys the registry.
        virtual ~ThingRegistry() = default;

        //! Adds an activating Thing and appends it to its owner's owned list.
        //! A Thing whose owner isn't registered becomes top-level, with a warning.
        void Register(ThingComponent& thing);

        //! Removes a deactivating Thing and takes it off its owner's owned list.
        void Unregister(ThingComponent& thing);

        //! The registered Thing, or nullptr.
        ThingComponent* Find(AZ::EntityId thing) const;

        //! Moves a Thing to a new owner, or makes it top-level when newOwner is invalid.
        //! Fails, with a warning, when either isn't a Thing or the new owner is the Thing itself or below it.
        bool Link(AZ::EntityId thing, AZ::EntityId newOwner);

        //! Visits root and everything it owns, depth first in ownership order.
        void Visit(AZ::EntityId root, const TreeVisitor& visitor) const;

        //! The nearest Thing above this one that matches, or an invalid id.
        AZ::EntityId FindAncestor(AZ::EntityId thing, const AZStd::function<bool(AZ::EntityId)>& match) const;

        //! Every Thing whose owner is not a registered Thing, ordered by id.
        AZStd::vector<AZ::EntityId> GetTopLevel() const;

        //! Number of registered Things.
        size_t GetCount() const;

    private:
        //! Visits thing at the given depth; returns false when the visit should stop.
        bool VisitAt(AZ::EntityId thing, AZ::u32 depth, const TreeVisitor& visitor) const;

        //! Takes thing off its owner's owned list and tells the owner and everything above it.
        void Detach(ThingComponent& thing);

        //! Sends OnTreeChanged(changed) to start and every Thing above it.
        void NotifyAncestors(AZ::EntityId start, AZ::EntityId changed) const;

        //! Sends OnAncestryChanged to everything below thing.
        void NotifyDescendants(AZ::EntityId thing) const;

        AZStd::unordered_map<AZ::EntityId, ThingComponent*> m_things; //!< Registered Things by entity id.
    };

    //! The global ThingRegistry, owned by the ThingSystemComponent.
    using ThingRegistryInterface = AZ::Interface<ThingRegistry>;
} // namespace Things
