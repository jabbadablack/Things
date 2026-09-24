#include <Things/ThingBodyComponent.h>

#include <AzCore/Asset/AssetSerializer.h>
#include <AzCore/Component/Entity.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzFramework/Components/TransformComponent.h>

namespace Things
{
    void ThingBodyComponent::Reflect(AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<ThingBodyComponent, AZ::Component>()->Version(0)->Field("Prefab", &ThingBodyComponent::m_prefab);
        }
    }

    void ThingBodyComponent::GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required)
    {
        required.push_back(AZ_CRC_CE("TransformService"));
    }

    void ThingBodyComponent::GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided)
    {
        provided.push_back(AZ_CRC_CE("ThingBodyService"));
    }

    void ThingBodyComponent::GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible)
    {
        incompatible.push_back(AZ_CRC_CE("ThingBodyService"));
    }

    const AZStd::vector<AZ::EntityId>& ThingBodyComponent::GetBodyEntities() const
    {
        static const AZStd::vector<AZ::EntityId> none;
        return m_bodyEntities ? *m_bodyEntities : none;
    }

    void ThingBodyComponent::Activate()
    {
        if (!m_prefab.GetId().IsValid())
        {
            AZ_Warning("Things", false, "Thing %s has a body part without a prefab; it stays invisible.", GetEntityId().ToString().c_str());
            return;
        }

        auto* spawner = AzFramework::SpawnableEntitiesInterface::Get();
        AZ_Assert(spawner, "ThingBodyComponent needs the spawnable system.");
        if (!spawner)
        {
            return;
        }

        m_bodyEntities = AZStd::make_shared<AZStd::vector<AZ::EntityId>>();
        m_prefab.QueueLoad();
        m_ticket = AzFramework::EntitySpawnTicket(m_prefab);

        const AZ::EntityId thing = GetEntityId();
        AzFramework::SpawnAllEntitiesOptionalArgs args;
        args.m_preInsertionCallback = [thing](AzFramework::EntitySpawnTicket::Id, AzFramework::SpawnableEntityContainerView view)
        {
            if (view.empty())
            {
                return;
            }
            if (auto* transform = (*view.begin())->FindComponent<AzFramework::TransformComponent>())
            {
                AzFramework::TransformComponentConfiguration config;
                config.m_parentId = thing;
                config.m_localTransform = transform->GetLocalTM();
                transform->SetConfiguration(config);
            }
        };
        args.m_completionCallback = [weakBody = AZStd::weak_ptr<AZStd::vector<AZ::EntityId>>(m_bodyEntities)](
                                        AzFramework::EntitySpawnTicket::Id, AzFramework::SpawnableConstEntityContainerView view)
        {
            if (auto body = weakBody.lock())
            {
                body->clear();
                for (const AZ::Entity* entity : view)
                {
                    body->push_back(entity->GetId());
                }
            }
        };
        spawner->SpawnAllEntities(m_ticket, AZStd::move(args));
    }

    void ThingBodyComponent::Deactivate()
    {
        m_ticket = AzFramework::EntitySpawnTicket();
        m_bodyEntities.reset();
    }
} // namespace Things
