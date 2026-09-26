#include <Things/ThingBodyComponent.h>

#include <AzCore/Asset/AssetSerializer.h>
#include <AzCore/Component/ComponentApplicationBus.h>
#include <AzCore/Component/Entity.h>
#include <AzCore/Component/TransformBus.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzFramework/Components/TransformComponent.h>
#include <AzFramework/Entity/GameEntityContextBus.h>
#include <Things/ThingSystemBus.h>

namespace Things
{
    namespace
    {
        //! The entity of an id, or nullptr.
        AZ::Entity* FindBodyPart(AZ::EntityId id)
        {
            AZ::Entity* entity = nullptr;
            AZ::ComponentApplicationBus::BroadcastResult(entity, &AZ::ComponentApplicationRequests::FindEntity, id);
            return entity;
        }

        //! Makes a body's root entity a child of another entity, keeping its place relative to its parent, whether or
        //! not it is active.
        void ParentBodyRoot(AZ::EntityId root, AZ::EntityId parent)
        {
            AZ::Entity* entity = FindBodyPart(root);
            if (!entity)
            {
                return;
            }
            if (entity->GetState() == AZ::Entity::State::Active)
            {
                AZ::TransformBus::Event(root, &AZ::TransformBus::Events::SetParentRelative, parent);
            }
            else if (auto* transform = entity->FindComponent<AzFramework::TransformComponent>())
            {
                AzFramework::TransformComponentConfiguration config;
                config.m_parentId = parent;
                config.m_localTransform = transform->GetLocalTM();
                transform->SetConfiguration(config);
            }
        }
    } // namespace

