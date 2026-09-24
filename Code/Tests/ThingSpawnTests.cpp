#include <AzCore/Component/ComponentApplicationBus.h>
#include <AzCore/Component/Entity.h>
#include <AzCore/Component/TransformBus.h>
#include <AzCore/RTTI/BehaviorContext.h>
#include <AzCore/Script/ScriptContext.h>
#include <AzCore/std/chrono/chrono.h>
#include <AzFramework/Components/TransformComponent.h>
#include <AzTest/AzTest.h>
#include <TestParts.h>
#include <Things/Testing/ThingsTestFixture.h>
#include <Things/ThingBodyComponent.h>
#include <Things/ThingComponent.h>
#include <Things/ThingFactory.h>
#include <Things/ThingSystemBus.h>
#include <cstdio>

namespace Things::Testing
{
    //! Tests of building, owning, visiting and destroying Things.
    class ThingSpawnTests : public ThingsTestFixture
    {
    protected:
        //! Registers the test parts.
        AZStd::vector<DescriptorFactory> GetDescriptors() const override
        {
            return GetTestPartDescriptors();
        }

        //! Starts the application and adds the test blueprints.
        void SetUp() override
        {
            ThingsTestFixture::SetUp();
            AddBlueprints(R"({
                "Rock":      {"Parts": {"Weight": {"$type": "TestValuePart", "Value": 1}}},
                "Sword":     {"Tags": {"Weapon": true}, "Parts": {"Weight": {"$type": "TestValuePart", "Value": 3}}},
                "Hero":      {"Tags": {"Creature": true, "Hero": true, "Monster": false},
                              "Parts": {"Weight": {"$type": "TestValuePart", "Value": 10},
                                        "Transform": {"$type": "TransformComponent"},
                                        "Ears": {"$type": "TestListenerPart"}},
                              "Children": {"Blade": {"Blueprint": "Sword", "Parts": {"Weight": {"Value": 4}}},
                                           "Pebble": {"Blueprint": "Rock"}}},
                "Recursive": {"Parts": {"Weight": {"$type": "TestValuePart", "Value": 1}},
                              "Children": {"Again": {"Blueprint": "Recursive"}}},
                "Needy":     {"Parts": {"Needs": {"$type": "TestNeedyPart"}}},
                "Holder":    {"Parts": {"Ears": {"$type": "TestListenerPart"}}}
            })");
        }

        //! The Things interface.
        ThingSystemRequests& Things() const
        {
            return *ThingSystemInterface::Get();
        }

        //! The entity with the id.
        static AZ::Entity* FindEntity(AZ::EntityId id)
        {
            AZ::Entity* entity = nullptr;
            AZ::ComponentApplicationBus::BroadcastResult(entity, &AZ::ComponentApplicationRequests::FindEntity, id);
            return entity;
        }

        //! The part of the given type on the Thing.
        template<class T>
        static T* FindPart(AZ::EntityId id)
        {
            AZ::Entity* entity = FindEntity(id);
            return entity ? entity->FindComponent<T>() : nullptr;
        }

