#pragma once

#include <AzCore/Debug/TraceMessageBus.h>
#include <AzCore/UnitTest/TestTypes.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/smart_ptr/unique_ptr.h>
#include <AzCore/std/string/string.h>
#include <AzFramework/Application/Application.h>

namespace Things::Testing
{
    //! Creates a component descriptor, e.g. &MyPart::CreateDescriptor.
    using DescriptorFactory = AZ::ComponentDescriptor* (*)();

    //! A small application with a game entity context and the Things system components, for unit tests.
    class ThingsTestApplication : public AzFramework::Application
    {
    public:
        AZ_CLASS_ALLOCATOR(ThingsTestApplication, AZ::SystemAllocator);

        //! Creates the application; it registers the extra descriptors next to the Things ones when it starts.
        explicit ThingsTestApplication(AZStd::vector<DescriptorFactory> extraDescriptors);

    protected:
        //! Registers the core, Things and extra descriptors.
        void RegisterCoreComponents() override;

        //! Only the system components the tests need.
        AZ::ComponentTypeList GetRequiredSystemComponents() const override;

    private:
        AZStd::vector<DescriptorFactory> m_extraDescriptors; //!< Descriptors of the test's own components.
    };

    //! Counts warnings and errors while it exists and keeps them out of the test output.
    //! Errors still count as failures unless the test also suppresses them with AZ_TEST_START_TRACE_SUPPRESSION.
    class TraceCounter : public AZ::Debug::TraceMessageBus::Handler
    {
    public:
        //! Starts counting.
        TraceCounter();

        //! Stops counting.
        ~TraceCounter() override;

        //! Counts and hides a warning.
        bool OnPreWarning(const char* window, const char* fileName, int line, const char* func, const char* message) override;

        //! Counts an error.
        bool OnPreError(const char* window, const char* fileName, int line, const char* func, const char* message) override;

        //! Whether any counted warning contains the text.
        bool HasWarningContaining(AZStd::string_view text) const;

        int m_warnings = 0;                      //!< Warnings seen.
        int m_errors = 0;                        //!< Errors seen.
        AZStd::vector<AZStd::string> m_messages; //!< Warning texts, in order.
    };

    //! Starts a ThingsTestApplication for each test, with no data roots, so tests add blueprints themselves.
    class ThingsTestFixture : public UnitTest::LeakDetectionFixture
    {
    protected:
        //! Starts the application.
        void SetUp() override;

        //! Stops the application.
        void TearDown() override;

        //! Descriptors of the test's own components; override to add them.
        virtual AZStd::vector<DescriptorFactory> GetDescriptors() const;

        //! Points the mod data at data roots and mod roots.
        void UseData(AZStd::vector<AZStd::string> dataRoots, AZStd::vector<AZStd::string> modRoots = {});

        //! Runs functions queued on the tick bus, such as deleting destroyed Things.
        void FlushQueuedEvents();

        //! Adds blueprints from JSON text and fails the test when they don't parse.
        void AddBlueprints(AZStd::string_view json);

        AZStd::unique_ptr<ThingsTestApplication> m_application; //!< The running application.
    };
} // namespace Things::Testing
