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
- The inspector reports collection radius and total forest remaining in range.
- Depleted forest state is deterministic and persisted in save format version 3.

## Next Programming Slices

1. Placement preview for gathering buildings, including in-range resource totals
   before construction is confirmed.
2. Demolition and road removal, with explicit decisions about whether cleared
   terrain can recover.
3. Terrain suitability and build-cost modifiers rather than treating every
   non-forest tile identically.
4. Stone and clay deposits using the same map-resource contract.
5. Quarry and brickyard chains that make construction materials geographic.
6. A selected-building logistics inspection mode for suppliers, customers,
   active routes, and flow volumes.

## Deliberate Limits

- Loggers are not individual moving agents yet.
- Gathering has no travel-time penalty inside the collection radius.
- Forest does not regrow.
- Roads and construction clear trees immediately without labor or recovered
  timber.
- Map generation is deterministic but not yet configurable by seed or scenario.
