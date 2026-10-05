#include <Things/ThingSystemComponent.h>

#include <AzCore/Console/IConsole.h>
#include <AzCore/JSON/prettywriter.h>
#include <AzCore/JSON/stringbuffer.h>
#include <AzCore/RTTI/BehaviorContext.h>
#include <AzCore/Serialization/Json/JsonUtils.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/Settings/SettingsRegistry.h>
#include <AzCore/std/string/conversions.h>
#include <AzFramework/Entity/GameEntityContextBus.h>
#include <Things/ThingBus.h>
#include <Things/ThingComponent.h>

namespace Things
{
    namespace
    {
        //! Settings Registry path of the folder below each data root that holds the blueprint files.
        constexpr AZStd::string_view BlueprintFolderRegistryPath = "/Things/BlueprintFolder";

        //! The blueprint folder used when the Settings Registry names none.
        constexpr const char* DefaultBlueprintFolder = "blueprints";

        //! Forwards ThingNotifications to Lua and Script Canvas.
        class BehaviorThingNotificationHandler
            : public ThingNotificationBus::Handler
            , public AZ::BehaviorEBusHandler
        {
        public:
            AZ_EBUS_BEHAVIOR_BINDER(
                BehaviorThingNotificationHandler,
                "{3BED8D4B-91EB-4457-97A6-D2792A01E215}",
                AZ::SystemAllocator,
                OnThingBuilt,
                OnOwnerChanged,
                OnOwnedAdded,
                OnOwnedRemoved,
                OnThingDestroying);

            //! Forwards OnThingBuilt.
            void OnThingBuilt() override
            {
                Call(FN_OnThingBuilt);
            }

            //! Forwards OnOwnerChanged.
            void OnOwnerChanged(AZ::EntityId oldOwner, AZ::EntityId newOwner) override
            {
                Call(FN_OnOwnerChanged, oldOwner, newOwner);
            }

            //! Forwards OnOwnedAdded.
            void OnOwnedAdded(AZ::EntityId owned) override
            {
                Call(FN_OnOwnedAdded, owned);
            }

            //! Forwards OnOwnedRemoved.
            void OnOwnedRemoved(AZ::EntityId owned) override
            {
                Call(FN_OnOwnedRemoved, owned);
            }

            //! Forwards OnThingDestroying.
            void OnThingDestroying() override
            {
                Call(FN_OnThingDestroying);
            }
        };

        //! Parses an entity id written as a number, with or without the brackets of EntityId::ToString.
        AZ::EntityId ParseEntityId(AZStd::string_view text)
        {
            AZStd::string digits;
            for (const char c : text)
            {
                if (c >= '0' && c <= '9')
                {
                    digits.push_back(c);
                }
            }
            return digits.empty() ? AZ::EntityId() : AZ::EntityId(AZStd::stoull(digits));
        }

        //! Pretty-printed JSON.
        AZStd::string ToPrettyJson(const rapidjson::Value& value)
        {
            rapidjson::StringBuffer buffer;
            rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
            value.Accept(writer);
            return AZStd::string(buffer.GetString(), buffer.GetSize());
        }

        //! Prints a Thing and everything it owns, indented by depth.
        void PrintTree(const ThingSystemRequests& things, AZ::EntityId root)
        {
            things.VisitTree(
                root,
                [&]([[maybe_unused]] AZ::EntityId thing, [[maybe_unused]] AZ::u32 depth)
                {
                    AZ_Info(
                        "Things",
                        "%*s%s %s\n",
                        aznumeric_cast<int>(depth * 2),
                        "",
                        thing.ToString().c_str(),
                        things.GetBlueprint(thing).c_str());
                    return VisitAction::Continue;
                });
        }

        //! Console: things_reload. Reads every blueprint file of the game and its mods again.
        void things_reload([[maybe_unused]] const AZ::ConsoleCommandContainer& arguments)
        {
            if (ThingSystemRequests* things = ThingSystemInterface::Get())
            {
                things->ReloadData();
            }
        }

