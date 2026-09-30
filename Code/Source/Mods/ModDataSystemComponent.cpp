#include <Mods/ModDataSystemComponent.h>

#include <AzCore/IO/FileIO.h>
#include <AzCore/Serialization/Json/JsonSerialization.h>
#include <AzCore/Serialization/Json/JsonUtils.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/Settings/SettingsRegistry.h>
#include <AzCore/StringFunc/StringFunc.h>
#include <AzCore/std/algorithm.h>
#include <AzCore/std/sort.h>
#include <Things/TypedJson.h>

namespace Things
{
    namespace
    {
        //! Name of the root that holds the game's own data.
        constexpr const char* GameRootName = "Game";

        //! Calls fn with the name of every entry directly inside the folder.
        template<class Fn>
        void ForEachEntry(const AZ::IO::Path& folder, Fn&& fn)
        {
            AZ::IO::FileIOBase* fileIO = AZ::IO::FileIOBase::GetInstance();
            fileIO->FindFiles(
                folder.c_str(),
                "*",
                [&fn](const char* path)
                {
                    fn(AZ::IO::PathView(path).Filename());
                    return true;
                });
        }

        //! Adds every file with the extension below folder, recursively, with paths relative to the root. Every other entry
        //! is looked into, since IsDirectory asks only the disk and misses folders inside archives; a file lists nothing.
        void CollectFiles(
            const DataRoot& root, const AZ::IO::Path& relativeFolder, AZStd::string_view extension, AZStd::vector<DataFile>& out)
        {
            ForEachEntry(
                root.m_path / relativeFolder,
                [&](AZ::IO::PathView name)
                {
                    const AZ::IO::Path relative = relativeFolder / name;
                    if (AZ::StringFunc::Equal(name.Extension().Native(), extension))
                    {
                        out.push_back({ root.m_name, root.m_path / relative, relative });
                    }
                    else if (name != "." && name != "..")
                    {
                        CollectFiles(root, relative, extension, out);
                    }
                });
        }
    } // namespace

