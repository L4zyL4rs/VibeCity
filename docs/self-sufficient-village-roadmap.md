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
- Construction sites with delivered materials and builder labor.
- Construction summary facts for active sites and blockers.
- Game-layer village objective tracker with stable-fed-days history.
- Starting village that requires the player to construct the production chain.
- Command-layer scenario test that constructs a woodcutter, farm, bakery, and houses, then reaches 25 residents and stays fed for several days.
- SDL client with placement, inspector, economy summary, objective summary, transport overlay, and drag path placement.
- First client split: pixel text and color helpers live outside `src/client/main.cpp`.
- Tests for core production, consumption, logistics, reachability, construction, and command-layer flow.

Main gaps:

- Current balance is still provisional, but starting stock is now limited enough that production construction matters.
- Objective completion exists, but there is no dedicated victory screen or richer endpoint feedback.
- No proper benchmark target.
- `src/client/main.cpp` needs splitting before much more UI work.

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

Programming work:

- Add basic construction priority or predictable construction ordering.
- Show assigned builders and remaining labor more clearly.
- Added settlement-level construction summary facts and client display for active sites, material blockers, and builder blockers.
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

Status: started.

Programming work:

- Split `src/client/main.cpp` before adding larger UI features.
- Suggested modules:
  - `Text` (started)
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

Programming work:

- Add a non-gating benchmark executable.
- Benchmark scenarios:
  - current starting village for 30 days
  - generated village with 100 buildings for 10 days
  - logistics-heavy layout with many candidate sources
- Report:
  - ticks simulated
  - elapsed time
  - active buildings
  - active/completed transport count if useful

Files likely touched:

- `src/bench/main.cpp`
- `CMakeLists.txt`
- `docs/engineering-quality.md`

Tests:

- Benchmark target should compile but should not gate ordinary CTest unless it is made deterministic and fast.

Done when:

- We have a repeatable number before optimizing pathfinding, data layout, or render culling.

## Suggested Order

1. Finish balancing production and construction around 25 residents.
2. Split the client UI before adding more panels.
3. Add a benchmark before scaling the simulation further.

## First Concrete Next Task

Split `src/client/main.cpp` before adding more village readability UI. The gameplay loop now needs more on-screen explanation, and the client file is large enough that another panel or interaction pass should not land there monolithically.