        //! Console: things_list. Prints every top-level Thing and what it owns.
        void things_list([[maybe_unused]] const AZ::ConsoleCommandContainer& arguments)
        {
            if (const ThingSystemRequests* things = ThingSystemInterface::Get())
            {
                const AZStd::vector<AZ::EntityId> topLevel = things->GetTopLevelThings();
                AZ_Info("Things", "%zu top-level Things.\n", topLevel.size());
                for (const AZ::EntityId& thing : topLevel)
                {
                    PrintTree(*things, thing);
                }
            }
        }

        //! Console: things_blueprints. Prints the names of all blueprints.
        void things_blueprints([[maybe_unused]] const AZ::ConsoleCommandContainer& arguments)
        {
            if (ThingSystemRequests* things = ThingSystemInterface::Get())
            {
                const AZStd::vector<AZStd::string> names = things->GetBlueprintNames();
                AZ_Info("Things", "%zu blueprints.\n", names.size());
                for ([[maybe_unused]] const AZStd::string& name : names)
                {
                    AZ_Info("Things", "  %s\n", name.c_str());
                }
            }
        }

        //! Console: things_dump <blueprint or entity id>. Prints a resolved blueprint and its files, or a Thing's tree.
        void things_dump(const AZ::ConsoleCommandContainer& arguments)
        {
            ThingSystemRequests* things = ThingSystemInterface::Get();
            if (!things || arguments.empty())
            {
                AZ_Info("Things", "Usage: things_dump <blueprint or entity id>\n");
                return;
            }

            const AZStd::string argument(arguments.front());
            const AZ::EntityId entity = ParseEntityId(argument);
            if (entity.IsValid() && things->IsThing(entity))
            {
                PrintTree(*things, entity);
                return;
            }

            const rapidjson::Value* resolved = things->GetResolvedBlueprint(argument);
            if (!resolved)
            {
                AZ_Info("Things", "No blueprint or Thing is called '%s'.\n", argument.c_str());
                return;
            }

            for ([[maybe_unused]] const AZStd::string& source : things->GetBlueprintSources(argument))
            {
                AZ_Info("Things", "From %s\n", source.c_str());
            }
            AZ_Info("Things", "%s\n", ToPrettyJson(*resolved).c_str());
        }

        AZ_CONSOLEFREEFUNC(things_reload, AZ::ConsoleFunctorFlags::Null, "Reads every blueprint file of the game and its mods again.");
        AZ_CONSOLEFREEFUNC(things_list, AZ::ConsoleFunctorFlags::Null, "Prints every top-level Thing and what it owns.");
        AZ_CONSOLEFREEFUNC(things_blueprints, AZ::ConsoleFunctorFlags::Null, "Prints the names of all blueprints.");
        AZ_CONSOLEFREEFUNC(
            things_dump,
            AZ::ConsoleFunctorFlags::Null,
            "things_dump <blueprint or entity id>: prints a resolved blueprint or a Thing's tree.");
    } // namespace

    void ThingSystemComponent::Reflect(AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<ThingSystemComponent, AZ::Component>()->Version(0);
        }

        if (auto* behaviorContext = azrtti_cast<AZ::BehaviorContext*>(context))
        {
            behaviorContext->EBus<ThingSystemRequestBus>("ThingSystemRequestBus")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "things")
                ->Attribute(AZ::Script::Attributes::Category, "Things")
                ->Event("Spawn", &ThingSystemRequests::Spawn)
                ->Event("SpawnComposed", &ThingSystemRequests::SpawnComposed)
                ->Event("SpawnOwned", &ThingSystemRequests::SpawnOwned)
                ->Event("Destroy", &ThingSystemRequests::Destroy)
                ->Event("Transfer", &ThingSystemRequests::Transfer)
                ->Event("IsThing", &ThingSystemRequests::IsThing)
                ->Event("GetOwner", &ThingSystemRequests::GetOwner)
                ->Event("GetOwned", &ThingSystemRequests::GetOwned)
                ->Event("GetKey", &ThingSystemRequests::GetKey)
                ->Event("SetKey", &ThingSystemRequests::SetKey)
                ->Event("FindOwnedByKey", &ThingSystemRequests::FindOwnedByKey)
                ->Event("GetBlueprint", &ThingSystemRequests::GetBlueprint)
                ->Event("HasTag", &ThingSystemRequests::HasTag)
                ->Event("GetTopLevelThings", &ThingSystemRequests::GetTopLevelThings)
                ->Event("HasBlueprint", &ThingSystemRequests::HasBlueprint)
                ->Event("GetBlueprintNames", &ThingSystemRequests::GetBlueprintNames)
                ->Event("GetBlueprintSources", &ThingSystemRequests::GetBlueprintSources)
                ->Event("ReloadData", &ThingSystemRequests::ReloadData);

