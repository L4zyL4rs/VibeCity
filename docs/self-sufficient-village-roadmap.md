# Self-Sufficient Village Roadmap

This roadmap is the next programming target. It is intentionally narrower than the full game design: no eras, politics, trade, carts, rail, ships, research, or large-scale aggregation yet.

The milestone is successful when a player can grow a small connected village into a stable settlement that produces its own bread, timber, firewood, and construction labor without relying on endless starting stock.

## Target Experience

The player should be able to:

1. Start with a few houses, residents, paths, and a limited storehouse stockpile.
2. Build farms, woodcutters, bakeries, storehouses, houses, and paths.
3. Watch construction materials and food move through visible transport jobs.
4. Grow to 25 residents.
5. Keep all houses supplied with bread for several days.
6. Understand every bottleneck from the UI or headless output.

## Acceptance Criteria

The milestone is done when all of these are true:

- A scripted scenario reaches 25 residents without external stock injections after startup.
- A scripted scenario can run for at least 5 days after reaching 25 residents with no hunger days.
- Construction of at least one house, one farm, one woodcutter, and one bakery happens through delivered materials and builder labor.
- The player can recover from at least one shortage by building or connecting the missing production chain.
- The UI exposes: population, housing capacity, food pressure, active construction blockers, and transport reservations.
- The simulation remains deterministic under repeated test runs.
- The normal test suite and SDL smoke test pass.

## Current State

Already implemented:

- Core resources: grain, bread, timber, firewood, tools.
- Buildings: house, farm, bakery, woodcutter, storehouse, construction site.
- Production recipes and storage capacities.
- Houses consuming bread daily.
- Settlement population facts: residents, housing capacity, free housing, and daily bread need.
- Food pressure facts: stored bread and days of bread remaining.
- Simple deterministic immigration when housing and bread are available.
- Growth blocker reporting for no housing, hungry houses, and low bread.
- Worker assignment by reachability.
- Dirt paths and building reachability.
- Transport jobs with reservations.
- Settlement-level logistics summary facts for active jobs, reservations, and carried goods.
- Construction sites with delivered materials and builder labor.
- Construction summary facts for active sites and blockers.
- Construction queue focus facts for the next site, remaining labor, and active builders.
- Game-layer village objective tracker with stable-fed-days history.
- Starting village that requires the player to construct the production chain.
- Command-layer scenario test that constructs a woodcutter, farm, bakery, and houses, then reaches 25 residents and stays fed for several days.
- SDL client with placement, inspector, economy summary, logistics reservation summary, objective summary, transport overlay, and drag path placement.
- SDL client shows construction queue focus and per-site construction progress.
- Catalog-driven construction menu shows each building's material and labor
  requirements and supports custom definitions.
- Cursor-centered map zoom, larger default tiles, and an interior starting
  settlement improve placement and map orientation.
- Selected-building production details expose cycle and daily capacity.
- First playtest balance requires two production chains for 25 residents.
- Farms now require fertile terrain, and placement hover text explains terrain
  blockers.
- First client splits: pixel text helpers, core palette helpers, map-view helpers, HUD helpers, inspector helpers, input handling, and client mode definitions live outside `src/client/main.cpp`.
- Versioned save/load with objective history, active logistics, reservations, construction progress, paths, and deterministic continuation.
- Tests for core production, consumption, logistics, reachability, construction, and command-layer flow.

Main gaps:

- The first playtest balance pass is implemented, but needs a second manual pass.
- Objective completion now has a HUD banner and headless summary, but there is no richer endpoint stats screen.
- Non-gating benchmark target exists with CSV output and an initial baseline history.
- Village playtest checklist exists for the first manual usability/pacing pass.
- Client responsibilities are now split enough for near-term UI work, but the inspector can still become crowded quickly.
- Icons and hover tooltips are still absent; current labels must remain
  self-explanatory without them.

## Implementation Slices

### Slice 1: Stabilize Test Infrastructure

Status: done for the current milestone.

Programming work:

- Keep tests on `VIBECITY_CHECK`, not `assert`.
- Add helper functions in tests for common village setup once tests become repetitive.
- Added a long-running deterministic scenario test for the whole milestone.

Files likely touched:

- `tests/TestSupport.hpp`
- `tests/simulation_tests.cpp`
- `tests/game_tests.cpp`

