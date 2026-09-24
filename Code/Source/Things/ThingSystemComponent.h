#pragma once

#include <AzCore/Component/Component.h>
#include <Blueprints/BlueprintLibrary.h>
#include <Things/ModDataBus.h>
#include <Things/ThingFactory.h>
#include <Things/ThingRegistry.h>
#include <Things/ThingSystemBus.h>

namespace Things
{
    //! Builds Things from blueprints, tracks ownership and serves the blueprints of the game and its mods.
    //!
    //! Blueprints are the JSON files in the blueprint folder (Settings Registry "/Things/BlueprintFolder", default
    //! "blueprints") of every data root, read lazily on first use and again whenever the mod data changes.
    class ThingSystemComponent
        : public AZ::Component
        , public ThingSystemRequestBus::Handler
        , public ModDataNotificationBus::Handler
    {
    public:
        AZ_COMPONENT(ThingSystemComponent, ThingSystemComponentTypeId);

        //! Reflects the component and the script API.
        static void Reflect(AZ::ReflectContext* context);

        //! Provides the Thing system service.
        static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided);

        //! Only one instance may exist.
        static void GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible);

        //! Needs the mod data to find blueprint files.
        static void GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required);

        //! Registers the ThingSystemInterface and ThingRegistryInterface.
        ThingSystemComponent();

        //! Unregisters the interfaces.
        ~ThingSystemComponent() override;

        //! ThingSystemRequests overrides.
        //! @{
        AZ::EntityId Spawn(const AZStd::string& blueprint, const AZ::Transform& transform) override;
        AZ::EntityId SpawnComposed(const AZStd::vector<AZStd::string>& layers, const AZ::Transform& transform) override;
        AZ::EntityId SpawnOwned(AZ::EntityId owner, const AZStd::string& blueprint) override;
        void Destroy(AZ::EntityId thing) override;
        bool Transfer(AZ::EntityId thing, AZ::EntityId newOwner) override;
        bool IsThing(AZ::EntityId entity) const override;
        AZ::EntityId GetOwner(AZ::EntityId thing) const override;
        AZStd::vector<AZ::EntityId> GetOwned(AZ::EntityId thing) const override;
        AZStd::string GetBlueprint(AZ::EntityId thing) const override;
        bool HasTag(AZ::EntityId thing, const AZStd::string& tag) const override;
        AZ::EntityId FindAncestor(AZ::EntityId thing, const AZStd::function<bool(AZ::EntityId)>& match) const override;
        void VisitTree(AZ::EntityId root, const TreeVisitor& visitor) const override;
        AZStd::vector<AZ::EntityId> GetTopLevelThings() const override;
        bool HasBlueprint(const AZStd::string& name) override;
        const rapidjson::Value* GetResolvedBlueprint(AZStd::string_view name) override;
        AZStd::vector<AZStd::string> GetBlueprintNames() override;
        AZStd::vector<AZStd::string> GetBlueprintSources(const AZStd::string& name) override;
        bool AddBlueprintLayers(AZStd::string_view json, AZStd::string_view source) override;
        void ReloadData() override;
        //! @}

        //! The registry of active Things.
        const ThingRegistry& GetRegistry() const;

    protected:
        //! Connects the buses.
        void Activate() override;

        //! Disconnects the buses.
        void Deactivate() override;

    private:
        //! Marks the blueprints stale so they are read again on next use.
        void OnModDataChanged() override;

        //! Reads the blueprint files unless they are current.
        void EnsureLoaded();

        BlueprintLibrary m_library; //!< The blueprints.
        ThingRegistry m_registry; //!< The active Things.
        ThingFactory m_factory{ m_library }; //!< Builds Things from m_library.
        bool m_loaded = false; //!< Whether m_library holds the current files.
    };
} // namespace Things
