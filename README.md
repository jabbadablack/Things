# Things

An O3DE gem for a data-driven, moddable object model in the style of Caves of Qud: everything in a game is a **Thing**, an entity built from a JSON **blueprint**.

## What it gives you

- **Blueprints**: JSON objects by name.
  - They `Inherit` other blueprints and layer over them with JSON Merge Patch (a `null` removes something).
  - Their `Parts` are any reflected components, created by `"$type"`.
  - Their `Children` are owned Things: skills, items, body parts, whatever the game needs.
- **Things**: entities with a `ThingComponent`, owning other Things in an ordered tree (not the transform hierarchy).
  - `ThingSystemRequests` (`ThingSystemBus.h`) spawns, owns, transfers, visits, saves and loads them.
  - A Thing's body (`ThingBodyComponent`) is a prefab shown while the Thing is top-level.
- **Mods**: folders that mirror the game's data folders, layered over it (`ModDataBus.h`). `things_reload` reads everything again.
- **Runtime layers**: `AddBlueprintLayers(json, source)` adds blueprints made while playing (e.g. a created character), and `RemoveBlueprintLayers(source)` takes them away again.

## Settings

| Registry key | What it sets |
|---|---|
| `/Things/BlueprintFolder` | Where blueprints are read from in the game's data |
| `/Things/ModData` | The data roots and the mods folder (see `ModDataBus.h`) |

## Layout

- `Things.API`: headers.
- `Things.Static` and `Things`: the module.
- `Things.TestSupport`: `ThingsTestFixture`, for the tests of gems and games built on it.
- `Things.Tests`.
