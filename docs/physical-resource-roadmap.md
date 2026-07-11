# Physical Resource Roadmap

## Goal

Make settlement placement and expansion depend on visible geography before the
economy gains many more production chains.

The milestone is successful when a player can identify a resource on the map,
place a suitable gathering building near it, watch the deposit deplete, and
understand when production stops or must move elsewhere.

## Implemented: Forest Slice

- Deterministic fixed-seed forest groves on new maps.
- Finite quantity stored on each forest tile.
- Data-driven building gathering rules with map resource, radius, and units per
  cycle.
- Woodcutters consume the nearest forest unit when a cycle starts.
- Roads and buildings clear forest on occupied tiles.
- Selected woodcutters highlight their collection area and usable forest tiles.
- Woodcutter placement preview highlights the proposed collection radius,
  shows usable forest tiles, marks trees that would be cleared by the
  footprint, and reports the reachable forest total before placement.
- The inspector reports collection radius and total forest remaining in range.
- Depleted forest state is deterministic and persisted in save files.
- Demolition and path removal free occupied tiles without refunds or terrain
  recovery. Removed buildings persist as inactive ID tombstones in saves.

## Implemented: Terrain Foundation

- New maps generate deterministic fertile and rocky terrain bands from explicit
  world-generation settings.
- Terrain is stored as authoritative simulation data and persisted in saves.
- Shallow water is supported as blocked terrain for roads and buildings, though
  it is not generated in the starter map yet.
- Map resources validate their terrain support: forest on grass/fertile tiles
  and clay on grass/fertile tiles, stone on rocky tiles.
- Stone deposits are generated on rocky terrain and rendered separately from
  forest.
- Hovering map tiles reports terrain, resource quantity, path/building
  occupancy, and common placement blockers.
- Building definitions can require a terrain under their footprint; farms now
  require fertile terrain.

## Implemented: Stone Slice

- The default catalog includes a data-defined quarry.
- Quarries require rocky terrain under their footprint.
- Quarries harvest finite nearby stone deposits with the same gathering
  contract used by woodcutters.
- Stone is a core transported resource with storage, palette, inspector, and
  build-menu support.
- Storehouses can store stone, and new storehouse construction requires
  delivered stone.
- Save format version 6 rejects older resource-array layouts rather than
  silently misreading them.

## Implemented: Terrain Cost Slice

- Building definitions can declare per-terrain construction material surcharges
  with `terrain.resource` keys.
- Construction sites compute their required material inventory from the actual
  placement footprint and persist those per-site requirements through save/load.
- Houses, bakeries, woodcutters, and storehouses now pay one extra stone per
  rocky footprint tile.
- The build menu reports terrain surcharges, and build-hover status reports the
  actual material cost for the currently hovered tile.

## Implemented: Clay And Generation Controls

- World generation now accepts seed and patch settings for fertile terrain,
  rocky terrain, forest groves, clay deposits, and stone deposits.
- Scenarios can disable clay or stone generation without changing save/load
  code; generated terrain and deposits remain authoritative saved state.
- Clay is a finite map resource on grass/fertile terrain and has its own map
  rendering, hover text, and placement collection overlay.
- Bricks are a transported resource with palette, inspector, storage, and
  save/load support.
- The default catalog includes a data-defined brickyard that gathers clay and
  consumes firewood to produce bricks.
- Storehouses can store bricks.

## Implemented: Logistics Inspection

- Selected buildings show active incoming and outgoing reservations.
- The inspector reports supplier and customer counts plus active goods volume
  for the selected building.
- Active routes touching the selected building list direction, resource,
  counterpart building, job state, and remaining minutes.
- Selecting a building brightens its active transport routes on the map while
  dimming unrelated transport visuals.

## Implemented: Map Visibility Slice

- The client has default, resource, and terrain map lenses, cycled with `L` or
  `Tab`.
- The resource lens brightens finite deposits while muting terrain, buildings,
  paths, and grid lines.
- The terrain lens brightens fertile, rocky, and water terrain while muting
  resource deposits and settlement overlays.
- The default view keeps all layers visible but tones finite deposits down so
  future resource types have room without turning the base map into a legend.

## Next Programming Slices

1. Scenario-level resource and terrain tuning for the village playtest.
2. Decide whether bricks gate a road upgrade, storage upgrade, or next village
   objective.
3. Better generation controls for water and broader map shapes.

## Deliberate Limits

- Loggers are not individual moving agents yet.
- Gathering has no travel-time penalty inside the collection radius.
- Forest does not regrow.
- Roads and construction clear trees immediately without labor or recovered
  timber.
- Demolition does not refund materials or regrow cleared terrain.
- Map generation is configurable, but there is not yet an external scenario
  data file format for world-generation settings.
- Stone currently gates the second storehouse in the village milestone, but
  terrain build-cost surcharges are still intentionally small.
