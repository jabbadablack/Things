#pragma once

#include <AzCore/Component/Component.h>
#include <Things/ModDataBus.h>

namespace Things
{
    //! Finds data files in the game's data roots and in every enabled mod, and layers same-named JSON files.
    //!
    //! Mods are the subfolders of the configured mod roots, loaded after the game in the configured order and then
    //! alphabetically. The configuration comes from the Settings Registry at ModDataConfigRegistryPath.
    class ModDataSystemComponent
        : public AZ::Component
        , public ModDataRequests
    {
    public:
        AZ_COMPONENT(ModDataSystemComponent, ModDataSystemComponentTypeId);

        //! Reflects the component and ModDataConfig.
        static void Reflect(AZ::ReflectContext* context);

        //! Provides the mod data service.
        static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided);

        //! Only one instance may exist.
        static void GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible);

        //! Registers the ModDataInterface.
        ModDataSystemComponent();

        //! Unregisters the ModDataInterface.
        ~ModDataSystemComponent() override;

        //! ModDataRequests overrides.
        //! @{
        void Configure(const ModDataConfig& config) override;
        void Rescan() override;
        const AZStd::vector<DataRoot>& GetRoots() const override;
        AZStd::vector<DataFile> FindFiles(AZStd::string_view folder, AZStd::string_view extension) const override;
        bool ReadLayered(AZStd::string_view relativePath, rapidjson::Document& out) const override;
        bool LoadLayered(AZStd::string_view relativePath, void* object, const AZ::TypeId& objectType) const override;
        //! @}

    protected:
        //! Reads the configuration and scans the mod folders.
        void Activate() override;

        //! Nothing to release.
        void Deactivate() override;

    private:
        //! Rebuilds m_roots from m_config and tells listeners.
        void BuildRoots();

        ModDataConfig m_config;          //!< Where data comes from.
        AZStd::vector<DataRoot> m_roots; //!< The game's data roots, then the enabled mods, in load order.
    };
} // namespace Things
