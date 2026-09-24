# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

**Things** is a shared O3DE 2.8 gem: a data-driven, moddable object model in the style of Caves of Qud, where everything in a game is a *Thing* built from JSON *blueprints*. It is registered in `~/.o3de/o3de_manifest.json` and used by the Enea project (`C:/Users/cisco/O3DE/Projects/Enea`). It builds as part of a project that enables it; it has no build tree of its own.

**The gem knows nothing about any game.** No rules, grids, turns or genre concepts belong here. Game gems build on it through its buses.

## Model

- **Thing:** a runtime `AZ::Entity` in the game entity context with a `ThingComponent` (blueprint name, tags, part keys, owner, owned list).
- **Part:** any reflected `AZ::Component`, created from JSON by `"$type"` (a SerializeContext class name or TypeId) with `Things::CreateFromTypedJson<AZ::Component>` (`Include/Things/TypedJson.h`). Its descriptor must be registered by some module.
- **Ownership:** Things own other Things (skills, items, body parts) in an ordered tree kept on `ThingComponent`. The transform hierarchy is not used for ownership. Walk it with `ThingSystemRequests::VisitTree` or `ForEachInTree(root, fn, include)`, and send typed `AZ::ComponentBus` events to each Thing.
- **Blueprint:** a JSON object keyed by name:
  ```json
  {"Goblin": {"Inherits": ["Creature"], "Load": "Merge", "Tags": {"Monster": true},
              "Parts": {"Body": {"$type": "ThingBodyComponent", "Prefab": {"assetId": {"guid": "{...}", "subId": 0}}}},
              "Children": {"Fists": {"Blueprint": "Weapon_Fists", "Parts": {"Melee": {"Damage": "thr"}}}}}}
  ```
  - All layering (mods, inheritance, recipes, child overrides) is JSON Merge Patch (`BlueprintLibrary::ApplyLayer`): objects merge, `null` removes, anything else replaces. So anything a mod may patch must be an object keyed by id; arrays such as `Inherits` are replaced whole.
  - A part whose `"$type"` changes, or that says `"$replace": true`, replaces the part below instead of merging. A type change without `"$replace"` warns only within one blueprint's own layers; layering resolved bases or recipe layers replaces silently.
  - Part type names are global across gems: pick distinctive class names (`ActorComponent` collides with EMotionFX's).
  - `"Load": "Replace"` discards the earlier layers of that name.
  - Raw layers are kept per name, so a mod's `null` removes a part the blueprint only inherits.
  - The key `"Id"` is reserved for the component id and is stripped from parts with a warning.
- **Data and mods:** `ModDataSystemComponent` reads the game's data roots, then every mod folder under the mod roots (alphabetical, or ordered by the Settings Registry). Configure it at `/Things/ModData` (`DataRoots` default `["@products@"]`, `ModRoots` default `["@user@/Mods"]`, `Order`, `Disabled`). Blueprints are the `.json` files under `/Things/BlueprintFolder` (default `blueprints`) of every root. `ModDataRequests::LoadLayered` gives any game data file (rules tables, recipes) the same mod layering. Folder and file names are lower-case, because Asset Processor products are.
- **Body:** `ThingBodyComponent` spawns a prefab (spawnable, by AssetId) as a child of the Thing's transform; `ThingBodyRequestBus::GetBodyEntities` lists the spawned entities (e.g. to hide them). Game logic never depends on it. Without a spawnable system (unit tests) it warns once and stays invisible.

## Layout

- `Code/Include/Things/`: the public API (`Things.API`). `ThingSystemBus.h` (spawn, destroy, ownership, blueprints), `ThingBus.h` (per-Thing notifications and body requests), `ModDataBus.h`, `TypedJson.h`, `ThingsTypeIds.h`.
- `Code/Source/`: `Blueprints/BlueprintLibrary`, `Mods/ModDataSystemComponent`, `Things/` (`ThingComponent`, `ThingRegistry`, `ThingFactory`, `ThingBodyComponent`, `ThingSystemComponent` with the script reflection and console commands), `ThingsModule.cpp`.
- `Code/Tests/`: AzTest suites, test parts (`TestParts.h`) and `Data/` (a game root and three mods).
- `Code/Tests/Support/Things/Testing/`: `ThingsTestFixture`, `ThingsTestApplication` and `TraceCounter`, built as `Things.TestSupport` so game test targets can reuse them.

Targets: `Things.API` (headers), `Things.Static` (all code), `Things` (the module), `Things.TestSupport`, `Things.Tests`. Games link `Gem::Things.API` in their runtime code, and `Gem::Things.Static` plus `Gem::Things.TestSupport` in their tests. Don't link `Things.Static` into a game module: it would duplicate the gem's code and console variables.

## Building and testing

Build and test from the project that enables the gem (Enea). From `C:/Users/cisco/O3DE/Projects/Enea`:

```powershell
cmd /c 'call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul && ninja -C build\windows -f build-profile.ninja Things Things.Tests AzTestRunner'
cd build/windows/bin/profile; ./AzTestRunner.exe Things.Tests.dll AzRunUnitTests
```

- Filter with `--gtest_filter=ThingSpawnTests.*`.
- Every source file must be listed in a `Code/things_*_files.cmake`.
- Format with the engine's `.clang-format`: `clang-format -i --style=file:C:/Users/cisco/O3DE/Engines/o3de/.clang-format <files>`.

## Debugging

- Console commands: `things_reload`, `things_list`, `things_blueprints`, `things_dump <blueprint or entity id>`.
- CVar `things_log_spawns` logs every Thing built.
- Every data problem is an `AZ_Warning` in the `Things` window naming the blueprint, part and file; loading carries on.

## Coding guidelines

The same rules as the Enea project, with the `things_` prefix:

- **Design.** Clean Architecture in O3DE idioms: components, EBuses and `AZ::Interface`, reflection, the Settings Registry. Everything is moddable through data. Reuse before you write; don't add parallel architecture; replace, don't wrap; ask whether removing code solves the problem.
- **Verification.** Never guess: check engine APIs in `C:/Users/cisco/O3DE/Engines/o3de/`. Ask the user about decisions that are theirs.
- **Tests first.** Write the failing AzTest, make it pass, refactor. Fixtures derive from `ThingsTestFixture` or `UnitTest::LeakDetectionFixture`. Use `TraceCounter` to check that bad data warns.
- **Diagnostics.** Assert often (`AZ_Assert` conditions must never do needed work). Warn on every recoverable bad state. Prefix every CVar, console command and debug name with `things_`.
- **Comments.** Documentation comments only (`//!`, `//!<`), on every function, member, constant and enum value, 3 lines or fewer on average.
- **Commits.** One short line, no body, no Claude attribution. Keep this file current when the architecture, layout, build or tests change.
