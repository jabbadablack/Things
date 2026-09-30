#include <Things/ThingRegistry.h>

#include <AzCore/std/algorithm.h>
#include <AzCore/std/sort.h>
#include <Things/ThingBus.h>
#include <Things/ThingComponent.h>

namespace Things
{
    namespace
    {
        //! How many owners up an ancestor search goes before assuming the ownership links are corrupt.
        constexpr AZ::u32 MaxOwnershipDepth = 1024;
    } // namespace

    void ThingRegistry::Register(ThingComponent& thing)
    {
        const AZ::EntityId id = thing.GetEntityId();
        AZ_Assert(!Find(id), "Thing %s registered twice.", id.ToString().c_str());
        m_things[id] = &thing;

        if (!thing.m_owner.IsValid())
        {
            return;
        }

        ThingComponent* owner = Find(thing.m_owner);
        if (!owner)
        {
            AZ_Warning(
                "Things",
                false,
                "Thing '%s' is owned by %s, which is not a Thing; it becomes top-level.",
                thing.m_blueprint.c_str(),
                thing.m_owner.ToString().c_str());
            thing.m_owner.SetInvalid();
            return;
        }

        if (AZStd::find(owner->m_owned.begin(), owner->m_owned.end(), id) == owner->m_owned.end())
        {
            owner->m_owned.push_back(id);
        }
        ThingNotificationBus::Event(thing.m_owner, &ThingNotifications::OnOwnedAdded, id);
        NotifyAncestors(thing.m_owner, id);
    }

    void ThingRegistry::Unregister(ThingComponent& thing)
    {
        Detach(thing);
        m_things.erase(thing.GetEntityId());
    }

    ThingComponent* ThingRegistry::Find(AZ::EntityId thing) const
    {
        const auto found = m_things.find(thing);
        return found != m_things.end() ? found->second : nullptr;
    }

    bool ThingRegistry::Link(AZ::EntityId thing, AZ::EntityId newOwner)
    {
        ThingComponent* component = Find(thing);
        if (!component)
        {
            AZ_Warning("Things", false, "%s is not a Thing and can't change owner.", thing.ToString().c_str());
            return false;
        }

        ThingComponent* ownerComponent = nullptr;
        if (newOwner.IsValid())
        {
            ownerComponent = Find(newOwner);
            if (!ownerComponent)
            {
                AZ_Warning(
                    "Things",
                    false,
                    "'%s' can't be given to %s, which is not a Thing.",
                    component->m_blueprint.c_str(),
                    newOwner.ToString().c_str());
                return false;
            }

            if (newOwner == thing ||
                FindAncestor(
                    newOwner,
                    [thing](AZ::EntityId ancestor)
                    {
                        return ancestor == thing;
                    })
                    .IsValid())
            {
                AZ_Warning(
                    "Things",
                    false,
                    "'%s' can't be given to '%s', which it owns.",
                    component->m_blueprint.c_str(),
                    ownerComponent->m_blueprint.c_str());
                return false;
            }
        }

        const AZ::EntityId oldOwner = component->m_owner;
        if (oldOwner == newOwner)
        {
            return true;
        }

        Detach(*component);
        component->m_owner = newOwner;
        if (ownerComponent)
        {
            ownerComponent->m_owned.push_back(thing);
            ThingNotificationBus::Event(newOwner, &ThingNotifications::OnOwnedAdded, thing);
            NotifyAncestors(newOwner, thing);
        }
        ThingNotificationBus::Event(thing, &ThingNotifications::OnOwnerChanged, oldOwner, newOwner);
        NotifyDescendants(thing);
        return true;
    }

    void ThingRegistry::Visit(AZ::EntityId root, const TreeVisitor& visitor) const
    {
        VisitAt(root, 0, visitor);
    }

    AZ::EntityId ThingRegistry::FindAncestor(AZ::EntityId thing, const AZStd::function<bool(AZ::EntityId)>& match) const
    {
        const ThingComponent* current = Find(thing);
        for (AZ::u32 depth = 0; current && current->m_owner.IsValid(); ++depth)
        {
            if (depth >= MaxOwnershipDepth)
            {
                AZ_Assert(false, "Thing %s has an ownership loop.", thing.ToString().c_str());
                break;
            }
            if (match(current->m_owner))
            {
                return current->m_owner;
            }
            current = Find(current->m_owner);
        }
        return AZ::EntityId();
    }

    AZStd::vector<AZ::EntityId> ThingRegistry::GetTopLevel() const
    {
        AZStd::vector<AZ::EntityId> topLevel;
        for (const auto& [id, thing] : m_things)
        {
            if (!Find(thing->m_owner))
            {
                topLevel.push_back(id);
            }
        }
        AZStd::sort(topLevel.begin(), topLevel.end());
        return topLevel;
    }

    size_t ThingRegistry::GetCount() const
    {
        return m_things.size();
    }

    bool ThingRegistry::VisitAt(AZ::EntityId thing, AZ::u32 depth, const TreeVisitor& visitor) const
    {
        const ThingComponent* component = Find(thing);
        if (!component)
        {
            return true;
        }

        switch (visitor(thing, depth))
        {
        case VisitAction::Stop:
            return false;
        case VisitAction::SkipChildren:
            return true;
        case VisitAction::Continue:
            break;
        }

        for (size_t i = 0; i < component->m_owned.size(); ++i)
        {
            if (!VisitAt(component->m_owned[i], depth + 1, visitor))
            {
                return false;
            }
        }
        return true;
    }

    void ThingRegistry::Detach(ThingComponent& thing)
    {
        ThingComponent* owner = Find(thing.m_owner);
        if (!owner)
        {
            return;
        }

        const AZ::EntityId id = thing.GetEntityId();
        const auto owned = AZStd::find(owner->m_owned.begin(), owner->m_owned.end(), id);
        if (owned != owner->m_owned.end())
        {
            owner->m_owned.erase(owned);
            ThingNotificationBus::Event(thing.m_owner, &ThingNotifications::OnOwnedRemoved, id);
            NotifyAncestors(thing.m_owner, id);
        }
    }

    void ThingRegistry::NotifyAncestors(AZ::EntityId start, AZ::EntityId changed) const
    {
        const ThingComponent* current = Find(start);
        for (AZ::u32 depth = 0; current && depth < MaxOwnershipDepth; ++depth)
        {
            const AZ::EntityId id = current->GetEntityId();
            ThingNotificationBus::Event(id, &ThingNotifications::OnTreeChanged, changed);
            current = Find(current->m_owner);
        }
    }

    void ThingRegistry::NotifyDescendants(AZ::EntityId thing) const
    {
        Visit(
            thing,
            [thing](AZ::EntityId below, AZ::u32)
            {
                if (below != thing)
                {
                    ThingNotificationBus::Event(below, &ThingNotifications::OnAncestryChanged);
                }
                return VisitAction::Continue;
            });
    }
} // namespace Things
