#include <AzCore/Serialization/Json/JsonUtils.h>
#include <AzCore/UnitTest/TestTypes.h>
#include <AzTest/AzTest.h>
#include <Blueprints/BlueprintLibrary.h>
#include <Things/Testing/ThingsTestFixture.h>

namespace Things::Testing
{
    namespace
    {
        //! The value at a JSON path of a resolved blueprint, e.g. {"Parts", "P", "V"}, or nullptr.
        const rapidjson::Value* Find(const rapidjson::Value* blueprint, std::initializer_list<const char*> path)
        {
            const rapidjson::Value* value = blueprint;
            for (const char* key : path)
            {
                if (!value || !value->IsObject() || !value->HasMember(key))
                {
                    return nullptr;
                }
                value = &(*value)[key];
            }
            return value;
        }

        //! The number at a JSON path, or -1 when there is none.
        double Number(const rapidjson::Value* blueprint, std::initializer_list<const char*> path)
        {
            const rapidjson::Value* value = Find(blueprint, path);
            return value && value->IsNumber() ? value->GetDouble() : -1.0;
        }
    } // namespace

    //! Tests of blueprint layering and inheritance.
    class BlueprintLibraryTests : public UnitTest::LeakDetectionFixture
    {
    protected:
        //! Adds the JSON text as a file.
        void Add(AZStd::string_view json, AZStd::string_view source = "test")
        {
            auto parsed = AZ::JsonSerializationUtils::ReadJsonString(json);
            ASSERT_TRUE(parsed.IsSuccess()) << parsed.GetError().c_str();
            EXPECT_TRUE(m_library.AddFile(parsed.GetValue(), source));
        }

        //! Frees the library while the leak detection is still watching.
        void TearDown() override
        {
            m_library.Clear();
            UnitTest::LeakDetectionFixture::TearDown();
        }

        BlueprintLibrary m_library; //!< The library under test.
    };

    TEST_F(BlueprintLibraryTests, LayerPropertiesMergeKeyByKey)
    {
        Add(R"({"Crate": {"Parts": {"P1": {"$type": "A", "A": 1, "B": 2}}}})");
        Add(R"({"Crate": {"Parts": {"P1": {"B": 3, "C": 4}}}})");

        const rapidjson::Value* crate = m_library.Resolve("Crate");
        EXPECT_EQ(Number(crate, { "Parts", "P1", "A" }), 1.0);
        EXPECT_EQ(Number(crate, { "Parts", "P1", "B" }), 3.0);
        EXPECT_EQ(Number(crate, { "Parts", "P1", "C" }), 4.0);
        EXPECT_EQ(m_library.GetSources("Crate").size(), 2u);
    }

    TEST_F(BlueprintLibraryTests, ReplaceRemoveAndAddParts)
    {
        Add(R"({"Base": {"Parts": {"P2": {"$type": "A", "A": 1}, "P3": {"$type": "A"}, "P5": {"$type": "A", "A": 1}}}})");
        Add(R"({"Base": {"Parts": {"P2": {"$replace": true, "Z": 9}, "P3": null, "P4": {"$type": "B"}, "P5": {"$type": "B", "Q": 2}}}})");

        TraceCounter trace;
        const rapidjson::Value* base = m_library.Resolve("Base");
        EXPECT_EQ(Find(base, { "Parts", "P2", "A" }), nullptr);
        EXPECT_EQ(Number(base, { "Parts", "P2", "Z" }), 9.0);
        EXPECT_EQ(Find(base, { "Parts", "P2", "$replace" }), nullptr);
        EXPECT_EQ(Find(base, { "Parts", "P3" }), nullptr);
        EXPECT_NE(Find(base, { "Parts", "P4" }), nullptr);
        EXPECT_EQ(Find(base, { "Parts", "P5", "A" }), nullptr) << "a changed type replaces the part";
        EXPECT_EQ(Number(base, { "Parts", "P5", "Q" }), 2.0);
        EXPECT_TRUE(trace.HasWarningContaining("changes type"));
    }

    TEST_F(BlueprintLibraryTests, LaterBasesWin)
    {
        Add(R"({
            "Thing":   {"Parts": {"P": {"$type": "A", "V": 1, "Only": 1}}},
            "Left":    {"Inherits": ["Thing"], "Parts": {"P": {"V": 2}, "L": {"$type": "A"}}},
            "Right":   {"Inherits": ["Thing"], "Parts": {"P": {"V": 3}, "L": null}},
            "Diamond": {"Inherits": ["Left", "Right"]}
        })");

