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
- Depleted forest state is deterministic and persisted in save format version 5.
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
  forest, but no default building consumes stone yet.
- Hovering map tiles reports terrain, resource quantity, path/building
  occupancy, and common placement blockers.
- Building definitions can require a terrain under their footprint; farms now
  require fertile terrain.

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
2. Quarry and brickyard chains that make construction materials geographic.
3. Clay deposits using the same terrain/resource contract.

## Deliberate Limits

- Loggers are not individual moving agents yet.
- Gathering has no travel-time penalty inside the collection radius.
- Forest does not regrow.
- Roads and construction clear trees immediately without labor or recovered
  timber.
- Demolition does not refund materials or regrow cleared terrain.
- Map generation is deterministic but not yet configurable by seed or scenario.
- Stone is visible and saved, but it has no economy chain in the default
  village yet.
