#include <AzCore/Component/ComponentApplicationBus.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzTest/AzTest.h>
#include <TestParts.h>
#include <Things/ModDataBus.h>
#include <Things/Testing/ThingsTestFixture.h>
#include <Things/ThingSystemBus.h>

namespace Things::Testing
{
    namespace
    {
        //! A reflected table that data files are loaded into.
        struct TestTable
        {
            AZ_TYPE_INFO(TestTable, "{5FFF733C-1C7A-4F3C-A957-307B56112113}");

            int m_a = 0; //!< A number.
            AZStd::unordered_map<AZStd::string, int> m_b; //!< Numbers by key.
        };

        //! The folder with the gem's test data.
        AZStd::string DataPath(const char* relative)
        {
            return AZStd::string::format("%s/%s", THINGS_TEST_DATA_DIR, relative);
        }
    } // namespace

    //! Tests of data roots, mods and layered files.
    class ModDataTests : public ThingsTestFixture
    {
    protected:
        //! Registers the test parts.
        AZStd::vector<DescriptorFactory> GetDescriptors() const override
        {
            return GetTestPartDescriptors();
        }

        //! Uses the test data with the given mod order and disabled mods.
        void UseTestData(AZStd::vector<AZStd::string> order = {}, AZStd::vector<AZStd::string> disabled = { "C_Disabled" })
        {
            ModDataConfig config;
            config.m_dataRoots = { DataPath("Game") };
            config.m_modRoots = { DataPath("Mods") };
            config.m_order = AZStd::move(order);
            config.m_disabled = AZStd::move(disabled);
            ModDataInterface::Get()->Configure(config);
        }

        //! The names of the roots in load order.
        AZStd::vector<AZStd::string> RootNames() const
        {
            AZStd::vector<AZStd::string> names;
            for (const DataRoot& root : ModDataInterface::Get()->GetRoots())
            {
                names.push_back(root.m_name);
            }
            return names;
        }
    };

    TEST_F(ModDataTests, ModsLoadAlphabeticallyAfterTheGame)
    {
        UseTestData();
        EXPECT_EQ(RootNames(), (AZStd::vector<AZStd::string>{ "Game", "A_First", "B_Second" }));
    }

    TEST_F(ModDataTests, ConfiguredOrderComesFirstAndDisabledModsAreSkipped)
    {
        UseTestData({ "B_Second" }, {});
        EXPECT_EQ(RootNames(), (AZStd::vector<AZStd::string>{ "Game", "B_Second", "A_First", "C_Disabled" }));
    }

    TEST_F(ModDataTests, MissingModRootsAreIgnored)
    {
        ModDataConfig config;
        config.m_dataRoots = { DataPath("Game") };
        config.m_modRoots = { DataPath("NoSuchFolder") };
        ModDataInterface::Get()->Configure(config);
        EXPECT_EQ(RootNames(), AZStd::vector<AZStd::string>{ "Game" });
    }

    TEST_F(ModDataTests, FindFilesListsMatchingFilesByRootThenPath)
    {
        UseTestData();
        const AZStd::vector<DataFile> files = ModDataInterface::Get()->FindFiles("blueprints", ".json");
        ASSERT_EQ(files.size(), 3u);
        EXPECT_EQ(files[0].m_root, "Game");
        EXPECT_EQ(files[0].m_relativePath, AZ::IO::Path("blueprints/props/crate.json"));
        EXPECT_EQ(files[1].m_root, "A_First");
        EXPECT_EQ(files[2].m_root, "B_Second");
    }

    TEST_F(ModDataTests, ReadLayeredMergesEveryRootsCopyInOrder)
    {
        UseTestData();
        rapidjson::Document table;
        ASSERT_TRUE(ModDataInterface::Get()->ReadLayered("rules/table.json", table));
        EXPECT_EQ(table["A"].GetInt(), 2);
        EXPECT_EQ(table["B"]["X"].GetInt(), 1);
        EXPECT_EQ(table["B"]["Y"].GetInt(), 5);

        rapidjson::Document missing;
        EXPECT_FALSE(ModDataInterface::Get()->ReadLayered("rules/missing.json", missing));
    }

    TEST_F(ModDataTests, LoadLayeredFillsAReflectedObject)
    {
        AZ::SerializeContext* serializeContext = nullptr;
        AZ::ComponentApplicationBus::BroadcastResult(serializeContext, &AZ::ComponentApplicationRequests::GetSerializeContext);
        serializeContext->Class<TestTable>()->Field("A", &TestTable::m_a)->Field("B", &TestTable::m_b);

        UseTestData();
        TestTable table;
        EXPECT_TRUE(ModDataInterface::Get()->LoadLayered("rules/table.json", table));
        EXPECT_EQ(table.m_a, 2);
        EXPECT_EQ(table.m_b["X"], 1);
        EXPECT_EQ(table.m_b["Y"], 5);

        serializeContext->EnableRemoveReflection();
        serializeContext->Class<TestTable>();
        serializeContext->DisableRemoveReflection();
    }

    TEST_F(ModDataTests, BlueprintsLayerTheGameAndItsMods)
    {
        UseTestData();
        ThingSystemRequests* things = ThingSystemInterface::Get();
        const AZ::EntityId crate = things->Spawn("Crate", AZ::Transform::CreateIdentity());
        ASSERT_TRUE(crate.IsValid());

        int total = 0;
        TestQueryBus::Event(crate, &TestQueries::AddValue, total);
        EXPECT_EQ(total, 25);
        EXPECT_TRUE(things->HasTag(crate, "Container"));
        EXPECT_TRUE(things->HasTag(crate, "Wooden"));
        EXPECT_EQ(things->GetBlueprintSources("Crate").size(), 3u);
    }

    TEST_F(ModDataTests, ChangingTheModsReloadsTheBlueprints)
    {
        UseTestData();
        ThingSystemRequests* things = ThingSystemInterface::Get();
        EXPECT_TRUE(things->HasBlueprint("Crate"));

        UseData({});
        EXPECT_FALSE(things->HasBlueprint("Crate"));
    }
} // namespace Things::Testing
