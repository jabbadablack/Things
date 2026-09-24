#include <AzCore/Serialization/Json/JsonUtils.h>
#include <AzTest/AzTest.h>
#include <TestParts.h>
#include <Things/Testing/ThingsTestFixture.h>
#include <Things/TypedJson.h>

namespace Things::Testing
{
    //! Tests of creating reflected objects from JSON with a "$type".
    class TypedJsonTests : public ThingsTestFixture
    {
    protected:
        //! Registers the test parts.
        AZStd::vector<DescriptorFactory> GetDescriptors() const override
        {
            return GetTestPartDescriptors();
        }

        //! Parses JSON text.
        static rapidjson::Document Parse(AZStd::string_view json)
        {
            auto parsed = AZ::JsonSerializationUtils::ReadJsonString(json);
            EXPECT_TRUE(parsed.IsSuccess());
            return parsed.TakeValue();
        }
    };

    TEST_F(TypedJsonTests, CreatesByClassNameAndLoadsValues)
    {
        const rapidjson::Document json =
            Parse(R"({"$type": "TestValuePart", "Value": 4, "List": [1, 2], "Map": {"a": 0.5}, "Text": "hi"})");
        AZStd::unique_ptr<AZ::Component> component(CreateFromTypedJson<AZ::Component>(json, "test"));
        auto* part = azrtti_cast<TestValuePart*>(component.get());
        ASSERT_NE(part, nullptr);
        EXPECT_EQ(part->m_value, 4);
        EXPECT_EQ(part->m_list, (AZStd::vector<int>{ 1, 2 })) << "containers are replaced, not appended to";
        EXPECT_FLOAT_EQ(part->m_map["a"], 0.5f);
        EXPECT_EQ(part->m_text, "hi");
    }

    TEST_F(TypedJsonTests, CreatesByTypeId)
    {
        const rapidjson::Document json = Parse(R"({"$type": "{80103496-F10C-475E-A2F1-56CECC5CC019}", "Other": 2.5})");
        AZStd::unique_ptr<AZ::Component> component(CreateFromTypedJson<AZ::Component>(json, "test"));
        auto* part = azrtti_cast<TestOtherPart*>(component.get());
        ASSERT_NE(part, nullptr);
        EXPECT_FLOAT_EQ(part->m_other, 2.5f);
    }

    TEST_F(TypedJsonTests, MissingOrUnknownTypesWarnAndReturnNothing)
    {
        TraceCounter trace;
        EXPECT_EQ(CreateFromTypedJson<AZ::Component>(Parse(R"({"Value": 1})"), "no type"), nullptr);
        EXPECT_EQ(CreateFromTypedJson<AZ::Component>(Parse(R"({"$type": "NoSuchPart"})"), "unknown"), nullptr);
        EXPECT_EQ(CreateFromTypedJson<AZ::Component>(Parse(R"([1])"), "array"), nullptr);
        EXPECT_GE(trace.m_warnings, 3);
        EXPECT_TRUE(trace.HasWarningContaining("no type"));
        EXPECT_TRUE(trace.HasWarningContaining("unknown"));
    }

    TEST_F(TypedJsonTests, TypesThatAreNotTheBaseWarn)
    {
        TraceCounter trace;
        EXPECT_EQ(CreateFromTypedJson<TestOtherPart>(Parse(R"({"$type": "TestValuePart"})"), "wrong base"), nullptr);
        EXPECT_TRUE(trace.HasWarningContaining("wrong base"));
    }

    TEST_F(TypedJsonTests, UnknownFieldsWarnButTheRestLoads)
    {
        TraceCounter trace;
        AZStd::unique_ptr<AZ::Component> component(
            CreateFromTypedJson<AZ::Component>(Parse(R"({"$type": "TestValuePart", "Value": 3, "Typo": 1})"), "Crate part 'Weight'"));
        auto* part = azrtti_cast<TestValuePart*>(component.get());
        ASSERT_NE(part, nullptr);
        EXPECT_EQ(part->m_value, 3);
        EXPECT_TRUE(trace.HasWarningContaining("Crate part 'Weight'"));
    }
} // namespace Things::Testing
