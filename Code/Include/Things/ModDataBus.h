#pragma once

#include <AzCore/EBus/EBus.h>
#include <AzCore/IO/Path/Path.h>
#include <AzCore/Interface/Interface.h>
#include <AzCore/JSON/document.h>
#include <AzCore/Memory/SystemAllocator.h>
#include <AzCore/RTTI/RTTI.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/string/string.h>
#include <Things/ThingsTypeIds.h>

namespace AZ
{
    class ReflectContext;
}

namespace Things
{
    //! Where data files come from: the game's data roots, then every enabled mod folder in load order.
    //! Read from the Settings Registry at ModDataConfigRegistryPath.
    struct ModDataConfig
    {
        AZ_TYPE_INFO(ModDataConfig, ModDataConfigTypeId);
        AZ_CLASS_ALLOCATOR(ModDataConfig, AZ::SystemAllocator);

        //! Registers the config with the SerializeContext so the Settings Registry can fill it.
        static void Reflect(AZ::ReflectContext* context);

        AZStd::vector<AZStd::string> m_dataRoots = { "@products@" }; //!< Game data folders, read first, in order.
        AZStd::vector<AZStd::string> m_modRoots = { "@user@/Mods" }; //!< Folders whose subfolders are mods.
        AZStd::vector<AZStd::string> m_order; //!< Mod folder names that load first, in this order.
        AZStd::vector<AZStd::string> m_disabled; //!< Mod folder names that are not loaded.
    };

    //! Settings Registry path of the ModDataConfig.
    inline constexpr AZStd::string_view ModDataConfigRegistryPath = "/Things/ModData";

    //! A folder that data files are read from: a game data root or one mod.
    struct DataRoot
    {
        AZStd::string m_name; //!< "Game" for a data root, otherwise the mod's folder name.
        AZ::IO::Path m_path; //!< The folder, possibly starting with an alias such as @products@.
    };

    //! A data file found under one of the roots.
    struct DataFile
    {
        AZStd::string m_root; //!< Name of the root the file is in.
        AZ::IO::Path m_path; //!< Full path, possibly starting with an alias.
        AZ::IO::Path m_relativePath; //!< Path below the root, which is what mods use to layer over the game's files.
    };

    //! Finds data files in the game and its mods, and layers same-named files with JSON Merge Patch.
    //! Later roots win: the game first, then mods in load order.
    class ModDataRequests
    {
    public:
        AZ_RTTI(ModDataRequests, ModDataRequestsTypeId);

        //! Destroys the interface.
        virtual ~ModDataRequests() = default;

        //! Replaces the configuration and rescans the mod folders.
        virtual void Configure(const ModDataConfig& config) = 0;

        //! Reads the configuration from the Settings Registry again and rescans the mod folders.
        virtual void Rescan() = 0;

        //! The roots in load order.
        virtual const AZStd::vector<DataRoot>& GetRoots() const = 0;

        //! Every file with the extension (such as ".json") in the folder below every root, recursively.
        //! Ordered by root, then by relative path.
        virtual AZStd::vector<DataFile> FindFiles(AZStd::string_view folder, AZStd::string_view extension) const = 0;

        //! Merge-patches every root's copy of the JSON file at relativePath into out, in load order.
        //! Returns false when no root has the file; unreadable files are skipped with a warning.
        virtual bool ReadLayered(AZStd::string_view relativePath, rapidjson::Document& out) const = 0;

        //! Loads the layered JSON file into a reflected object, warning about every field that doesn't fit.
        //! Returns false when no root has the file.
        virtual bool LoadLayered(AZStd::string_view relativePath, void* object, const AZ::TypeId& objectType) const = 0;

        //! Loads the layered JSON file into a reflected object. Returns false when no root has the file.
        template<class T>
        bool LoadLayered(AZStd::string_view relativePath, T& object) const
        {
            return LoadLayered(relativePath, &object, azrtti_typeid<T>());
        }
    };

    //! The global ModDataRequests.
    using ModDataInterface = AZ::Interface<ModDataRequests>;

    //! Tells listeners that the roots changed, so data read from them is stale.
    class ModDataNotifications : public AZ::EBusTraits
    {
    public:
        //! Destroys the handler.
        virtual ~ModDataNotifications() = default;

        //! The roots were configured or rescanned.
        virtual void OnModDataChanged()
        {
        }
    };

    //! Bus for ModDataNotifications.
    using ModDataNotificationBus = AZ::EBus<ModDataNotifications>;
} // namespace Things
