# Village Playtest Checklist

Use this checklist for the next village playtest. The goal is to verify that
throughput, shortages, pacing, road-following transport, and the new
quarry/stone layer are understandable.

## Build And Run

```bash
cmake --build build
ctest --test-dir build --output-on-failure
./build/vibecity_client
```

If the client fails to start, run the headless and benchmark checks instead and report the output:

```bash
./build/vibecity_headless
./build/vibecity_headless --milestone
./build/vibecity_bench --csv
```

The `--milestone` headless run queues the reference route below and advances it
for 30 days.

## Current Controls

- `1`: select
- `2`: place path
- `X`: demolish mode
- Hold left mouse in path mode: drag-place paths
- Left click in demolish mode: remove a building, otherwise remove a path tile
- `Delete` / `Backspace`: demolish the selected building
- Click a construction-menu entry to select a building
- `3` through `9`: select the first seven listed buildings
- `Space`: pause or run
- `+` / `-`: simulation speed
- Arrow keys or `WASD`: pan
- Mouse wheel over the map: zoom toward the cursor
- `L` or `Tab`: cycle default, resource, and terrain map lenses
- Mouse wheel over the construction menu or inspector: scroll that panel
- `Esc`: cancel placement or demolition, then clear the current selection
- `F5`: save to `vibecity-save.vcs`
- `F9`: load from `vibecity-save.vcs`

## Playtest Goal

Starting state:

- 3 full houses.
- 1 storehouse.
- Limited bread, timber, and five tools.
- No farm, woodcutter, quarry, or bakery built yet.

Milestone:

- Build enough production to exceed the village's daily bread demand.
- Build a quarry and use its stone to construct a second storehouse.
- Reach 25 residents.
- Keep the village fed for 5 stable days.
- Confirm that the milestone-complete banner appears.

## Preferred First Run

Try this without reading the reference route first:

1. Start the client.
2. Look at the inspector and HUD for one minute without placing anything.
3. Decide what you think the village needs next.
4. Build and connect whatever seems necessary.
5. Use speed controls when waiting becomes boring.
6. Stop when either the milestone banner appears or you feel stuck for more than 5 real minutes.

Record the first moment where you are unsure what to do. That moment matters more than whether you eventually solve the village.

## Reference Route

Use this only after the preferred first run, or if you want to verify that the simulation can complete.

The scripted scenario succeeds with this construction sequence:

1. Woodcutter near `x=10, y=21`.
2. Farm near `x=13, y=21`.
3. Bakery near `x=16, y=21`.
4. House near `x=19, y=21`.
5. House near `x=21, y=21`.
6. Second woodcutter near `x=23, y=21`.
7. Second farm near `x=26, y=21`.
8. Second bakery near `x=29, y=21`.
9. Build a vertical path spur at `x=20` from `y=12` through `y=19`.
10. Quarry near `x=19, y=10`.
11. Extend the main road from `x=31` through `x=34`.
12. Second storehouse near `x=32, y=21`.

One chain cannot support 25 residents. The selected-building production section
and settlement daily bread need should make the shortfall visible.

The quarry and second storehouse are required for the current milestone. The
food chain should stabilize first; the stone leg is an expansion check after
the second bakery is underway.

The exact coordinates are not meant to be final gameplay advice. They are a
control route matching the automated milestone test. The starter map is now
authored so fertile land, forest, stone, clay, and surface water are
deliberately near this route while still requiring placement choices.

## Feedback To Capture

When I ask for detailed input, please report these points:

- First confusion: what did you expect, what did you click, and what did the UI show?
- Bottlenecks: could you tell whether the problem was food, materials, workers, builders, paths, or haulers?
- Logistics: when selecting a source or destination building, could you
  understand what goods were reserved, moving, or already stored?
- Transport visibility: could you follow a cargo marker along the road from its
  source building to its destination, including short trips, and does selecting
  one endpoint make the relevant routes stand out?
- Growth ceiling: when one bakery settles at 16 residents, does the displayed
  bread reserve make the missing surplus clear?
- Forests: when selecting a woodcutter, is its collection radius and remaining
  local forest supply understandable?
- Stone/quarries: can you find rocky stone deposits, place a quarry only on
  suitable terrain, and understand its collection radius and remaining local
  stone supply?
- Clay/brickyards: can you identify clay deposits, place a brickyard near
  usable clay, understand why it is locked before brickmaking, and understand
  that it also needs firewood to make bricks once unlocked?
- Pottery/granaries: can you identify that potters need clay and firewood, and
  that potters/granaries are locked before pottery is discovered?
- Terrain/resources: can you distinguish normal grass, fertile terrain, rocky
  terrain, shallow water, forests, stone deposits, and clay deposits without
  guessing?
- Surface water: does the nearby water read as a meaningful future constraint,
  and is it clear that it currently blocks construction without yet supplying
  houses or workshops?
- Map lenses: does the resource lens make deposits easier to find, and does
  the terrain lens make fertile/rocky regions easier to read without hiding
  roads and buildings too much?
- Hover info: does the status line make the tile terrain, resource amount,
  path/building occupancy, and placement blocker clear enough?
- Farm placement: is it clear that farms require fertile terrain, and are
  fertile patches easy enough to find without being trivial?
- Placement: when hovering a woodcutter, quarry, or brickyard before placement,
  is the collection radius, reachable resource count, and footprint-cleared
  resource marker clear?
- Depletion: after several production cycles, can you identify which nearby
  forest, stone, or clay tiles were consumed?
- Construction: could you tell which site was next and whether builders were working?
- Pacing: which speed did you use, and where did waiting feel too slow or too fast?
- Controls: any accidental mode switches, placement mistakes, camera issues, or unclear buttons.
- Demolition: could you correct placement/path mistakes, and was no-refund/no-regrowth behavior unsurprising?
- Visuals: any text overlap, unreadable rows, missing contrast, or map markers that were too subtle.
- Endpoint: did the objective summary and milestone-complete banner make the goal clear?

Helpful evidence:

- Current day and time.
- Selected building summary from the window title or inspector.
- Screenshot if the UI looked wrong.
- Short note of the last 3-5 actions before the issue happened.

## Known Rough Edges To Ignore For Now

- The graphics are intentionally placeholder.
- There is still no undo.
- The UI does not yet have mouse tooltips.
- The game has no costs for placing construction sites beyond delivered materials.
- Workers do not visibly commute yet; reachability and assignment are simulated.
- The 100-building benchmark is still a prototype stress case, despite the first pathfinding optimization.
