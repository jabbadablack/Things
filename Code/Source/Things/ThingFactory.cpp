#include <Things/ThingFactory.h>

#include <AzCore/Component/Entity.h>
#include <AzCore/Component/TransformBus.h>
#include <AzCore/Console/IConsole.h>
#include <AzFramework/Entity/GameEntityContextBus.h>
#include <Blueprints/BlueprintLibrary.h>
#include <Things/ThingBus.h>
#include <Things/ThingComponent.h>
#include <Things/TypedJson.h>

AZ_CVAR(bool, things_log_spawns, false, nullptr, AZ::ConsoleFunctorFlags::Null, "Logs every Thing that is built.");

namespace Things
{
    namespace
    {
        //! The names of the tags that are true.
        AZStd::vector<AZStd::string> ReadTags(const rapidjson::Value& resolved, const AZStd::string& name)
        {
            AZStd::vector<AZStd::string> tags;
            const auto member = resolved.FindMember(BlueprintLibrary::TagsKey);
            if (member == resolved.MemberEnd())
            {
                return tags;
            }
            if (!member->value.IsObject())
            {
                AZ_Warning("Things", false, "Blueprint '%s': \"Tags\" must be an object of tag names to true or false.", name.c_str());
                return tags;
            }

            for (const auto& tag : member->value.GetObject())
            {
                if (!tag.value.IsBool())
                {
                    AZ_Warning("Things", false, "Blueprint '%s': tag '%s' must be true or false.", name.c_str(), tag.name.GetString());
                }
                else if (tag.value.GetBool())
                {
                    tags.emplace_back(tag.name.GetString(), tag.name.GetStringLength());
                }
            }
            return tags;
        }

        //! Creates a component for every part of the blueprint and adds it to the entity.
        void AddParts(AZ::Entity& entity, ThingComponent& thing, const rapidjson::Value& resolved, const AZStd::string& name)
        {
            const auto parts = resolved.FindMember(BlueprintLibrary::PartsKey);
            if (parts == resolved.MemberEnd())
            {
                return;
            }
            if (!parts->value.IsObject())
            {
                AZ_Warning("Things", false, "Blueprint '%s': \"Parts\" must be an object of parts by id.", name.c_str());
                return;
            }

            for (const auto& part : parts->value.GetObject())
            {
                const AZStd::string key(part.name.GetString(), part.name.GetStringLength());
                const AZStd::string context = AZStd::string::format("Blueprint '%s' part '%s'", name.c_str(), key.c_str());

                rapidjson::Document withoutId;
                const rapidjson::Value* data = &part.value;
                if (part.value.IsObject() && part.value.HasMember("Id"))
                {
                    AZ_Warning("Things", false, "%s: \"Id\" is reserved for the component id and is ignored.", context.c_str());
                    withoutId.CopyFrom(part.value, withoutId.GetAllocator());
                    withoutId.EraseMember("Id");
                    data = &withoutId;
                }

                AZ::Component* component = CreateFromTypedJson<AZ::Component>(*data, context);
                if (!component)
                {
                    continue;
                }
                if (azrtti_istypeof<ThingComponent>(component))
                {
                    AZ_Warning("Things", false, "%s: a ThingComponent can't be a part; every Thing has one already.", context.c_str());
                    delete component;
                    continue;
                }
                if (!entity.AddComponent(component))
                {
                    AZ_Warning("Things", false, "%s: the component could not be added.", context.c_str());
                    delete component;
                    continue;
                }
                thing.AddPart(key, component->GetId());
            }
        }

        //! Places a spatial Thing: the first component with a transform interface gets the transform.
        void PlaceTransform(AZ::Entity& entity, const AZ::Transform& transform)
        {
            for (AZ::Component* component : entity.GetComponents())
            {
                if (auto* transformInterface = azrtti_cast<AZ::TransformInterface*>(component))
                {
                    transformInterface->SetWorldTM(transform);
                    return;
                }
            }
        }
    } // namespace

    ThingFactory::ThingFactory(BlueprintLibrary& library)
        : m_library(library)
    {
    }

    AZ::EntityId ThingFactory::Build(
        const rapidjson::Value& resolved, const AZStd::string& name, const AZ::Transform& transform, AZ::EntityId owner)
    {
        AZStd::vector<AZ::EntityId> built;
        const AZ::EntityId thing = BuildAt(resolved, name, transform, owner, 0, built);
        for (const AZ::EntityId& id : built)
        {
            ThingNotificationBus::Event(id, &ThingNotifications::OnThingBuilt);
        }
        return thing;
    }