Tests:

- Existing CTest suite.
- Add one scenario test named around self-sufficient village progression.

Done when:

- Tests fail in Release builds if behavior breaks.
- The milestone scenario can express expected growth and stability without huge boilerplate.

### Slice 2: Population Capacity And Growth

Status: first pass done.

Programming work:

- Added settlement-level population facts:
  - total residents
  - total housing capacity
  - free housing
  - daily bread need
- Added a simple growth rule:
  - If houses have free capacity and bread is available, residents immigrate slowly.
  - Growth should be deterministic.
  - Growth should stop when food or housing is missing.
- Added first-pass growth cadence at the end of each day after food consumption.
- Added growth blocker reporting:
  - no housing
  - hungry house
  - low bread

Files likely touched:

- `src/core/Simulation.hpp`
- `src/core/Simulation.cpp`
- `src/game/Scenario.cpp`
- `tests/simulation_tests.cpp`
- `src/client/main.cpp` or later client inspector module

Tests:

- Residents grow into free housing when bread is stable.
- Residents do not grow when housing is full.
- Residents do not grow when bread is missing.
- Growth is deterministic for the same command sequence.

Done when:

- The player can build a new house and see population eventually occupy it.
- Growth blockers are visible in the inspector or economy summary.

Remaining follow-up:

- Decide whether immigration should require a dedicated town hall, route, or desirability later.
- Balance growth speed after the 25-resident scenario exists.

### Slice 3: Balance The Existing Chain

Status: in progress.

Programming work:

- Tune storage capacities, recipe outputs, worker slots, construction costs, and labor durations.
- Reduce starting stock enough that production matters.
- Keep early waiting times short enough for a prototype session.
- Make woodcutter output support both construction timber and bakery firewood.
- Make bakery and farm rates support at least 25 residents with a readable building count.
- Added explicit food-pressure facts so balance can assert days of bread remaining.
- Added a regression test proving farm and woodcutter output can supply a bakery without preloaded inputs.
- Reworked the starting village so the farm, woodcutter, and bakery begin unbuilt.
- Updated the milestone scenario to construct the woodcutter, farm, bakery, and two houses from limited startup materials.

Files likely touched:

- `src/core/Building.cpp`
- `src/game/Scenario.cpp`
- `tests/simulation_tests.cpp`
- `docs/prototype-simulation.md`

Tests:

- One farm/woodcutter/bakery configuration can support the target population, or the test documents the required counts.
- A bakery stalls when firewood or grain is unavailable.
- A woodcutter producing firewood and a farm producing grain unblock a bakery after logistics delivery.

Done when:

- The numbers produce a playable loop rather than instant abundance or permanent starvation.
- The self-sufficient scenario passes without hidden inventory injections after startup.
- The production-chain objectives are initially incomplete and become complete through construction.

### Slice 4: Construction And Work Priority

Status: first pass done.

Programming work:

- Added basic construction priority visibility by exposing the next queued site in placement/id order.
- Show assigned builders and remaining labor more clearly.
- Added settlement-level construction summary facts and client display for active sites, material blockers, and builder blockers.
- Added client display for next construction site, remaining labor, active builders, and per-site progress bars.
- Ensure haulers and builders do not starve production labor in surprising ways.
- Consider a simple labor reservation model if idle worker accounting becomes unclear.

Files likely touched:

- `src/core/Building.hpp`
- `src/core/Simulation.hpp`
- `src/core/Simulation.cpp`
- `src/game/GameSession.hpp`
- `src/client/main.cpp`
- `tests/simulation_tests.cpp`

Tests:

- Multiple construction sites complete in deterministic priority order.
- Sites without materials show material blockers.
- Sites with materials but no builders show labor blockers.
- Active transport jobs reduce available haulers/builders consistently.

Done when:

- The player can queue several buildings and understand which one will complete first and why.

### Slice 5: Player Objectives

Status: first pass done.

Programming work:

- Added a lightweight objective tracker in the game layer, outside the simulation core.
- First objectives:
  - have a woodcutter
  - have a farm
  - have a bakery
  - reach 15 residents
  - reach 25 residents
  - run 5 stable days at 25 residents
- Exposed active objective status in the client inspector and headless output.
- Added objective summary helpers for active objective, completed count, and all-complete state.
- Added a client milestone-complete banner and headless completion summary.