    void ModDataConfig::Reflect(AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<ModDataConfig>()
                ->Version(0)
                ->Field("DataRoots", &ModDataConfig::m_dataRoots)
                ->Field("ModRoots", &ModDataConfig::m_modRoots)
                ->Field("Order", &ModDataConfig::m_order)
                ->Field("Disabled", &ModDataConfig::m_disabled);
        }
    }

    void ModDataSystemComponent::Reflect(AZ::ReflectContext* context)
    {
        ModDataConfig::Reflect(context);
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<ModDataSystemComponent, AZ::Component>()->Version(0);
        }
    }

    void ModDataSystemComponent::GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided)
    {
        provided.push_back(AZ_CRC_CE("ThingsModDataService"));
    }

    void ModDataSystemComponent::GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible)
    {
        incompatible.push_back(AZ_CRC_CE("ThingsModDataService"));
    }

    ModDataSystemComponent::ModDataSystemComponent()
    {
        if (ModDataInterface::Get() == nullptr)
        {
            ModDataInterface::Register(this);
        }
    }

    ModDataSystemComponent::~ModDataSystemComponent()
    {
        if (ModDataInterface::Get() == this)
        {
            ModDataInterface::Unregister(this);
        }
    }

    void ModDataSystemComponent::Activate()
    {
        Rescan();
    }

    void ModDataSystemComponent::Deactivate()
    {
    }

    void ModDataSystemComponent::Configure(const ModDataConfig& config)
    {
        m_config = config;
        BuildRoots();
    }

    void ModDataSystemComponent::Rescan()
    {
        m_config = ModDataConfig{};
        if (AZ::SettingsRegistryInterface* registry = AZ::SettingsRegistry::Get())
        {
            registry->GetObject(m_config, ModDataConfigRegistryPath);
        }
        BuildRoots();
    }

    const AZStd::vector<DataRoot>& ModDataSystemComponent::GetRoots() const
    {
        return m_roots;
    }

    AZStd::vector<DataFile> ModDataSystemComponent::FindFiles(AZStd::string_view folder, AZStd::string_view extension) const
    {
        AZ_Assert(AZ::IO::FileIOBase::GetInstance(), "ModDataSystemComponent needs a FileIO instance.");

        AZStd::vector<DataFile> files;
        for (const DataRoot& root : m_roots)
        {
            const size_t first = files.size();
            CollectFiles(root, AZ::IO::Path(folder), extension, files);
            AZStd::sort(
                files.begin() + first,
                files.end(),
                [](const DataFile& lhs, const DataFile& rhs)
                {
                    return lhs.m_relativePath < rhs.m_relativePath;
                });
        }
        return files;
    }

    bool ModDataSystemComponent::ReadLayered(AZStd::string_view relativePath, rapidjson::Document& out) const
    {
        AZ::IO::FileIOBase* fileIO = AZ::IO::FileIOBase::GetInstance();
        AZ_Assert(fileIO, "ModDataSystemComponent needs a FileIO instance.");

        out.SetObject();
        bool found = false;
        for (const DataRoot& root : m_roots)
        {
            const AZ::IO::Path path = root.m_path / relativePath;
            if (!fileIO->Exists(path.c_str()))
            {
                continue;
            }

            auto layer = AZ::JsonSerializationUtils::ReadJsonFile(path.Native());
            if (!layer.IsSuccess())
            {
                AZ_Warning("Things", false, "Data file '%s' could not be read and is skipped: %s", path.c_str(), layer.GetError().c_str());
                continue;
            }

            AZ::JsonSerialization::ApplyPatch(out, out.GetAllocator(), layer.GetValue(), AZ::JsonMergeApproach::JsonMergePatch);
            found = true;
        }
        return found;
    }

    bool ModDataSystemComponent::LoadLayered(AZStd::string_view relativePath, void* object, const AZ::TypeId& objectType) const
    {
        rapidjson::Document layered;
        if (!ReadLayered(relativePath, layered))
        {
            return false;
        }
        LoadTypedJson(object, objectType, layered, AZStd::string::format("Data file '%.*s'", AZ_STRING_ARG(relativePath)));
        return true;
    }

    void ModDataSystemComponent::BuildRoots()
    {
        m_roots.clear();
        for (const AZStd::string& dataRoot : m_config.m_dataRoots)
        {
            m_roots.push_back({ GameRootName, AZ::IO::Path(dataRoot) });
        }

        AZ::IO::FileIOBase* fileIO = AZ::IO::FileIOBase::GetInstance();
        for (const AZStd::string& modRoot : m_config.m_modRoots)
        {
            const AZ::IO::Path rootPath(modRoot);
            if (!fileIO || !fileIO->IsDirectory(rootPath.c_str()))
            {
                continue;
            }

            AZStd::vector<AZStd::string> mods;
            ForEachEntry(
                rootPath,
                [&](AZ::IO::PathView name)
                {
                    if (fileIO->IsDirectory((rootPath / name).c_str()))
                    {
                        mods.emplace_back(name.Native());
                    }
                });

            const auto rank = [this](const AZStd::string& mod)
            {
                const auto ordered = AZStd::find(m_config.m_order.begin(), m_config.m_order.end(), mod);
                return static_cast<size_t>(ordered - m_config.m_order.begin());
            };
            AZStd::sort(
                mods.begin(),
                mods.end(),
                [&rank](const AZStd::string& lhs, const AZStd::string& rhs)
                {
                    const size_t lhsRank = rank(lhs);
                    const size_t rhsRank = rank(rhs);
                    return lhsRank != rhsRank ? lhsRank < rhsRank : lhs < rhs;
                });

            for (const AZStd::string& mod : mods)
            {
                if (AZStd::find(m_config.m_disabled.begin(), m_config.m_disabled.end(), mod) != m_config.m_disabled.end())
                {
                    AZ_Info("Things", "Mod '%s' is disabled.\n", mod.c_str());
                    continue;
                }
                AZ_Info("Things", "Mod '%s' loads from '%s'.\n", mod.c_str(), (rootPath / mod).c_str());
                m_roots.push_back({ mod, rootPath / mod });
            }
        }

        ModDataNotificationBus::Broadcast(&ModDataNotifications::OnModDataChanged);
    }
} // namespace Things
