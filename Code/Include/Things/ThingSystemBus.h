#pragma once

#include <AzCore/Component/EntityId.h>
#include <AzCore/EBus/EBus.h>
#include <AzCore/Interface/Interface.h>
#include <AzCore/JSON/document.h>
#include <AzCore/Math/Transform.h>
#include <AzCore/RTTI/RTTI.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/functional.h>
#include <AzCore/std/string/string.h>
#include <Things/ThingsTypeIds.h>

namespace Things
{
    //! What a tree visit does after visiting a Thing.
    enum class VisitAction : AZ::u8
    {
        Continue,     //!< Visit the Things this one owns, then carry on.
        SkipChildren, //!< Don't visit the Things this one owns.
        Stop,         //!< End the whole visit.
    };

    //! Called for each Thing of an owned tree with its depth below the root (the root is 0).
    using TreeVisitor = AZStd::function<VisitAction(AZ::EntityId thing, AZ::u32 depth)>;

    //! Builds Things from blueprints, tracks who owns what, and serves the blueprints.
    //!
    //! A Thing is an entity with a ThingComponent. Its parts are ordinary components built from its blueprint.
    //! Things can own other Things (skills, items, body parts), which form an ordered tree.
    class ThingSystemRequests
    {
    public:
        AZ_RTTI(ThingSystemRequests, ThingSystemRequestsTypeId);

        //! Destroys the interface.
        virtual ~ThingSystemRequests() = default;

        //! Builds a Thing and everything it owns from a blueprint. Returns an invalid id, with a warning, on failure.
        virtual AZ::EntityId Spawn(const AZStd::string& blueprint, const AZ::Transform& transform) = 0;

        //! Builds a Thing from blueprints layered in order (a recipe such as {"Human", "Warrior"}).
        virtual AZ::EntityId SpawnComposed(const AZStd::vector<AZStd::string>& layers, const AZ::Transform& transform) = 0;

        //! Builds a Thing that the owner owns, such as a learned skill or a picked-up item.
        virtual AZ::EntityId SpawnOwned(AZ::EntityId owner, const AZStd::string& blueprint) = 0;

        //! Destroys a Thing and everything it owns. The entities are deactivated now and deleted on the next tick.
        virtual void Destroy(AZ::EntityId thing) = 0;

        //! Moves a Thing to a new owner, or makes it top-level when newOwner is invalid.
        //! Fails, with a warning, when the new owner is the Thing itself or something it owns.
        virtual bool Transfer(AZ::EntityId thing, AZ::EntityId newOwner) = 0;

        //! Whether the entity is an active Thing.
        virtual bool IsThing(AZ::EntityId entity) const = 0;

        //! The Thing that owns this one, or an invalid id for a top-level Thing.
        virtual AZ::EntityId GetOwner(AZ::EntityId thing) const = 0;

        //! The Things this one owns directly, in order.
        virtual AZStd::vector<AZ::EntityId> GetOwned(AZ::EntityId thing) const = 0;

        //! The blueprint the Thing was built from, or the layers joined with '+' for composed Things.
        virtual AZStd::string GetBlueprint(AZ::EntityId thing) const = 0;

        //! Whether the Thing's blueprint gave it the tag.
        virtual bool HasTag(AZ::EntityId thing, const AZStd::string& tag) const = 0;

        //! The nearest Thing above this one (not itself) that matches, or an invalid id.
        virtual AZ::EntityId FindAncestor(AZ::EntityId thing, const AZStd::function<bool(AZ::EntityId)>& match) const = 0;

        //! Visits root and the Things it owns, depth first in ownership order.
        virtual void VisitTree(AZ::EntityId root, const TreeVisitor& visitor) const = 0;

        //! Every Thing that no other Thing owns.
        virtual AZStd::vector<AZ::EntityId> GetTopLevelThings() const = 0;

        //! Whether a blueprint with this name exists.
        virtual bool HasBlueprint(const AZStd::string& name) = 0;

        //! The fully resolved blueprint, or nullptr. Valid until the blueprints change.
        virtual const rapidjson::Value* GetResolvedBlueprint(AZStd::string_view name) = 0;

        //! Names of all blueprints, sorted.
        virtual AZStd::vector<AZStd::string> GetBlueprintNames() = 0;

        //! The files that contributed layers to a blueprint, in load order, as "<root>:<path>".
        virtual AZStd::vector<AZStd::string> GetBlueprintSources(const AZStd::string& name) = 0;

        //! Adds blueprint definitions from JSON text on top of the loaded ones, as if from a file named source.
        //! Returns false, with a warning, when the text isn't a JSON object.
        virtual bool AddBlueprintLayers(AZStd::string_view json, AZStd::string_view source) = 0;

        //! Reads every blueprint file of the game and its mods again.
        virtual void ReloadData() = 0;
    };

    //! Traits of the ThingSystemRequestBus: one global handler.
    class ThingSystemBusTraits : public AZ::EBusTraits
    {
    public:
        static constexpr AZ::EBusHandlerPolicy HandlerPolicy = AZ::EBusHandlerPolicy::Single;   //!< One handler.
        static constexpr AZ::EBusAddressPolicy AddressPolicy = AZ::EBusAddressPolicy::Single;   //!< One address.
    };

    //! Bus for ThingSystemRequests, used by scripts.
    using ThingSystemRequestBus = AZ::EBus<ThingSystemRequests, ThingSystemBusTraits>;

    //! The global ThingSystemRequests, used by C++.
    using ThingSystemInterface = AZ::Interface<ThingSystemRequests>;

    //! Calls fn for root and every Thing it owns, depth first. A Thing below the root for which include returns false
    //! is skipped together with everything it owns, e.g. to keep a query from reaching into a carried creature.
    template<class Fn>
    void ForEachInTree(AZ::EntityId root, Fn&& fn, const AZStd::function<bool(AZ::EntityId)>& include = {})
    {
        const ThingSystemRequests* things = ThingSystemInterface::Get();
        AZ_Assert(things, "ForEachInTree needs the ThingSystemComponent.");
        if (!things)
        {
            return;
        }

        things->VisitTree(
            root,
            [&fn, &include](AZ::EntityId thing, AZ::u32 depth)
            {
                if (depth > 0 && include && !include(thing))
                {
                    return VisitAction::SkipChildren;
                }
                fn(thing);
                return VisitAction::Continue;
            });
    }
} // namespace Things