Files likely touched:

- `src/game/GameSession.hpp`
- `src/game/GameSession.cpp`
- `src/game/Scenario.hpp`
- `src/game/Scenario.cpp`
- `src/client/main.cpp`
- `tests/game_tests.cpp`

Tests:

- Objective state advances when the required building exists.
- Objective state advances when resident count reaches threshold.
- Stable-days objective resets on hunger.
- The self-sufficient village scenario completes the 25-resident and stable-days objectives.

Done when:

- The client gives the player a reason to continue and a clear milestone endpoint.

### Slice 6: UI Split And Village Readability

Status: first pass done.

Programming work:

- Split `src/client/main.cpp` before adding larger UI features.
- Added modules:
  - `Text`
  - `Palette`
  - `MapView`
  - `Hud`
  - `Inspector`
  - `InputController`
- Add housing/population rows.
- Add food pressure rows:
  - bread stored
  - daily bread need
  - days of bread remaining
- Add construction summary:
  - active sites
  - missing materials
  - waiting for builders
- Add logistics summary:
  - active jobs
  - jobs moving to pickup
  - jobs carrying goods
  - incoming, outgoing, and in-transit reservation totals
- Cull path tiles and buildings outside the visible client viewport.
- Resolve transport overlay endpoints through constant-time building lookup.
- Clip and scroll inspector content while preventing UI input from reaching the map behind it.

Files likely touched:

- `src/client/*`
- `CMakeLists.txt`
- `tests/game_tests.cpp` only if objective display depends on game state

Tests:

- Existing SDL smoke test.
- Add more game-layer tests if objective or summary data is exposed outside rendering.

Done when:

- The client can show why growth stopped without requiring terminal output.
- `src/client/main.cpp` is no longer the place every UI feature must touch.

### Slice 7: Performance Baseline

Status: first pass done.

Programming work:

- Added a non-gating benchmark executable.
- Benchmark scenarios:
  - current starting village for 30 days
  - generated village with 100 buildings for 10 days
  - logistics-heavy layout with many candidate sources
- Report:
  - ticks simulated
  - elapsed time
  - ticks per second
  - active buildings
  - active/completed transport count if useful
- Added CSV output for trend-friendly benchmark snapshots.
- Added initial benchmark history documentation.
- Added adaptive destination-rooted path distance fields for logistics requests with many viable sources.
- Improved the default-build 100-building benchmark by roughly 7x without changing scenario results.
- Added constant-time lookup for append-only dense building IDs.
- Reused destination distance fields across same-destination requests within a dispatch.
- Stored selected route duration on transport jobs instead of recomputing it after pickup.
- Improved the 100-building benchmark by another roughly 31% without changing scenario results.
- Reused distance storage and BFS frontier capacity across logistics dispatches.
- Cleared only previously visited road tiles between distance-field searches.
- Improved the 100-building benchmark from roughly 1.20 seconds to 0.57 seconds without changing scenario results.
- Added path connected-component labeling for large worker-assignment candidate sets.
- Retained pairwise reachability for small villages where a full component pass costs more.
- Improved the 100-building benchmark again to roughly 0.31-0.34 seconds without changing scenario results.

Files likely touched:

- `src/bench/main.cpp`
- `CMakeLists.txt`
- `docs/engineering-quality.md`
- `docs/benchmark-history.md`

Tests:

- Benchmark target should compile but should not gate ordinary CTest unless it is made deterministic and fast.

Done when:

- We have a repeatable number before optimizing pathfinding, data layout, or render culling.

### Slice 8: Persistence

Status: first pass done.

Programming work:

- Added a versioned little-endian binary save format.
- Save all authoritative village state:
  - paths and building placement
  - inventories and reservations
  - active transport jobs and route durations
  - production and construction progress
  - population, hunger, worker assignment, statistics, and ID generators
  - objective tracker history
- Validate checksums, enum values, dimensions, counts, placements, inventory invariants, and transport reservations before accepting a load.
- Load transactionally so an invalid file leaves the current session unchanged.
- Save through a temporary file before replacing the previous save.
- Added `F5` save and `F9` load controls for `vibecity-save.vcs`.

Tests:

- Canonical save/load/save round trip.
- Deterministic continuation from active logistics and construction state.
- Stable-days objective history survives loading.
- Corrupt and unsupported-version files are rejected.
- Failed loads do not replace the active session.

