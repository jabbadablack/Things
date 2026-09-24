#include <Things/Testing/ThingsTestFixture.h>

#include <AzCore/Asset/AssetManagerComponent.h>
#include <AzCore/Component/TickBus.h>
#include <AzCore/IO/Streamer/StreamerComponent.h>
#include <AzCore/Jobs/JobManagerComponent.h>
#include <AzCore/Slice/SliceSystemComponent.h>
#include <AzCore/Task/TaskGraphSystemComponent.h>
#include <AzCore/UnitTest/UnitTest.h>
#include <AzFramework/Asset/AssetSystemComponent.h>
#include <AzFramework/Entity/GameEntityContextComponent.h>
#include <Mods/ModDataSystemComponent.h>
#include <Things/ModDataBus.h>
#include <Things/ThingBodyComponent.h>
#include <Things/ThingComponent.h>
#include <Things/ThingSystemBus.h>
#include <Things/ThingSystemComponent.h>

namespace Things::Testing
{
    ThingsTestApplication::ThingsTestApplication(AZStd::vector<DescriptorFactory> extraDescriptors)
        : m_extraDescriptors(AZStd::move(extraDescriptors))
    {
    }

    void ThingsTestApplication::RegisterCoreComponents()
    {
        AzFramework::Application::RegisterCoreComponents();
        RegisterComponentDescriptor(ModDataSystemComponent::CreateDescriptor());
        RegisterComponentDescriptor(ThingSystemComponent::CreateDescriptor());
        RegisterComponentDescriptor(ThingComponent::CreateDescriptor());
        RegisterComponentDescriptor(ThingBodyComponent::CreateDescriptor());
        for (DescriptorFactory factory : m_extraDescriptors)
        {
            RegisterComponentDescriptor(factory());
        }
    }

    AZ::ComponentTypeList ThingsTestApplication::GetRequiredSystemComponents() const
    {
        return AZ::ComponentTypeList{
            azrtti_typeid<AZ::AssetManagerComponent>(),
            azrtti_typeid<AZ::JobManagerComponent>(),
            azrtti_typeid<AZ::TaskGraphSystemComponent>(),
            azrtti_typeid<AZ::StreamerComponent>(),
            azrtti_typeid<AZ::SliceSystemComponent>(),
            azrtti_typeid<AzFramework::GameEntityContextComponent>(),
            azrtti_typeid<AzFramework::AssetSystem::AssetSystemComponent>(),
            azrtti_typeid<ModDataSystemComponent>(),
            azrtti_typeid<ThingSystemComponent>(),
        };
    }

    TraceCounter::TraceCounter()
    {
        BusConnect();
    }

    TraceCounter::~TraceCounter()
    {
        BusDisconnect();
    }

    bool TraceCounter::OnPreWarning(
        [[maybe_unused]] const char* window,
        [[maybe_unused]] const char* fileName,
        [[maybe_unused]] int line,
        [[maybe_unused]] const char* func,
        const char* message)
    {
        ++m_warnings;
        m_messages.emplace_back(message);
        return true;
    }

    bool TraceCounter::OnPreError(
        [[maybe_unused]] const char* window,
        [[maybe_unused]] const char* fileName,
        [[maybe_unused]] int line,
        [[maybe_unused]] const char* func,
        [[maybe_unused]] const char* message)
    {
        ++m_errors;
        return false;
    }

    bool TraceCounter::HasWarningContaining(AZStd::string_view text) const
    {
        for (const AZStd::string& message : m_messages)
        {
            if (message.contains(text))
            {
                return true;
            }
        }
        return false;
    }

    void ThingsTestFixture::SetUp()
    {
        AZ::ComponentApplication::Descriptor descriptor;
        descriptor.m_useExistingAllocator = true;
        AZ::ComponentApplication::StartupParameters startup;
        startup.m_loadSettingsRegistry = false;

        m_application = AZStd::make_unique<ThingsTestApplication>(GetDescriptors());
        m_application->Start(descriptor, startup);
        UseData({});
    }

    void ThingsTestFixture::TearDown()
    {
        FlushQueuedEvents();
        m_application->Stop();
        m_application.reset();
    }

    AZStd::vector<DescriptorFactory> ThingsTestFixture::GetDescriptors() const
    {
        return {};
    }

    void ThingsTestFixture::UseData(AZStd::vector<AZStd::string> dataRoots, AZStd::vector<AZStd::string> modRoots)
    {
        ModDataConfig config;
        config.m_dataRoots = AZStd::move(dataRoots);
        config.m_modRoots = AZStd::move(modRoots);
        ModDataRequests* modData = ModDataInterface::Get();
        ASSERT_NE(modData, nullptr);
        modData->Configure(config);
    }

    void ThingsTestFixture::FlushQueuedEvents()
    {
        AZ::TickBus::ExecuteQueuedEvents();
    }

    void ThingsTestFixture::AddBlueprints(AZStd::string_view json)
    {
        ThingSystemRequests* things = ThingSystemInterface::Get();
        ASSERT_NE(things, nullptr);
        EXPECT_TRUE(things->AddBlueprintLayers(json, "test"));
    }
} // namespace Things::Testing