    AZ::EntityId ThingFactory::BuildAt(
        const rapidjson::Value& resolved,
        const AZStd::string& name,
        const AZ::Transform& transform,
        AZ::EntityId owner,
        AZ::u32 depth,
        AZStd::vector<AZ::EntityId>& built)
    {
        AZ::Entity* entity = nullptr;
        AzFramework::GameEntityContextRequestBus::BroadcastResult(
            entity, &AzFramework::GameEntityContextRequests::CreateGameEntity, name.c_str());
        if (!entity)
        {
            AZ_Error("Things", false, "Thing '%s' can't be built: there is no game entity context.", name.c_str());
            return AZ::EntityId();
        }

        auto* thing = entity->CreateComponent<ThingComponent>();
        thing->SetBlueprint(name);
        thing->SetOwner(owner);
        thing->SetTags(ReadTags(resolved, name));
        AddParts(*entity, *thing, resolved, name);
        PlaceTransform(*entity, transform);

        if (entity->GetState() == AZ::Entity::State::Constructed)
        {
            entity->Init();
        }
        const AZ::Entity::DependencySortOutcome dependencies = entity->EvaluateDependenciesGetDetails();
        if (!dependencies.IsSuccess())
        {
            AZ_Warning(
                "Things",
                false,
                "Thing '%s' can't be built because its parts don't fit together: %s %s",
                name.c_str(),
                dependencies.GetError().m_message.c_str(),
                dependencies.GetError().m_extendedMessage.c_str());
            AzFramework::GameEntityContextRequestBus::Broadcast(
                &AzFramework::GameEntityContextRequests::DestroyGameEntity, entity->GetId());
            return AZ::EntityId();
        }

        const AZ::EntityId id = entity->GetId();
        AzFramework::GameEntityContextRequestBus::Broadcast(&AzFramework::GameEntityContextRequests::ActivateGameEntity, id);
        built.push_back(id);
        if (things_log_spawns)
        {
            AZ_Info("Things", "Built '%s' as %s.\n", name.c_str(), id.ToString().c_str());
        }

        BuildChildren(resolved, name, id, depth, built);
        return id;
    }

    void ThingFactory::BuildChildren(
        const rapidjson::Value& resolved, const AZStd::string& name, AZ::EntityId owner, AZ::u32 depth, AZStd::vector<AZ::EntityId>& built)
    {
        const auto children = resolved.FindMember(BlueprintLibrary::ChildrenKey);
        if (children == resolved.MemberEnd())
        {
            return;
        }
        if (!children->value.IsObject())
        {
            AZ_Warning("Things", false, "Blueprint '%s': \"Children\" must be an object of children by id.", name.c_str());
            return;
        }
        if (depth >= MaxChildDepth && children->value.MemberCount() > 0)
        {
            AZ_Warning(
                "Things",
                false,
                "Thing '%s': children nest deeper than %u; its children are not built. Does a blueprint own itself?",
                name.c_str(),
                MaxChildDepth);
            return;
        }

        for (const auto& child : children->value.GetObject())
        {
            const char* childId = child.name.GetString();
            const auto blueprint =
                child.value.IsObject() ? child.value.FindMember(BlueprintLibrary::BlueprintKey) : child.value.MemberEnd();
            if (!child.value.IsObject() || blueprint == child.value.MemberEnd() || !blueprint->value.IsString())
            {
                AZ_Warning("Things", false, "Blueprint '%s': child '%s' needs a \"Blueprint\" name.", name.c_str(), childId);
                continue;
            }

            const AZStd::string childBlueprint(blueprint->value.GetString(), blueprint->value.GetStringLength());
            const rapidjson::Value* childResolved = m_library.Resolve(childBlueprint);
            if (!childResolved)
            {
                AZ_Warning(
                    "Things",
                    false,
                    "Thing '%s': child '%s' uses blueprint '%s', which does not exist.",
                    name.c_str(),
                    childId,
                    childBlueprint.c_str());
                continue;
            }

            if (child.value.MemberCount() == 1)
            {
                BuildAt(*childResolved, childBlueprint, AZ::Transform::CreateIdentity(), owner, depth + 1, built);
                continue;
            }

            rapidjson::Document overrides;
            overrides.CopyFrom(child.value, overrides.GetAllocator());
            overrides.EraseMember(BlueprintLibrary::BlueprintKey);

            rapidjson::Document layered;
            layered.CopyFrom(*childResolved, layered.GetAllocator());
            BlueprintLibrary::ApplyLayer(layered, layered.GetAllocator(), overrides, childBlueprint);
            BuildAt(layered, childBlueprint, AZ::Transform::CreateIdentity(), owner, depth + 1, built);
        }
    }
} // namespace Things
