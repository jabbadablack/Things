#include <Things/ThingComponent.h>

#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/std/algorithm.h>
#include <AzCore/std/sort.h>
#include <Things/ThingRegistry.h>

namespace Things
{
    void ThingComponent::Reflect(AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<ThingComponent, AZ::Component>()
                ->Version(1)
                ->Field("Blueprint", &ThingComponent::m_blueprint)
                ->Field("Key", &ThingComponent::m_key)
                ->Field("Owner", &ThingComponent::m_owner)
                ->Field("Owned", &ThingComponent::m_owned)
                ->Field("Tags", &ThingComponent::m_tags)
                ->Field("Parts", &ThingComponent::m_parts);
        }
    }

    void ThingComponent::GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided)
    {
        provided.push_back(AZ_CRC_CE("ThingService"));
    }

    void ThingComponent::GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible)
    {
        incompatible.push_back(AZ_CRC_CE("ThingService"));
    }

    const AZStd::string& ThingComponent::GetBlueprint() const
    {
        return m_blueprint;
    }

    void ThingComponent::SetBlueprint(AZStd::string blueprint)
    {
        m_blueprint = AZStd::move(blueprint);
    }

    AZ::EntityId ThingComponent::GetOwner() const
    {
        return m_owner;
    }

    void ThingComponent::SetOwner(AZ::EntityId owner)
    {
        AZ_Assert(
            !GetEntity() || GetEntity()->GetState() < AZ::Entity::State::Activating,
            "Use Transfer to change the owner of an active Thing.");
        m_owner = owner;
    }

    const AZStd::string& ThingComponent::GetKey() const
    {
        return m_key;
    }

    void ThingComponent::SetKey(AZStd::string key)
    {
        m_key = AZStd::move(key);
    }

    const AZStd::vector<AZ::EntityId>& ThingComponent::GetOwned() const
    {
        return m_owned;
    }

    bool ThingComponent::HasTag(AZStd::string_view tag) const
    {
        return AZStd::binary_search(
            m_tags.begin(),
            m_tags.end(),
            tag,
            [](AZStd::string_view lhs, AZStd::string_view rhs)
            {
                return lhs < rhs;
            });
    }

    const AZStd::vector<AZStd::string>& ThingComponent::GetTags() const
    {
        return m_tags;
    }

    void ThingComponent::SetTags(AZStd::vector<AZStd::string> tags)
    {
        m_tags = AZStd::move(tags);
        AZStd::sort(m_tags.begin(), m_tags.end());
    }

    void ThingComponent::AddPart(AZStd::string key, AZ::ComponentId component)
    {
        m_parts[AZStd::move(key)] = component;
    }

    const AZStd::unordered_map<AZStd::string, AZ::ComponentId>& ThingComponent::GetParts() const
    {
        return m_parts;
    }

    void ThingComponent::Activate()
    {
        ThingRegistry* registry = ThingRegistryInterface::Get();
        AZ_Assert(registry, "Things need the ThingSystemComponent.");
        if (registry)
        {
            registry->Register(*this);
        }
    }

    void ThingComponent::Deactivate()
    {
        if (ThingRegistry* registry = ThingRegistryInterface::Get())
        {
            registry->Unregister(*this);
        }
    }
} // namespace Things
