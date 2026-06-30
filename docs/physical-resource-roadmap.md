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

- New maps generate deterministic fertile and rocky terrain bands.
- Terrain is stored as authoritative simulation data and persisted in saves.
- Shallow water is supported as blocked terrain for roads and buildings, though
  it is not generated in the starter map yet.
- Map resources validate their terrain support: forest on grass/fertile tiles
  and stone on rocky tiles.
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

## Implemented: Logistics Inspection

- Selected buildings show active incoming and outgoing reservations.
- The inspector reports supplier and customer counts plus active goods volume
  for the selected building.
- Active routes touching the selected building list direction, resource,
  counterpart building, job state, and remaining minutes.
- Selecting a building brightens its active transport routes on the map while
  dimming unrelated transport visuals.

## Next Programming Slices

1. Terrain build-cost modifiers rather than treating every buildable suitable
   tile identically.
2. Make stone matter in at least one normal village expansion route, not only
   optional storehouse construction.
3. Clay deposits and brickyards using the same terrain/resource contract.

## Deliberate Limits

- Loggers are not individual moving agents yet.
- Gathering has no travel-time penalty inside the collection radius.
- Forest does not regrow.
- Roads and construction clear trees immediately without labor or recovered
  timber.
- Demolition does not refund materials or regrow cleared terrain.
- Map generation is deterministic but not yet configurable by seed or scenario.
- Stone exists as a quarry chain, but the current 25-resident route does not
  force the player to build one yet.