        const rapidjson::Value* diamond = m_library.Resolve("Diamond");
        ASSERT_NE(diamond, nullptr);
        EXPECT_EQ(Number(diamond, { "Parts", "P", "V" }), 3.0);
        EXPECT_EQ(Number(diamond, { "Parts", "P", "Only" }), 1.0);
        EXPECT_NE(Find(diamond, { "Parts", "L" }), nullptr) << "a base can't remove what another base adds";
        EXPECT_EQ(Find(diamond, { "Inherits" }), nullptr) << "reserved keys are stripped";
    }

    TEST_F(BlueprintLibraryTests, OwnLayerWinsAndRemovesInheritedParts)
    {
        Add(R"({
            "Thing": {"Parts": {"P": {"$type": "A", "V": 1}}},
            "Left":  {"Inherits": ["Thing"], "Parts": {"L": {"$type": "A"}}},
            "Own":   {"Inherits": ["Left"], "Parts": {"P": {"V": 7}, "L": null}}
        })");

        const rapidjson::Value* own = m_library.Resolve("Own");
        EXPECT_EQ(Number(own, { "Parts", "P", "V" }), 7.0);
        EXPECT_EQ(Find(own, { "Parts", "L" }), nullptr);
    }

    TEST_F(BlueprintLibraryTests, ModLayerRemovesInheritedPart)
    {
        Add(R"({"Creature": {"Parts": {"Vitals": {"$type": "A"}}}, "Goblin": {"Inherits": ["Creature"]}})");
        Add(R"({"Goblin": {"Parts": {"Vitals": null}}})", "mod");

        EXPECT_EQ(Find(m_library.Resolve("Goblin"), { "Parts", "Vitals" }), nullptr);
        EXPECT_NE(Find(m_library.Resolve("Creature"), { "Parts", "Vitals" }), nullptr);
    }

    TEST_F(BlueprintLibraryTests, LoadReplaceDiscardsEarlierLayers)
    {
        Add(R"({"Barrel": {"Parts": {"A": {"$type": "A"}}}})");
        Add(R"({"Barrel": {"Load": "Replace", "Parts": {"C": {"$type": "A"}}}})");

        const rapidjson::Value* barrel = m_library.Resolve("Barrel");
        EXPECT_EQ(Find(barrel, { "Parts", "A" }), nullptr);
        EXPECT_NE(Find(barrel, { "Parts", "C" }), nullptr);
        EXPECT_EQ(Find(barrel, { "Load" }), nullptr);
    }

    TEST_F(BlueprintLibraryTests, ComposeLayersResolvedBlueprintsInOrder)
    {
        Add(R"({
            "Thing": {"Parts": {"P": {"$type": "A", "V": 1}}},
            "Left":  {"Inherits": ["Thing"], "Parts": {"P": {"V": 2}}},
            "Right": {"Inherits": ["Thing"], "Parts": {"P": {"V": 3}}}
        })");

        rapidjson::Document composed;
        EXPECT_TRUE(m_library.Compose({ "Right", "Left" }, composed));
        EXPECT_EQ(Number(&composed, { "Parts", "P", "V" }), 2.0);

        TraceCounter trace;
        EXPECT_FALSE(m_library.Compose({ "Nope" }, composed));
        EXPECT_EQ(trace.m_warnings, 1);
    }

    TEST_F(BlueprintLibraryTests, ProblemsAreReportedNotFatal)
    {
        Add(R"({
            "Thing":  {"Parts": {"P": {"$type": "A"}}},
            "CycleA": {"Inherits": ["CycleB"], "Parts": {"A": {"$type": "A"}}},
            "CycleB": {"Inherits": ["CycleA"], "Parts": {"B": {"$type": "A"}}},
            "Orphan": {"Inherits": ["Missing", "Thing"]}
        })");

        TraceCounter trace;
        const rapidjson::Value* cycle = m_library.Resolve("CycleA");
        ASSERT_NE(cycle, nullptr);
        EXPECT_NE(Find(cycle, { "Parts", "A" }), nullptr);
        EXPECT_TRUE(trace.HasWarningContaining("cycle"));

        const int before = trace.m_warnings;
        const rapidjson::Value* orphan = m_library.Resolve("Orphan");
        ASSERT_NE(orphan, nullptr);
        EXPECT_NE(Find(orphan, { "Parts", "P" }), nullptr);
        EXPECT_EQ(trace.m_warnings, before + 1);

        EXPECT_EQ(m_library.Resolve("Nope"), nullptr);
    }

    TEST_F(BlueprintLibraryTests, BadDefinitionsAreSkippedWithWarnings)
    {
        TraceCounter trace;
        auto parsed = AZ::JsonSerializationUtils::ReadJsonString(R"({"NotAnObject": 3, "Ok": {"Load": "Sometimes"}})");
        ASSERT_TRUE(parsed.IsSuccess());
        EXPECT_TRUE(m_library.AddFile(parsed.GetValue(), "bad"));
        EXPECT_FALSE(m_library.Has("NotAnObject"));
        EXPECT_TRUE(m_library.Has("Ok"));
        EXPECT_EQ(trace.m_warnings, 2);

        auto array = AZ::JsonSerializationUtils::ReadJsonString("[1, 2]");
        EXPECT_FALSE(m_library.AddFile(array.GetValue(), "array"));
    }

    TEST_F(BlueprintLibraryTests, ResultsAreMemoizedUntilTheNextLayer)
    {
        Add(R"({"Thing": {"Parts": {"P": {"$type": "A", "V": 1}}}})");
        const rapidjson::Value* first = m_library.Resolve("Thing");
        EXPECT_EQ(first, m_library.Resolve("Thing"));

        Add(R"({"Thing": {"Parts": {"P": {"V": 5}}}})");
        EXPECT_EQ(Number(m_library.Resolve("Thing"), { "Parts", "P", "V" }), 5.0);
        EXPECT_EQ(m_library.GetNames(), AZStd::vector<AZStd::string>{ "Thing" });
    }
} // namespace Things::Testing
