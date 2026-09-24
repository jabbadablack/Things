#pragma once

#include <AzCore/Component/Component.h>
#include <AzCore/Component/ComponentBus.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/std/containers/unordered_map.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/string/string.h>
#include <Things/ThingBus.h>

namespace Things::Testing
{
    //! A query that every TestValuePart of an owned tree answers by adding its value.
    class TestQueries : public AZ::ComponentBus
    {
    public:
        //! Adds the part's value to total.
        virtual void AddValue(int& total) = 0;
    };

    //! Bus for TestQueries.
    using TestQueryBus = AZ::EBus<TestQueries>;

    //! A part with values of several kinds, to test loading from JSON.
    class TestValuePart
        : public AZ::Component
        , public TestQueryBus::Handler
    {
    public:
        AZ_COMPONENT(TestValuePart, "{1E95CE82-B9B4-443B-870C-BA93C190470F}");

        //! Reflects the values.
        static void Reflect(AZ::ReflectContext* context)
        {
            if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
            {
                serializeContext->Class<TestValuePart, AZ::Component>()
                    ->Version(0)
                    ->Field("Value", &TestValuePart::m_value)
                    ->Field("List", &TestValuePart::m_list)
                    ->Field("Map", &TestValuePart::m_map)
                    ->Field("Text", &TestValuePart::m_text);
            }
        }

        //! Answers queries.
        void Activate() override
        {
            TestQueryBus::Handler::BusConnect(GetEntityId());
        }

        //! Stops answering queries.
        void Deactivate() override
        {
            TestQueryBus::Handler::BusDisconnect();
        }

        //! Adds m_value.
        void AddValue(int& total) override
        {
            total += m_value;
        }

        int m_value = 0; //!< A number.
        AZStd::vector<int> m_list = { 7 }; //!< A list with a default element.
        AZStd::unordered_map<AZStd::string, float> m_map; //!< A keyed map.
        AZStd::string m_text; //!< A string.
    };

    //! A second part type, to test type changes between layers.
    class TestOtherPart : public AZ::Component
    {
    public:
        AZ_COMPONENT(TestOtherPart, "{80103496-F10C-475E-A2F1-56CECC5CC019}");

        //! Reflects the value.
        static void Reflect(AZ::ReflectContext* context)
        {
            if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
            {
                serializeContext->Class<TestOtherPart, AZ::Component>()->Version(0)->Field("Other", &TestOtherPart::m_other);
            }
        }

        //! Nothing to do.
        void Activate() override
        {
        }

        //! Nothing to do.
        void Deactivate() override
        {
        }

        float m_other = 0.0f; //!< A number.
    };

    //! A part that needs a service nothing provides, so its Thing can't activate.
    class TestNeedyPart : public AZ::Component
    {
    public:
        AZ_COMPONENT(TestNeedyPart, "{B8F8BEAD-70C7-4EBE-9D2B-4694B71994F1}");

        //! Reflects the type.
        static void Reflect(AZ::ReflectContext* context)
        {
            if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
            {
                serializeContext->Class<TestNeedyPart, AZ::Component>()->Version(0);
            }
        }

        //! Requires a service no component provides.
        static void GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required)
        {
            required.push_back(AZ_CRC_CE("TestServiceNobodyProvides"));
        }

        //! Nothing to do.
        void Activate() override
        {
        }

        //! Nothing to do.
        void Deactivate() override
        {
        }
    };

    //! Records every ThingNotification its Thing receives.
    class TestListenerPart
        : public AZ::Component
        , public ThingNotificationBus::Handler
    {
    public:
        AZ_COMPONENT(TestListenerPart, "{D5F24AA8-8C4F-468B-AE63-988E84878BF6}");

        //! Reflects the type.
        static void Reflect(AZ::ReflectContext* context)
        {
            if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
            {
                serializeContext->Class<TestListenerPart, AZ::Component>()->Version(0);
            }
        }

        //! Starts listening.
        void Activate() override
        {
            ThingNotificationBus::Handler::BusConnect(GetEntityId());
        }

        //! Stops listening.
        void Deactivate() override
        {
            ThingNotificationBus::Handler::BusDisconnect();
        }

        //! Records the event.
        void OnThingBuilt() override
        {
            m_events.push_back("Built");
        }

        //! Records the event.
        void OnOwnerChanged(AZ::EntityId, AZ::EntityId) override
        {
            m_events.push_back("OwnerChanged");
        }

        //! Records the event.
        void OnOwnedAdded(AZ::EntityId) override
        {
            m_events.push_back("OwnedAdded");
        }

        //! Records the event.
        void OnOwnedRemoved(AZ::EntityId) override
        {
            m_events.push_back("OwnedRemoved");
        }

        //! Records the event.
        void OnThingDestroying() override
        {
            m_events.push_back("Destroying");
        }

        AZStd::vector<AZStd::string> m_events; //!< Events in the order they arrived.
    };

    //! Descriptors of every test part.
    inline AZStd::vector<AZ::ComponentDescriptor* (*)()> GetTestPartDescriptors()
    {
        return {
            &TestValuePart::CreateDescriptor,
            &TestOtherPart::CreateDescriptor,
            &TestNeedyPart::CreateDescriptor,
            &TestListenerPart::CreateDescriptor,
        };
    }
} // namespace Things::Testing