    void BodyLook::Reflect(AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<BodyLook>()->Version(0)->Field("Prefab", &BodyLook::m_prefab)->Field("Socket", &BodyLook::m_socket);
        }
    }

    void ThingBodyComponent::Reflect(AZ::ReflectContext* context)
    {
        BodyLook::Reflect(context);
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<ThingBodyComponent, AZ::Component>()
                ->Version(1)
                ->Field("Prefab", &ThingBodyComponent::m_prefab)
                ->Field("Looks", &ThingBodyComponent::m_looks)
                ->Field("HiddenParts", &ThingBodyComponent::m_hiddenParts);
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

    AZStd::vector<AZ::EntityId> ThingBodyComponent::GetBodyEntities() const
    {
        const auto current = m_spawned.find(m_look);
        return current != m_spawned.end() && current->second.m_entities ? *current->second.m_entities : AZStd::vector<AZ::EntityId>();
    }

    bool ThingBodyComponent::IsInWorld() const
    {
        return !m_look.empty() || !GetOwner().IsValid();
    }

    bool ThingBodyComponent::IsSpawning() const
    {
        for (const auto& [look, spawned] : m_spawned)
        {
            if (spawned.m_entities && spawned.m_entities->empty())
            {
                return true;
            }
        }
        return false;
    }

    void ThingBodyComponent::SetLook(const AZStd::string& look)
    {
        AZ_Warning(
            "Things", look.empty() || m_looks.contains(look), "Thing %s has no look '%s'.", GetEntityId().ToString().c_str(), look.c_str());
        if (look == m_look || (!look.empty() && !m_looks.contains(look)))
        {
            return;
        }
        m_look = look;
        m_socket = AZ::EntityId();
        UpdateBody();
    }

    const AZStd::string& ThingBodyComponent::GetLook() const
    {
        return m_look;
    }

    void ThingBodyComponent::SetShown(bool shown)
    {
        if (shown != m_shown)
        {
            m_shown = shown;
            UpdateBody();
        }
    }

    bool ThingBodyComponent::IsShown() const
    {
        return IsLookShowing();
    }

    void ThingBodyComponent::SetPartShown(const AZStd::string& part, bool shown)
    {
        if (shown)
        {
            m_hiddenNow.erase(part);
        }
        else
        {
            m_hiddenNow.insert(part);
        }
        UpdateBody();
    }

    AZ::EntityId ThingBodyComponent::FindBodyEntity(const AZStd::string& name) const
    {
        for (const AZ::EntityId& id : GetBodyEntities())
        {
            if (const AZ::Entity* entity = FindBodyPart(id); entity && entity->GetName() == name)
            {
                return id;
            }
        }
        return AZ::EntityId();
    }

    void ThingBodyComponent::OnOwnerChanged([[maybe_unused]] AZ::EntityId oldOwner, AZ::EntityId newOwner)
    {
        ThingBodyNotificationBus::Handler::BusDisconnect();
        m_following = newOwner;
        if (m_following.IsValid())
        {
            ThingBodyNotificationBus::Handler::BusConnect(m_following);
        }
        m_socket = AZ::EntityId();
        UpdateBody();
    }

    void ThingBodyComponent::OnBodySpawned()
    {
        m_socket = AZ::EntityId();
        UpdateBody();
    }

    void ThingBodyComponent::OnShownChanged([[maybe_unused]] bool shown)
    {
        UpdateBody();
    }

    void ThingBodyComponent::Activate()
    {
        m_look.clear();
        m_shown = true;
        m_lastShown = false;
        m_hiddenNow = AZStd::unordered_set<AZStd::string>(m_hiddenParts.begin(), m_hiddenParts.end());
        m_socket = AZ::EntityId();
        ThingBodyRequestBus::Handler::BusConnect(GetEntityId());
        ThingNotificationBus::Handler::BusConnect(GetEntityId());
        m_following = GetOwner();
        if (m_following.IsValid())
        {
            ThingBodyNotificationBus::Handler::BusConnect(m_following);
        }
        AZ_Warning(
            "Things",
            m_prefab.GetId().IsValid() || !m_looks.empty(),
            "Thing %s has a body part without a prefab; it stays invisible.",
            GetEntityId().ToString().c_str());
        if (m_prefab.GetId().IsValid())
        {
            Spawn("", m_prefab);
        }
        for (const auto& [name, look] : m_looks)
        {
            AZ_Warning(
                "Things",
                look.m_prefab.GetId().IsValid(),
                "Look '%s' of Thing %s has no prefab.",
                name.c_str(),
                GetEntityId().ToString().c_str());
            if (look.m_prefab.GetId().IsValid())
            {
                Spawn(name, look.m_prefab);
            }
        }
    }

    void ThingBodyComponent::Deactivate()
    {
        ThingBodyNotificationBus::Handler::BusDisconnect();
        ThingNotificationBus::Handler::BusDisconnect();
        ThingBodyRequestBus::Handler::BusDisconnect();
        m_spawned.clear();
        m_active.clear();
        m_following = AZ::EntityId();
        m_socket = AZ::EntityId();
    }

    void ThingBodyComponent::Spawn(const AZStd::string& look, const AZ::Data::Asset<AzFramework::Spawnable>& prefab)
    {
        auto* spawner = AzFramework::SpawnableEntitiesInterface::Get();
        if (!spawner)
        {
            static bool warned = false;
            AZ_Warning("Things", warned, "There is no spawnable system, so Thing bodies stay invisible (expected in unit tests).");
            warned = true;
            return;
        }

        AZ::Data::Asset<AzFramework::Spawnable> asset = prefab;
        asset.QueueLoad();
        Spawned& spawned = m_spawned[look];
        spawned.m_entities = AZStd::make_shared<AZStd::vector<AZ::EntityId>>();
        spawned.m_ticket = AzFramework::EntitySpawnTicket(asset);

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
        args.m_completionCallback = [this, thing, weakBody = AZStd::weak_ptr<AZStd::vector<AZ::EntityId>>(spawned.m_entities)](
                                        AzFramework::EntitySpawnTicket::Id, AzFramework::SpawnableConstEntityContainerView view)
        {
            const AZStd::shared_ptr<AZStd::vector<AZ::EntityId>> body = weakBody.lock();
            if (!body)
            {
                return;
            }
            body->clear();
            for (const AZ::Entity* entity : view)
            {
                body->push_back(entity->GetId());
                m_active[entity->GetId()] = true;
            }
            UpdateBody();
            ThingBodyNotificationBus::Event(thing, &ThingBodyNotifications::OnBodySpawned);
        };
        spawner->SpawnAllEntities(spawned.m_ticket, AZStd::move(args));
    }

    AZ::EntityId ThingBodyComponent::GetOwner() const
    {
        const ThingSystemRequests* things = ThingSystemInterface::Get();
        return things ? things->GetOwner(GetEntityId()) : AZ::EntityId();
    }

    bool ThingBodyComponent::IsLookShowing() const
    {
        if (!m_shown)
        {
            return false;
        }
        const AZ::EntityId owner = GetOwner();
        if (!owner.IsValid())
        {
            return true;
        }
        if (m_look.empty())
        {
            return false;
        }
        bool ownerShown = true;
        ThingBodyRequestBus::EventResult(ownerShown, owner, &ThingBodyRequests::IsShown);
        return ownerShown;
    }

    void ThingBodyComponent::SetEntityActive(AZ::EntityId entity, bool active)
    {
        bool& current = m_active[entity];
        if (current == active)
        {
            return;
        }
        current = active;
        if (active)
        {
            AzFramework::GameEntityContextRequestBus::Broadcast(
                &AzFramework::GameEntityContextRequestBus::Events::ActivateGameEntity, entity);
        }
        else
        {
            AzFramework::GameEntityContextRequestBus::Broadcast(
                &AzFramework::GameEntityContextRequestBus::Events::DeactivateGameEntity, entity);
        }
    }

    void ThingBodyComponent::UpdateBody()
    {
        bool showing = IsLookShowing();
        const AZStd::vector<AZ::EntityId> current = GetBodyEntities();
        if (!current.empty())
        {
            AZ::EntityId parent = GetEntityId();
            const AZ::EntityId owner = GetOwner();
            if (owner.IsValid() && !m_look.empty())
            {
                const AZStd::string& socketName = m_looks[m_look].m_socket;
                AZ::EntityId socket;
                if (!socketName.empty())
                {
                    ThingBodyRequestBus::EventResult(socket, owner, &ThingBodyRequests::FindBodyEntity, socketName);
                }
                showing = showing && socket.IsValid();
                parent = socket.IsValid() ? socket : GetEntityId();
            }
            if (parent != m_socket)
            {
                ParentBodyRoot(current.front(), parent);
                m_socket = parent;
            }
        }
        for (const auto& [look, spawned] : m_spawned)
        {
            if (!spawned.m_entities)
            {
                continue;
            }
            for (const AZ::EntityId& entity : *spawned.m_entities)
            {
                const AZ::Entity* part = FindBodyPart(entity);
                const bool hidden = part && m_hiddenNow.contains(part->GetName());
                SetEntityActive(entity, showing && look == m_look && !hidden);
            }
        }
        if (IsShown() != m_lastShown)
        {
            m_lastShown = IsShown();
            ThingBodyNotificationBus::Event(GetEntityId(), &ThingBodyNotifications::OnShownChanged, m_lastShown);
        }
    }
} // namespace Things
