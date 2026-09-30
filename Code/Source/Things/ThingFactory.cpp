#include <Things/ThingFactory.h>

#include <AzCore/Component/Entity.h>
#include <AzCore/Component/TransformBus.h>
#include <AzCore/Console/IConsole.h>
#include <AzCore/Serialization/Json/JsonSerialization.h>
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
        AZStd::vector<AZStd::string> ReadTags(const rapidjson::Value& resolved, [[maybe_unused]] const AZStd::string& name)
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

    AZ::EntityId ThingFactory::BuildSnapshot(const rapidjson::Value& snapshot, const AZ::Transform& transform, AZ::EntityId owner)
    {
        AZStd::vector<AZ::EntityId> built;
        const AZ::EntityId thing = BuildSnapshotAt(snapshot, transform, owner, 0, built);
        for (const AZ::EntityId& id : built)
        {
            ThingNotificationBus::Event(id, &ThingNotifications::OnThingBuilt);
        }
        return thing;
    }

    AZ::EntityId ThingFactory::BuildSnapshotAt(
        const rapidjson::Value& snapshot,
        const AZ::Transform& transform,
        AZ::EntityId owner,
        AZ::u32 depth,
        AZStd::vector<AZ::EntityId>& built)
    {
        const auto blueprint = snapshot.IsObject() ? snapshot.FindMember(BlueprintLibrary::BlueprintKey) : snapshot.MemberEnd();
        const auto parts = snapshot.IsObject() ? snapshot.FindMember(BlueprintLibrary::PartsKey) : snapshot.MemberEnd();
        if (!snapshot.IsObject() || parts == snapshot.MemberEnd() || !parts->value.IsObject())
        {
            AZ_Warning("Things", false, "A snapshot of a Thing needs an object of \"Parts\"; nothing is built.");
            return AZ::EntityId();
        }
        if (snapshot.HasMember(BlueprintLibrary::ChildrenKey))
        {
            AZ_Warning("Things", false, "A snapshot's \"Children\" are ignored; what a Thing owned is in \"Owned\".");
        }
        const AZStd::string name = blueprint != snapshot.MemberEnd() && blueprint->value.IsString()
            ? AZStd::string(blueprint->value.GetString(), blueprint->value.GetStringLength())
            : AZStd::string("Snapshot");

        rapidjson::Document own;
        own.SetObject();
        for (const char* key : { BlueprintLibrary::TagsKey, BlueprintLibrary::PartsKey })
        {
            if (const auto member = snapshot.FindMember(key); member != snapshot.MemberEnd())
            {
                own.AddMember(rapidjson::StringRef(key), rapidjson::Value(member->value, own.GetAllocator()), own.GetAllocator());
            }
        }
        const AZ::EntityId thing = BuildAt(own, name, transform, owner, depth, built);
        const auto owned = snapshot.FindMember(OwnedKey);
        if (!thing.IsValid() || owned == snapshot.MemberEnd())
        {
            return thing;
        }
        if (!owned->value.IsArray())
        {
            AZ_Warning("Things", false, "Snapshot of '%s': \"Owned\" must be a list of snapshots.", name.c_str());
            return thing;
        }
        if (depth >= MaxChildDepth && !owned->value.Empty())
        {
            AZ_Warning(
                "Things", false, "Snapshot of '%s': owned Things nest deeper than %u and are not built.", name.c_str(), MaxChildDepth);
            return thing;
        }
        for (const rapidjson::Value& child : owned->value.GetArray())
        {
            BuildSnapshotAt(child, AZ::Transform::CreateIdentity(), thing, depth + 1, built);
        }
        return thing;
    }

    void ThingFactory::SaveOne(const ThingComponent& thing, rapidjson::Value& output, rapidjson::Document::AllocatorType& allocator)
    {
        output.SetObject();
        output.AddMember(
            rapidjson::StringRef(BlueprintLibrary::BlueprintKey), rapidjson::Value(thing.GetBlueprint().c_str(), allocator), allocator);
        rapidjson::Value tags(rapidjson::kObjectType);
        for (const AZStd::string& tag : thing.GetTags())
        {
            tags.AddMember(rapidjson::Value(tag.c_str(), allocator), rapidjson::Value(true), allocator);
        }
        output.AddMember(rapidjson::StringRef(BlueprintLibrary::TagsKey), tags, allocator);

        rapidjson::Value parts(rapidjson::kObjectType);
        const AZ::Entity* entity = thing.GetEntity();
        for (const auto& [key, componentId] : thing.GetParts())
        {
            const AZ::Component* component = entity ? entity->FindComponent(componentId) : nullptr;
            if (!component)
            {
                AZ_Warning("Things", false, "'%s' has lost its part '%s'; it is not saved.", thing.GetBlueprint().c_str(), key.c_str());
                continue;
            }
            const AZ::TypeId type = component->RTTI_GetType();
            rapidjson::Value part;
            const AZ::JsonSerializationResult::ResultCode stored = AZ::JsonSerialization::Store(part, allocator, component, nullptr, type);
            AZ_Warning(
                "Things",
                stored.GetProcessing() != AZ::JsonSerializationResult::Processing::Halted,
                "'%s' part '%s' could not be saved: %s",
                thing.GetBlueprint().c_str(),
                key.c_str(),
                stored.ToString("").c_str());
            if (!part.IsObject())
            {
                part.SetObject();
            }
            part.RemoveMember("Id");
            rapidjson::Value typeName;
            AZ::JsonSerialization::StoreTypeId(typeName, allocator, type);
            part.AddMember(rapidjson::StringRef(BlueprintLibrary::TypeKey), typeName, allocator);
            parts.AddMember(rapidjson::Value(key.c_str(), allocator), part, allocator);
        }
        output.AddMember(rapidjson::StringRef(BlueprintLibrary::PartsKey), parts, allocator);
    }

    void ThingFactory::BuildChildren(
        const rapidjson::Value& resolved, [[maybe_unused]] const AZStd::string& name, AZ::EntityId owner, AZ::u32 depth, AZStd::vector<AZ::EntityId>& built)
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
            [[maybe_unused]] const char* childId = child.name.GetString();
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
