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
stone
tools
```

`[construction]` declares material costs. `[storage]` declares per-resource
capacity. `[inputs]` and `[outputs]` declare one production cycle, whose positive
duration is set by `[recipe].cycle_minutes`.

Recipe inputs and outputs must fit their corresponding storage capacities.
`source_policy = recipe_outputs` makes only declared outputs available to other
buildings. `all_stored` makes every stored resource available.

## Map Gathering

An optional `[gathering]` section makes a recipe consume a finite resource from
map tiles when each cycle starts:

```ini
[gathering]
map_resource = forest
radius = 6
units_per_cycle = 1
```

- `map_resource`: Stable map-resource ID. Currently `forest` and `stone`.
- `radius`: Manhattan collection radius measured from the building footprint.
- `units_per_cycle`: Quantity removed from the nearest in-range tiles.

The nearest deposits are consumed deterministically, breaking ties by map row
and column. Production stops with a visible blocker when the required quantity
is no longer available. The selected-building inspector shows the collection
radius and remaining in-range quantity.

## Placement Rules

An optional `[placement]` section can restrict where a building may be placed:

```ini
[placement]
requires_terrain = fertile
```

- `requires_terrain`: Stable terrain ID required under every footprint tile.
  Currently `grass`, `fertile`, `rocky`, and `shallow_water` are valid IDs,
  though shallow water also blocks construction globally.

The client placement preview reports terrain blockers, and the construction menu
includes required terrain in the details row.

## Current Boundary

Definitions can compose existing simulation behavior. A building that needs a
new algorithm, resource type, transport mode, UI placement command, or special
rule still needs C++ work.

The client construction menu is generated from the active catalog. Custom
definitions automatically receive a selectable row with their name, color,
construction materials, footprint, worker or housing capacity, required terrain,
and labor cost.
The first seven listed buildings receive numeric shortcuts.

Save format version 6 records stable IDs and a catalog fingerprint. Changing a
simulation-relevant definition intentionally makes existing saves incompatible
until a migration is implemented.