            behaviorContext->EBus<ThingNotificationBus>("ThingNotificationBus")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "things")
                ->Attribute(AZ::Script::Attributes::Category, "Things")
                ->Handler<BehaviorThingNotificationHandler>();
        }
    }

    void ThingSystemComponent::GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided)
    {
        provided.push_back(AZ_CRC_CE("ThingSystemService"));
    }

    void ThingSystemComponent::GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible)
    {
        incompatible.push_back(AZ_CRC_CE("ThingSystemService"));
    }

    void ThingSystemComponent::GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required)
    {
        required.push_back(AZ_CRC_CE("ThingsModDataService"));
    }

    ThingSystemComponent::ThingSystemComponent()
    {
        if (ThingSystemInterface::Get() == nullptr)
        {
            ThingSystemInterface::Register(this);
        }
        if (ThingRegistryInterface::Get() == nullptr)
        {
            ThingRegistryInterface::Register(&m_registry);
        }
    }

    ThingSystemComponent::~ThingSystemComponent()
    {
        if (ThingRegistryInterface::Get() == &m_registry)
        {
            ThingRegistryInterface::Unregister(&m_registry);
        }
        if (ThingSystemInterface::Get() == this)
        {
            ThingSystemInterface::Unregister(this);
        }
    }

    void ThingSystemComponent::Activate()
    {
        ThingSystemRequestBus::Handler::BusConnect();
        ModDataNotificationBus::Handler::BusConnect();
    }

    void ThingSystemComponent::Deactivate()
    {
        ModDataNotificationBus::Handler::BusDisconnect();
        ThingSystemRequestBus::Handler::BusDisconnect();
    }

    AZ::EntityId ThingSystemComponent::Spawn(const AZStd::string& blueprint, const AZ::Transform& transform)
    {
        EnsureLoaded();
        const rapidjson::Value* resolved = m_library.Resolve(blueprint);
        if (!resolved)
        {
            AZ_Warning("Things", false, "There is no blueprint called '%s'.", blueprint.c_str());
            return AZ::EntityId();
        }
        return m_factory.Build(*resolved, blueprint, transform, AZ::EntityId());
    }

    AZ::EntityId ThingSystemComponent::SpawnComposed(const AZStd::vector<AZStd::string>& layers, const AZ::Transform& transform)
    {
        EnsureLoaded();
        rapidjson::Document composed;
        if (!m_library.Compose(layers, composed))
        {
            AZ_Warning("Things", false, "None of the composition layers exist; nothing is built.");
            return AZ::EntityId();
        }

        AZStd::string name;
        for (const AZStd::string& layer : layers)
        {
            name += name.empty() ? layer : "+" + layer;
        }
        return m_factory.Build(composed, name, transform, AZ::EntityId());
    }

    AZ::EntityId ThingSystemComponent::SpawnOwned(AZ::EntityId owner, const AZStd::string& blueprint)
    {
        if (!IsThing(owner))
        {
            AZ_Warning("Things", false, "'%s' can't be given to %s, which is not a Thing.", blueprint.c_str(), owner.ToString().c_str());
            return AZ::EntityId();
        }

        EnsureLoaded();
        const rapidjson::Value* resolved = m_library.Resolve(blueprint);
        if (!resolved)
        {
            AZ_Warning(
                "Things", false, "'%s' can't be given '%s': there is no such blueprint.", GetBlueprint(owner).c_str(), blueprint.c_str());
            return AZ::EntityId();
        }
        return m_factory.Build(*resolved, blueprint, AZ::Transform::CreateIdentity(), owner);
    }

    void ThingSystemComponent::Destroy(AZ::EntityId thing)
    {
        if (!IsThing(thing))
        {
            AZ_Warning("Things", false, "%s is not a Thing and can't be destroyed as one.", thing.ToString().c_str());
            return;
        }

        AZStd::vector<AZ::EntityId> tree;
        m_registry.Visit(
            thing,
            [&tree](AZ::EntityId id, AZ::u32)
            {
                tree.push_back(id);
                return VisitAction::Continue;
            });

        for (const AZ::EntityId& id : tree)
        {
            ThingNotificationBus::Event(id, &ThingNotifications::OnThingDestroying);
        }
        for (auto id = tree.rbegin(); id != tree.rend(); ++id)
        {
            AzFramework::GameEntityContextRequestBus::Broadcast(&AzFramework::GameEntityContextRequests::DestroyGameEntity, *id);
        }
    }

    bool ThingSystemComponent::Transfer(AZ::EntityId thing, AZ::EntityId newOwner)
    {
        return m_registry.Link(thing, newOwner);
    }

    bool ThingSystemComponent::IsThing(AZ::EntityId entity) const
    {
        return m_registry.Find(entity) != nullptr;
    }

    AZ::EntityId ThingSystemComponent::GetOwner(AZ::EntityId thing) const
    {
        const ThingComponent* component = m_registry.Find(thing);
        return component ? component->GetOwner() : AZ::EntityId();
    }

    AZStd::vector<AZ::EntityId> ThingSystemComponent::GetOwned(AZ::EntityId thing) const
    {
        const ThingComponent* component = m_registry.Find(thing);
        return component ? component->GetOwned() : AZStd::vector<AZ::EntityId>();
    }

    AZStd::string ThingSystemComponent::GetKey(AZ::EntityId thing) const
    {
        const ThingComponent* component = m_registry.Find(thing);
        return component ? component->GetKey() : AZStd::string();
    }

    void ThingSystemComponent::SetKey(AZ::EntityId thing, const AZStd::string& key)
    {
        ThingComponent* component = m_registry.Find(thing);
        AZ_Warning("Things", component, "%s is not a Thing and can't be given a key.", thing.ToString().c_str());
        if (component)
        {
            component->SetKey(key);
        }
    }

    AZ::EntityId ThingSystemComponent::FindOwnedByKey(AZ::EntityId owner, const AZStd::string& key) const
    {
        const ThingComponent* component = m_registry.Find(owner);
        if (!component || key.empty())
        {
            return AZ::EntityId();
        }
        for (const AZ::EntityId& owned : component->GetOwned())
        {
            const ThingComponent* child = m_registry.Find(owned);
            if (child && child->GetKey() == key)
            {
                return owned;
            }
        }
        return AZ::EntityId();
    }

    AZStd::string ThingSystemComponent::GetBlueprint(AZ::EntityId thing) const
    {
        const ThingComponent* component = m_registry.Find(thing);
        return component ? component->GetBlueprint() : AZStd::string();
    }

    bool ThingSystemComponent::HasTag(AZ::EntityId thing, const AZStd::string& tag) const
    {
        const ThingComponent* component = m_registry.Find(thing);
        return component && component->HasTag(tag);
    }

    AZ::EntityId ThingSystemComponent::FindAncestor(AZ::EntityId thing, const AZStd::function<bool(AZ::EntityId)>& match) const
    {
        return m_registry.FindAncestor(thing, match);
    }

    void ThingSystemComponent::VisitTree(AZ::EntityId root, const TreeVisitor& visitor) const
    {
        m_registry.Visit(root, visitor);
    }

    AZStd::vector<AZ::EntityId> ThingSystemComponent::GetTopLevelThings() const
    {
        return m_registry.GetTopLevel();
    }

    bool ThingSystemComponent::HasBlueprint(const AZStd::string& name)
    {
        EnsureLoaded();
        return m_library.Has(name);
    }

    const rapidjson::Value* ThingSystemComponent::GetResolvedBlueprint(AZStd::string_view name)
    {
        EnsureLoaded();
        return m_library.Resolve(name);
    }

    AZStd::vector<AZStd::string> ThingSystemComponent::GetBlueprintNames()
    {
        EnsureLoaded();
        return m_library.GetNames();
    }

    bool ThingSystemComponent::AddBlueprintLayers(AZStd::string_view json, AZStd::string_view source)
    {
        EnsureLoaded();
        auto parsed = AZ::JsonSerializationUtils::ReadJsonString(json);
        if (!parsed.IsSuccess())
        {
            AZ_Warning("Things", false, "Blueprints from '%.*s' are not valid JSON: %s", AZ_STRING_ARG(source), parsed.GetError().c_str());
            return false;
        }
        return m_library.AddFile(parsed.GetValue(), source);
    }

    void ThingSystemComponent::RemoveBlueprintLayers(AZStd::string_view source)
    {
        m_library.RemoveLayers(source);
    }

    void ThingSystemComponent::ReloadData()
    {
        if (ModDataRequests* modData = ModDataInterface::Get())
        {
            modData->Rescan();
        }
        m_loaded = false;
        EnsureLoaded();
    }

    bool ThingSystemComponent::SaveThing(AZ::EntityId thing, rapidjson::Value& output, rapidjson::Document::AllocatorType& allocator) const
    {
        return SaveAt(thing, 0, output, allocator);
    }

    bool ThingSystemComponent::SaveAt(
        AZ::EntityId thing, AZ::u32 depth, rapidjson::Value& output, rapidjson::Document::AllocatorType& allocator) const
    {
        const ThingComponent* component = m_registry.Find(thing);
        if (!component)
        {
            AZ_Warning("Things", false, "%s can't be saved: it is not a Thing.", thing.ToString().c_str());
            return false;
        }
        ThingFactory::SaveOne(*component, output, allocator);
        rapidjson::Value owned(rapidjson::kArrayType);
        AZ_Warning(
            "Things",
            depth < ThingFactory::MaxChildDepth || component->GetOwned().empty(),
            "'%s' owns Things nested deeper than %u; they are not saved.",
            component->GetBlueprint().c_str(),
            ThingFactory::MaxChildDepth);
        for (const AZ::EntityId& child : component->GetOwned())
        {
            rapidjson::Value snapshot;
            if (depth < ThingFactory::MaxChildDepth && SaveAt(child, depth + 1, snapshot, allocator))
            {
                owned.PushBack(snapshot, allocator);
            }
        }
        output.AddMember(rapidjson::StringRef(ThingFactory::OwnedKey), owned, allocator);
        return true;
    }

    AZ::EntityId ThingSystemComponent::LoadThing(const rapidjson::Value& snapshot, const AZ::Transform& transform, AZ::EntityId owner)
    {
        if (owner.IsValid() && !IsThing(owner))
        {
            AZ_Warning("Things", false, "A saved Thing can't be given to %s, which is not a Thing.", owner.ToString().c_str());
            return AZ::EntityId();
        }
        return m_factory.BuildSnapshot(snapshot, transform, owner);
    }

    AZStd::vector<AZStd::string> ThingSystemComponent::GetBlueprintSources(const AZStd::string& name)
    {
        EnsureLoaded();
        return m_library.GetSources(name);
    }

    const ThingRegistry& ThingSystemComponent::GetRegistry() const
    {
        return m_registry;
    }

    void ThingSystemComponent::OnModDataChanged()
    {
        m_loaded = false;
    }

    void ThingSystemComponent::EnsureLoaded()
    {
        if (m_loaded)
        {
            return;
        }
        m_loaded = true;
        m_library.Clear();

        const ModDataRequests* modData = ModDataInterface::Get();
        AZ_Assert(modData, "ThingSystemComponent needs the ModDataSystemComponent.");
        if (!modData)
        {
            return;
        }

        AZStd::string folder = DefaultBlueprintFolder;
        if (const AZ::SettingsRegistryInterface* registry = AZ::SettingsRegistry::Get())
        {
            registry->Get(folder, BlueprintFolderRegistryPath);
        }

        const AZStd::vector<DataFile> files = modData->FindFiles(folder, ".json");
        for (const DataFile& file : files)
        {
            const AZStd::string source = AZStd::string::format("%s:%s", file.m_root.c_str(), file.m_relativePath.c_str());
            auto parsed = AZ::JsonSerializationUtils::ReadJsonFile(file.m_path.Native());
            if (!parsed.IsSuccess())
            {
                AZ_Warning(
                    "Things", false, "Blueprint file '%s' could not be read and is skipped: %s", source.c_str(), parsed.GetError().c_str());
                continue;
            }
            m_library.AddFile(parsed.GetValue(), source);
        }
        AZ_Info("Things", "%zu blueprints from %zu files.\n", m_library.GetCount(), files.size());
    }
} // namespace Things