        //! The sum of the values of every TestValuePart in the tree.
        int SumTree(AZ::EntityId root, const AZStd::function<bool(AZ::EntityId)>& include = {}) const
        {
            int total = 0;
            ForEachInTree(
                root,
                [&total](AZ::EntityId thing)
                {
                    TestQueryBus::Event(thing, &TestQueries::AddValue, total);
                },
                include);
            return total;
        }
    };

    TEST_F(ThingSpawnTests, SpawnBuildsPartsTagsAndChildren)
    {
        const AZ::Transform place = AZ::Transform::CreateTranslation(AZ::Vector3(1.0f, 2.0f, 3.0f));
        const AZ::EntityId hero = Things().Spawn("Hero", place);
        ASSERT_TRUE(hero.IsValid());
        EXPECT_TRUE(Things().IsThing(hero));
        EXPECT_EQ(Things().GetBlueprint(hero), "Hero");
        EXPECT_TRUE(Things().HasTag(hero, "Hero"));
        EXPECT_FALSE(Things().HasTag(hero, "Monster"));

        AZ::Transform world = AZ::Transform::CreateIdentity();
        AZ::TransformBus::EventResult(world, hero, &AZ::TransformBus::Events::GetWorldTM);
        EXPECT_TRUE(world.IsClose(place));

        const AZStd::vector<AZ::EntityId> owned = Things().GetOwned(hero);
        ASSERT_EQ(owned.size(), 2u);
        EXPECT_EQ(Things().GetBlueprint(owned[0]), "Sword");
        EXPECT_EQ(Things().GetOwner(owned[0]), hero);
        EXPECT_TRUE(Things().HasTag(owned[0], "Weapon"));
        EXPECT_EQ(SumTree(owned[0]), 4) << "child overrides layer onto that child only";
        EXPECT_EQ(SumTree(hero), 15);

        const ThingComponent* thing = FindPart<ThingComponent>(hero);
        ASSERT_NE(thing, nullptr);
        EXPECT_EQ(thing->GetParts().size(), 3u);
    }

    TEST_F(ThingSpawnTests, ThingBuiltArrivesAfterTheWholeTreeExists)
    {
        const AZ::EntityId hero = Things().Spawn("Hero", AZ::Transform::CreateIdentity());
        const TestListenerPart* ears = FindPart<TestListenerPart>(hero);
        ASSERT_NE(ears, nullptr);
        EXPECT_EQ(ears->m_events, (AZStd::vector<AZStd::string>{ "OwnedAdded", "OwnedAdded", "Built" }));
    }

    TEST_F(ThingSpawnTests, SpawnComposedLayersBlueprints)
    {
        const AZ::EntityId thing = Things().SpawnComposed({ "Rock", "Sword" }, AZ::Transform::CreateIdentity());
        ASSERT_TRUE(thing.IsValid());
        EXPECT_EQ(Things().GetBlueprint(thing), "Rock+Sword");
        EXPECT_EQ(SumTree(thing), 3);
        EXPECT_TRUE(Things().HasTag(thing, "Weapon"));
    }

    TEST_F(ThingSpawnTests, SpawnOwnedGivesAThingToAnOwner)
    {
        const AZ::EntityId rock = Things().Spawn("Rock", AZ::Transform::CreateIdentity());
        const AZ::EntityId sword = Things().SpawnOwned(rock, "Sword");
        ASSERT_TRUE(sword.IsValid());
        EXPECT_EQ(Things().GetOwner(sword), rock);
        EXPECT_EQ(Things().GetOwned(rock), AZStd::vector<AZ::EntityId>{ sword });
        EXPECT_EQ(Things().GetTopLevelThings(), AZStd::vector<AZ::EntityId>{ rock });
    }

    TEST_F(ThingSpawnTests, BadRequestsWarnAndBuildNothing)
    {
        TraceCounter trace;
        EXPECT_FALSE(Things().Spawn("Nope", AZ::Transform::CreateIdentity()).IsValid());
        EXPECT_FALSE(Things().SpawnOwned(AZ::EntityId(12345), "Rock").IsValid());
        EXPECT_FALSE(Things().SpawnComposed({ "Nope" }, AZ::Transform::CreateIdentity()).IsValid());
        EXPECT_GE(trace.m_warnings, 3);
        EXPECT_TRUE(Things().GetTopLevelThings().empty());
    }

    TEST_F(ThingSpawnTests, PartsThatDontFitTogetherStopTheBuild)
    {
        TraceCounter trace;
        EXPECT_FALSE(Things().Spawn("Needy", AZ::Transform::CreateIdentity()).IsValid());
        EXPECT_TRUE(trace.HasWarningContaining("don't fit together"));
        FlushQueuedEvents();
        EXPECT_TRUE(Things().GetTopLevelThings().empty());
    }

    TEST_F(ThingSpawnTests, RunawayNestingStopsAtTheDepthLimit)
    {
        TraceCounter trace;
        const AZ::EntityId root = Things().Spawn("Recursive", AZ::Transform::CreateIdentity());
        ASSERT_TRUE(root.IsValid());
        EXPECT_EQ(SumTree(root), static_cast<int>(ThingFactory::MaxChildDepth) + 1);
        EXPECT_TRUE(trace.HasWarningContaining("nest deeper"));
    }

    TEST_F(ThingSpawnTests, IncludeFilterSkipsSubtrees)
    {
        const AZ::EntityId hero = Things().Spawn("Hero", AZ::Transform::CreateIdentity());
        const AZ::EntityId blade = Things().GetOwned(hero)[0];
        EXPECT_EQ(
            SumTree(
                hero,
                [blade](AZ::EntityId thing)
                {
                    return thing != blade;
                }),
            11);
    }

    TEST_F(ThingSpawnTests, VisitCanStopEarly)
    {
        const AZ::EntityId hero = Things().Spawn("Hero", AZ::Transform::CreateIdentity());
        int visited = 0;
        Things().VisitTree(
            hero,
            [&visited](AZ::EntityId, AZ::u32 depth)
            {
                ++visited;
                return depth == 1 ? VisitAction::Stop : VisitAction::Continue;
            });
        EXPECT_EQ(visited, 2);
    }

    TEST_F(ThingSpawnTests, TransferMovesAThingAndRefusesLoops)
    {
        const AZ::EntityId hero = Things().Spawn("Hero", AZ::Transform::CreateIdentity());
        const AZ::EntityId holder = Things().Spawn("Holder", AZ::Transform::CreateIdentity());
        const AZ::EntityId blade = Things().GetOwned(hero)[0];

        EXPECT_TRUE(Things().Transfer(blade, holder));
        EXPECT_EQ(Things().GetOwner(blade), holder);
        EXPECT_EQ(Things().GetOwned(hero).size(), 1u);
        EXPECT_EQ(FindPart<TestListenerPart>(holder)->m_events.back(), "OwnedAdded");
        EXPECT_EQ(FindPart<TestListenerPart>(hero)->m_events.back(), "OwnedRemoved");

        TraceCounter trace;
        EXPECT_FALSE(Things().Transfer(holder, blade)) << "a Thing can't be owned by what it owns";
        EXPECT_FALSE(Things().Transfer(hero, hero));
        EXPECT_EQ(trace.m_warnings, 2);

        EXPECT_TRUE(Things().Transfer(blade, AZ::EntityId()));
        EXPECT_EQ(Things().GetTopLevelThings().size(), 3u);
        EXPECT_EQ(
            Things().FindAncestor(
                Things().GetOwned(hero)[0],
                [hero](AZ::EntityId id)
                {
                    return id == hero;
                }),
            hero);
    }

    TEST_F(ThingSpawnTests, DestroyRemovesTheWholeTree)
    {
        const AZ::EntityId holder = Things().Spawn("Holder", AZ::Transform::CreateIdentity());
        const AZ::EntityId hero = Things().SpawnOwned(holder, "Hero");
        const AZStd::vector<AZ::EntityId> owned = Things().GetOwned(hero);
        const TestListenerPart* holderEars = FindPart<TestListenerPart>(holder);

        Things().Destroy(hero);
        EXPECT_FALSE(Things().IsThing(hero));
        EXPECT_FALSE(Things().IsThing(owned[0]));
        EXPECT_TRUE(Things().GetOwned(holder).empty());
        EXPECT_EQ(holderEars->m_events.back(), "OwnedRemoved");

        FlushQueuedEvents();
        EXPECT_EQ(FindEntity(hero), nullptr);
        EXPECT_EQ(FindEntity(owned[1]), nullptr);
        EXPECT_EQ(Things().GetTopLevelThings(), AZStd::vector<AZ::EntityId>{ holder });
    }

    TEST_F(ThingSpawnTests, BodyWithoutAPrefabWarns)
    {
        AddBlueprints(R"({"Ghost": {"Parts": {"Transform": {"$type": "TransformComponent"}, "Body": {"$type": "ThingBodyComponent"}}}})");
        TraceCounter trace;
        const AZ::EntityId ghost = Things().Spawn("Ghost", AZ::Transform::CreateIdentity());
        EXPECT_TRUE(ghost.IsValid());
        EXPECT_TRUE(trace.HasWarningContaining("without a prefab"));
        EXPECT_TRUE(FindPart<ThingBodyComponent>(ghost)->GetBodyEntities().empty());
    }

    TEST_F(ThingSpawnTests, ThreeHundredThingsWithTenChildrenEachSpawnQuickly)
    {
        AddBlueprints(R"({"Pack": {"Parts": {"Weight": {"$type": "TestValuePart", "Value": 1}},
            "Children": {"A": {"Blueprint": "Rock"}, "B": {"Blueprint": "Rock"}, "C": {"Blueprint": "Rock"}, "D": {"Blueprint": "Rock"},
                         "E": {"Blueprint": "Rock"}, "F": {"Blueprint": "Rock"}, "G": {"Blueprint": "Rock"}, "H": {"Blueprint": "Rock"},
                         "I": {"Blueprint": "Rock"}, "J": {"Blueprint": "Rock"}}}})");

        const AZStd::chrono::steady_clock::time_point start = AZStd::chrono::steady_clock::now();
        AZStd::vector<AZ::EntityId> packs;
        for (int i = 0; i < 300; ++i)
        {
            packs.push_back(Things().Spawn("Pack", AZ::Transform::CreateIdentity()));
        }
        const auto spawnTime = AZStd::chrono::duration_cast<AZStd::chrono::milliseconds>(AZStd::chrono::steady_clock::now() - start);

        const AZStd::chrono::steady_clock::time_point queryStart = AZStd::chrono::steady_clock::now();
        int total = 0;
        for (const AZ::EntityId& pack : packs)
        {
            total += SumTree(pack);
        }
        const auto queryTime = AZStd::chrono::duration_cast<AZStd::chrono::microseconds>(AZStd::chrono::steady_clock::now() - queryStart);

        EXPECT_EQ(total, 300 * 11);
        EXPECT_LT(spawnTime.count(), 5000) << "3300 Things took too long to build";
        printf(
            "[ TIMING   ] 3300 Things built in %lld ms; 300 tree queries in %lld us\n",
            static_cast<long long>(spawnTime.count()),
            static_cast<long long>(queryTime.count()));
    }

    TEST_F(ThingSpawnTests, LuaCanSpawnAndQueryThings)
    {
        AZ::BehaviorContext* behaviorContext = nullptr;
        AZ::ComponentApplicationBus::BroadcastResult(behaviorContext, &AZ::ComponentApplicationRequests::GetBehaviorContext);
        ASSERT_NE(behaviorContext, nullptr);

        AZ::ScriptContext script;
        script.BindTo(behaviorContext);
        EXPECT_TRUE(script.Execute(R"(
            local hero = ThingSystemRequestBus.Broadcast.Spawn("Hero", Transform.CreateIdentity())
            local sword = ThingSystemRequestBus.Broadcast.SpawnOwned(hero, "Sword")
            assert(ThingSystemRequestBus.Broadcast.GetOwner(sword) == hero)
            assert(ThingSystemRequestBus.Broadcast.HasTag(hero, "Hero"))
        )"));
        EXPECT_EQ(Things().GetTopLevelThings().size(), 1u);
    }
} // namespace Things::Testing
