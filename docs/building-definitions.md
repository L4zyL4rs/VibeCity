# Building Definitions

Ordinary buildings are loaded from `data/buildings/*.vbd` when the process starts.
One file defines one building. Adding a production, storage, or residential
building does not require changing the simulation code when its behavior fits the
existing fields.

The format is strict by design. Unknown sections, keys, resources, duplicate IDs,
invalid ranges, and recipes that exceed their storage are rejected with the
definition filename in the error.

## Example

```ini
[building]
id = charcoal_kiln
name = Charcoal Kiln
footprint = 2, 2
worker_slots = 1
resident_capacity = 0
worker_supply = 0
consumes_bread = false
requests_storage_inputs = false
internal_construction_site = false
source_policy = recipe_outputs
map_color = 92, 104, 76
construction_labor_minutes = 1440

[construction]
timber = 4

[storage]
timber = 10
firewood = 20

[recipe]
cycle_minutes = 120

[inputs]
timber = 2

[outputs]
firewood = 4
```

Comments begin with `#`. Empty resource sections may be omitted.

## Building Fields

- `id`: Stable lowercase identifier containing letters, numbers, or underscores.
  Saves refer to this ID rather than the runtime enum value.
- `name`: Display name.
- `footprint`: Width and height in map tiles, each from 1 through 64.
- `worker_slots`: Workers required for full production.
- `resident_capacity`: Maximum residents.
- `worker_supply`: Workers supplied by this building's residents.
- `consumes_bread`: Whether residents consume one bread each day.
- `requests_storage_inputs`: Whether logistics should fill every configured
  storage slot. This is used by storehouses.
- `internal_construction_site`: Reserved for the engine's
  `construction_site` definition. Custom buildings must set it to `false`.
- `source_policy`: `none`, `recipe_outputs`, or `all_stored`.
- `map_color`: Red, green, and blue values from 0 through 255.
- `construction_labor_minutes`: Builder-minutes required to complete the
  building.

## Resource Sections

The currently supported stable resource IDs are:

```text
grain
bread
timber
firewood
tools
```

`[construction]` declares material costs. `[storage]` declares per-resource
capacity. `[inputs]` and `[outputs]` declare one production cycle, whose positive
duration is set by `[recipe].cycle_minutes`.

Recipe inputs and outputs must fit their corresponding storage capacities.
`source_policy = recipe_outputs` makes only declared outputs available to other
buildings. `all_stored` makes every stored resource available.

## Current Boundary

Definitions can compose existing simulation behavior. A building that needs a
new algorithm, resource type, transport mode, UI placement command, or special
rule still needs C++ work.

The client palette currently exposes the built-in village buildings explicitly.
Custom definitions are supported by the simulation, command layer, renderer,
inspector, and save system, but do not yet receive an automatic palette button.

Save format version 2 records stable IDs and a catalog fingerprint. Changing a
simulation-relevant definition intentionally makes existing saves incompatible
until a migration is implemented.