Done when:

- A village can be stopped and resumed without changing its deterministic outcome.
- Persistence failures produce a useful status and do not damage the running game.

### Slice 9: External Building Definitions

Status: first pass done.

Programming work:

- Moved built-in building costs, capacities, recipes, footprints, names, and map
  colors into strict `.vbd` files.
- Added a runtime catalog with stable string IDs and constant-time kind lookup.
- Routed simulation, placement, renderer, inspector, and save/load behavior
  through the active catalog.
- Added save catalog fingerprints so changed economic definitions fail loudly.
- Kept the compact resource enum and resource arrays for the current hot
  simulation paths.

Tests:

- A data-only custom production building runs through worker assignment and
  production.
- The custom building survives save/load through its stable ID.
- An edited catalog is rejected by save compatibility checks.
- Malformed resource IDs are rejected while loading definitions.

Done when:

- An ordinary recipe building can be added without changing C++ simulation code.
- Invalid data cannot enter a running simulation.

### Slice 10: Catalog-Driven Construction Menu

Status: first pass done.

Programming work:

- Replaced building-specific client modes with one build mode and a selected
  catalog kind.
- Added a scrollable construction panel generated from all non-internal
  definitions.
- Display construction materials, footprint, jobs or housing, and builder labor.
- Route mouse selection and dynamic `3` through `9` shortcuts through catalog
  order.
- Keep menu, map, inspector, and milestone-banner input and rendering regions
  separate.

Tests:

- Menu order excludes internal definitions.
- Material-cost text matches catalog construction costs.
- Row hit testing respects gaps and scroll position.
- Existing SDL smoke rendering remains active.

Done when:

- A newly added data-only building can be selected and placed without client
  code changes.
- The player can see required construction resources before placing a site.

### Slice 11: First Playtest Response

Status: first pass done.

Programming work:

- Changed `Esc` from quit to cancel placement and clear selection.
- Added cursor-centered map zoom and increased the default tile size.
- Moved the starting road and buildings away from the map boundary.
- Reworked inspector labels and hid inactive construction/logistics sections.
- Added selected-building cycle and daily production details.
- Reduced default simulation speed from 10 to 2 ticks per rendered frame.
- Rebalanced startup stock, construction labor, worker counts, and recipes.
- Made the woodcutter a labor-only bootstrap so timber production cannot
  deadlock behind missing timber.
- Tuned one production chain below the 25-resident requirement.

Tests:

- Escape cancellation order and zoom anchoring.
- Updated recipe and construction expectations.
- One-chain scenario stalls below the milestone.
- Two-chain scenario reaches 25 residents and remains fed for five days.

Done when:

- The milestone cannot be completed by placing one of every building.
- Building throughput and daily demand are visible before shortages occur.
- The second manual playtest can identify a real economic bottleneck.

### Slice 12: Second Playtest Response

Status: done.

Programming work:

- Added deterministic road-route reconstruction for connected buildings.
- Replaced direct building-to-building transport lines with road-following
  polylines.
- Added client-owned animation state so short deliveries remain readable after
  the authoritative transport job completes.
- Added a brief arrival linger and fade.
- Exposed and displayed the bread reserve required for the next citizen.

Tests:

- Reconstructed routes contain only contiguous road tiles and match the
  simulation's path distance.
- Completed transport jobs remain in the visual overlay long enough to read and
  are eventually removed.
- Population facts expose the next-citizen bread requirement.

Done when:

- Goods visibly follow the road from their source building to their destination.
- A one-chain village stopping at 16 residents is explained by visible bread
  supply and growth demand.

## Suggested Order

1. Run the third manual village playtest against road-following transport.
2. Verify that overlapping transport jobs remain readable.
3. Decide whether the village milestone needs a richer completion summary.
4. Use benchmark history before further logistics/pathfinding changes.

## First Concrete Next Task

Run the updated playtest from `docs/playtest-checklist.md` and verify that cargo
can be followed along roads and that the `16 / 17` growth reserve explains the
single-chain population ceiling.

## Follow-On Milestone

The self-sufficient village is now feeding into
`docs/physical-resource-roadmap.md`. Forests are the first finite map resource:
woodcutters require nearby trees, selection shows their collection area, and
depletion persists through save/load.
