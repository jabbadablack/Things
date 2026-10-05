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
        EXPECT_EQ(ears->m_events, (AZStd::vector<AZStd::string>{ "OwnedAdded", "TreeChanged", "OwnedAdded", "TreeChanged", "Built" }));
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

    TEST_F(ThingSpawnTests, TopLevelThingsFollowTransfersAndOwnersComingAndGoing)
    {
        const AZ::EntityId rock = Things().Spawn("Rock", AZ::Transform::CreateIdentity());
        const AZ::EntityId sword = Things().SpawnOwned(rock, "Sword");
        ASSERT_TRUE(Things().Transfer(sword, AZ::EntityId()));
        EXPECT_EQ(Things().GetTopLevelThings().size(), 2u) << "a sword given to nobody lies on its own";
        ASSERT_TRUE(Things().Transfer(sword, rock));
        EXPECT_EQ(Things().GetTopLevelThings(), AZStd::vector<AZ::EntityId>{ rock });

        FindEntity(rock)->Deactivate();
        EXPECT_EQ(Things().GetTopLevelThings(), AZStd::vector<AZ::EntityId>{ sword }) << "its owner isn't a Thing while inactive";
        FindEntity(rock)->Activate();
        EXPECT_EQ(Things().GetTopLevelThings(), AZStd::vector<AZ::EntityId>{ rock }) << "and owns it again once active";
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
        EXPECT_EQ(FindPart<TestListenerPart>(holder)->m_events.end()[-2], "OwnedAdded") << "then TreeChanged";
        EXPECT_EQ(FindPart<TestListenerPart>(hero)->m_events.end()[-2], "OwnedRemoved");

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
        EXPECT_EQ(holderEars->m_events.end()[-2], "OwnedRemoved");

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

    TEST_F(ThingSpawnTests, BodyIsInTheWorldOnlyWhileTheThingIsTopLevel)
    {
        AddBlueprints(R"({
            "Lamp":   {"Parts": {"Transform": {"$type": "TransformComponent"}, "Body": {"$type": "ThingBodyComponent"}}},
            "Statue": {"Parts": {"Transform": {"$type": "TransformComponent"},
                                 "Body": {"$type": "ThingBodyComponent", "Looks": {"Carried": {"Socket": "Hand"}}}}},
            "Bearer": {"Children": {"Lamp": {"Blueprint": "Lamp"}}}
        })");
        TraceCounter trace;
        const auto isInWorld = [](AZ::EntityId thing)
        {
            bool inWorld = false;
            ThingBodyRequestBus::EventResult(inWorld, thing, &ThingBodyRequests::IsInWorld);
            return inWorld;
        };

        const AZ::EntityId bearer = Things().Spawn("Bearer", AZ::Transform::CreateIdentity());
        ASSERT_EQ(Things().GetOwned(bearer).size(), 1u);
        const AZ::EntityId lamp = Things().GetOwned(bearer).front();
        EXPECT_FALSE(isInWorld(lamp));

        ASSERT_TRUE(Things().Transfer(lamp, AZ::EntityId()));
        EXPECT_TRUE(isInWorld(lamp));
        ASSERT_TRUE(Things().Transfer(lamp, bearer));
        EXPECT_FALSE(isInWorld(lamp));

        const AZ::EntityId statue = Things().Spawn("Statue", AZ::Transform::CreateIdentity());
        ASSERT_TRUE(Things().Transfer(statue, bearer));
        EXPECT_FALSE(isInWorld(statue)) << "the default look stays out of the world while owned";
        ThingBodyRequestBus::Event(statue, &ThingBodyRequests::SetLook, AZStd::string("Carried"));
        EXPECT_TRUE(isInWorld(statue)) << "a named look shows while owned, on its owner's socket";
        AZStd::string look;
        ThingBodyRequestBus::EventResult(look, statue, &ThingBodyRequests::GetLook);
        EXPECT_EQ(look, "Carried");
    }

    TEST_F(ThingSpawnTests, BodiesShowOnlyWhenAskedAndWithTheirOwners)
    {
        AddBlueprints(R"({
            "Torch":  {"Parts": {"Transform": {"$type": "TransformComponent"},
                                 "Body": {"$type": "ThingBodyComponent", "Looks": {"Held": {"Socket": "Hand"}}}}},
            "Bearer": {"Parts": {"Transform": {"$type": "TransformComponent"}, "Body": {"$type": "ThingBodyComponent"}},
                       "Children": {"Torch": {"Blueprint": "Torch"}}}
        })");
        const auto isShown = [](AZ::EntityId thing)
        {
            bool shown = false;
            ThingBodyRequestBus::EventResult(shown, thing, &ThingBodyRequests::IsShown);
            return shown;
        };
        const AZ::EntityId bearer = Things().Spawn("Bearer", AZ::Transform::CreateIdentity());
        ASSERT_EQ(Things().GetOwned(bearer).size(), 1u);
        const AZ::EntityId torch = Things().GetOwned(bearer).front();
        EXPECT_TRUE(isShown(bearer));
        EXPECT_FALSE(isShown(torch)) << "carried in the pack";
        ThingBodyRequestBus::Event(torch, &ThingBodyRequests::SetLook, AZStd::string("Held"));
        EXPECT_TRUE(isShown(torch)) << "held in hand";
        ThingBodyRequestBus::Event(bearer, &ThingBodyRequests::SetShown, false);
        EXPECT_FALSE(isShown(bearer));
        EXPECT_FALSE(isShown(torch)) << "a hidden bearer hides what it holds";
        ThingBodyRequestBus::Event(bearer, &ThingBodyRequests::SetShown, true);
        EXPECT_TRUE(isShown(torch));

        TraceCounter trace;
        ThingBodyRequestBus::Event(torch, &ThingBodyRequests::SetLook, AZStd::string("Worn"));
        EXPECT_TRUE(trace.HasWarningContaining("has no look 'Worn'"));
        AZStd::string look;
        ThingBodyRequestBus::EventResult(look, torch, &ThingBodyRequests::GetLook);
        EXPECT_EQ(look, "Held") << "an unknown look changes nothing";
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
    TEST_F(ThingSpawnTests, ASavedThingIsBuiltAgainAsItWas)
    {
        TraceCounter trace;
        const AZ::Transform place = AZ::Transform::CreateTranslation(AZ::Vector3(4.0f, 5.0f, 6.0f));
        const AZ::EntityId hero = Things().Spawn("Hero", AZ::Transform::CreateIdentity());
        FindPart<TestValuePart>(hero)->m_value = 42;
        FindPart<TestValuePart>(hero)->m_text = "scarred";
        Things().Destroy(Things().GetOwned(hero)[1]);
        const AZ::EntityId rock = Things().SpawnOwned(hero, "Rock");
        FindPart<TestValuePart>(rock)->m_value = 9;

        rapidjson::Document snapshot;
        ASSERT_TRUE(Things().SaveThing(hero, snapshot, snapshot.GetAllocator()));
        Things().Destroy(hero);
        EXPECT_TRUE(Things().GetTopLevelThings().empty());

        const AZ::EntityId loaded = Things().LoadThing(snapshot, place, AZ::EntityId());
        ASSERT_TRUE(loaded.IsValid());
        EXPECT_EQ(Things().GetBlueprint(loaded), "Hero");
        EXPECT_TRUE(Things().HasTag(loaded, "Hero"));
        EXPECT_FALSE(Things().HasTag(loaded, "Monster"));
        EXPECT_EQ(FindPart<TestValuePart>(loaded)->m_value, 42);
        EXPECT_EQ(FindPart<TestValuePart>(loaded)->m_text, "scarred");
        EXPECT_EQ(FindPart<ThingComponent>(loaded)->GetParts().size(), 3u);
        AZ::Transform world = AZ::Transform::CreateIdentity();
        AZ::TransformBus::EventResult(world, loaded, &AZ::TransformBus::Events::GetWorldTM);
        EXPECT_TRUE(world.IsClose(place));

        const AZStd::vector<AZ::EntityId> owned = Things().GetOwned(loaded);
        ASSERT_EQ(owned.size(), 2u) << "what it owned when saved, not what its blueprint gives";
        EXPECT_EQ(Things().GetBlueprint(owned[0]), "Sword");
        EXPECT_EQ(SumTree(owned[0]), 4);
        EXPECT_EQ(Things().GetBlueprint(owned[1]), "Rock");
        EXPECT_EQ(SumTree(owned[1]), 9);
        EXPECT_EQ(Things().GetOwner(owned[1]), loaded);
        EXPECT_EQ(FindPart<TestListenerPart>(loaded)->m_events.back(), "Built") << "a loaded Thing is built like a spawned one";
        EXPECT_EQ(trace.m_warnings, 0);
    }

    TEST_F(ThingSpawnTests, ChildrenKeepTheirKeys)
    {
        const AZ::EntityId hero = Things().Spawn("Hero", AZ::Transform::CreateIdentity());
        const AZStd::vector<AZ::EntityId> owned = Things().GetOwned(hero);
        ASSERT_EQ(owned.size(), 2u);
        EXPECT_EQ(Things().GetKey(owned[0]), "Blade");
        EXPECT_EQ(Things().FindOwnedByKey(hero, "Pebble"), owned[1]);
        EXPECT_FALSE(Things().FindOwnedByKey(hero, "Shield").IsValid());
        EXPECT_TRUE(Things().GetKey(hero).empty()) << "a top-level Thing has no key";

        const AZ::EntityId rock = Things().SpawnOwned(hero, "Rock");
        EXPECT_TRUE(Things().GetKey(rock).empty());
        Things().SetKey(rock, "Spare");
        EXPECT_EQ(Things().FindOwnedByKey(hero, "Spare"), rock);

        const AZ::EntityId holder = Things().Spawn("Holder", AZ::Transform::CreateIdentity());
        ASSERT_TRUE(Things().Transfer(owned[0], holder));
        EXPECT_EQ(Things().FindOwnedByKey(holder, "Blade"), owned[0]) << "a key moves with its Thing";

        rapidjson::Document snapshot;
        ASSERT_TRUE(Things().SaveThing(hero, snapshot, snapshot.GetAllocator()));
        const AZ::EntityId loaded = Things().LoadThing(snapshot, AZ::Transform::CreateIdentity(), AZ::EntityId());
        EXPECT_TRUE(Things().FindOwnedByKey(loaded, "Pebble").IsValid()) << "keys survive a save";
        EXPECT_TRUE(Things().FindOwnedByKey(loaded, "Spare").IsValid());
    }

    TEST_F(ThingSpawnTests, DeepTreesSaveAndLoadWhole)
    {
        constexpr AZ::u32 Depth = 12;
        TraceCounter trace;
        const AZ::EntityId root = Things().Spawn("Rock", AZ::Transform::CreateIdentity());
        AZ::EntityId deepest = root;
        for (AZ::u32 i = 0; i < Depth; ++i)
        {
            deepest = Things().SpawnOwned(deepest, "Rock");
        }
        ASSERT_EQ(SumTree(root), static_cast<int>(Depth) + 1);

        rapidjson::Document snapshot;
        ASSERT_TRUE(Things().SaveThing(root, snapshot, snapshot.GetAllocator()));
        const AZ::EntityId loaded = Things().LoadThing(snapshot, AZ::Transform::CreateIdentity(), AZ::EntityId());
        EXPECT_EQ(SumTree(loaded), static_cast<int>(Depth) + 1) << "a creature, its arm, hand, bag and what is in it";
        EXPECT_EQ(trace.m_warnings, 0);
    }

    TEST_F(ThingSpawnTests, TreeChangesReachEveryAncestorAndDescendant)
    {
        const AZ::EntityId holder = Things().Spawn("Holder", AZ::Transform::CreateIdentity());
        const AZ::EntityId middle = Things().SpawnOwned(holder, "Holder");
        const AZ::EntityId pouch = Things().SpawnOwned(middle, "Holder");
        const AZ::EntityId bead = Things().SpawnOwned(pouch, "Holder");
        const AZ::EntityId rock = Things().Spawn("Rock", AZ::Transform::CreateIdentity());
        auto& holderEvents = FindPart<TestListenerPart>(holder)->m_events;
        auto& beadEvents = FindPart<TestListenerPart>(bead)->m_events;

        holderEvents.clear();
        ASSERT_TRUE(Things().Transfer(rock, pouch));
        EXPECT_EQ(holderEvents.back(), "TreeChanged") << "the top owner hears of a Thing put deep inside it";

        holderEvents.clear();
        beadEvents.clear();
        ASSERT_TRUE(Things().Transfer(pouch, AZ::EntityId()));
        EXPECT_EQ(holderEvents.back(), "TreeChanged") << "and of one taken out";
        EXPECT_EQ(beadEvents.back(), "AncestryChanged") << "what the moved Thing owns hears it has new ancestors";

        holderEvents.clear();
        Things().Destroy(bead);
        EXPECT_TRUE(holderEvents.empty()) << "the pouch is no longer the holder's";
        ASSERT_TRUE(Things().Transfer(pouch, middle));
        holderEvents.clear();
        Things().Destroy(rock);
        EXPECT_EQ(holderEvents.back(), "TreeChanged") << "a Thing destroyed deep inside counts too";
    }

    TEST_F(ThingSpawnTests, OwnedLooksFollowTheNearestAncestorWithABody)
    {
        AddBlueprints(R"({
            "Torch":  {"Parts": {"Transform": {"$type": "TransformComponent"},
                                 "Body": {"$type": "ThingBodyComponent", "Looks": {"Held": {"Socket": "Hand"}}}}},
            "Hand":   {"Children": {"Torch": {"Blueprint": "Torch"}}},
            "Bearer": {"Parts": {"Transform": {"$type": "TransformComponent"}, "Body": {"$type": "ThingBodyComponent"}},
                       "Children": {"HandL": {"Blueprint": "Hand"}}}
        })");
        const auto isShown = [](AZ::EntityId thing)
        {
            bool shown = false;
            ThingBodyRequestBus::EventResult(shown, thing, &ThingBodyRequests::IsShown);
            return shown;
        };
        const AZ::EntityId bearer = Things().Spawn("Bearer", AZ::Transform::CreateIdentity());
        const AZ::EntityId hand = Things().FindOwnedByKey(bearer, "HandL");
        const AZ::EntityId torch = Things().FindOwnedByKey(hand, "Torch");
        ASSERT_TRUE(torch.IsValid());
        ThingBodyRequestBus::Event(torch, &ThingBodyRequests::SetLook, AZStd::string("Held"));
        EXPECT_TRUE(isShown(torch));
        ThingBodyRequestBus::Event(bearer, &ThingBodyRequests::SetShown, false);
        EXPECT_FALSE(isShown(torch)) << "a hidden bearer hides what its hand holds";
        ThingBodyRequestBus::Event(bearer, &ThingBodyRequests::SetShown, true);
        EXPECT_TRUE(isShown(torch));

        const AZ::EntityId other = Things().Spawn("Bearer", AZ::Transform::CreateIdentity());
        ASSERT_TRUE(Things().Transfer(hand, other));
        ThingBodyRequestBus::Event(other, &ThingBodyRequests::SetShown, false);
        EXPECT_FALSE(isShown(torch)) << "the torch follows its hand to the new bearer";
    }

    TEST_F(ThingSpawnTests, ASnapshotWithoutPartsWarns)
    {
        TraceCounter trace;
        rapidjson::Document snapshot;
        snapshot.Parse(R"({"Parts": 3})");
        EXPECT_FALSE(Things().LoadThing(snapshot, AZ::Transform::CreateIdentity(), AZ::EntityId()).IsValid());
        EXPECT_GT(trace.m_warnings, 0);
    }
} // namespace Things::Testing
