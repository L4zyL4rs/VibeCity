# Village Playtest Checklist

Use this checklist for the second village playtest. The goal is to verify that
the first playtest fixes made throughput, shortages, pacing, and map interaction
understandable.

## Build And Run

```bash
cmake --build build
ctest --test-dir build --output-on-failure
./build/vibecity_client
```

If the client fails to start, run the headless and benchmark checks instead and report the output:

```bash
./build/vibecity_headless
./build/vibecity_bench --csv
```

## Current Controls

- `1`: select
- `2`: place path
- Hold left mouse in path mode: drag-place paths
- Click a construction-menu entry to select a building
- `3` through `9`: select the first seven listed buildings
- `Space`: pause or run
- `+` / `-`: simulation speed
- Arrow keys or `WASD`: pan
- Mouse wheel over the map: zoom toward the cursor
- Mouse wheel over the construction menu or inspector: scroll that panel
- `Esc`: cancel placement, then clear the current selection
- `F5`: save to `vibecity-save.vcs`
- `F9`: load from `vibecity-save.vcs`

## Playtest Goal

Starting state:

- 3 full houses.
- 1 storehouse.
- Limited bread, timber, and two tools.
- No farm, woodcutter, or bakery built yet.

Milestone:

- Build enough production to exceed the village's daily bread demand.
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

The scripted scenario succeeds with this construction sequence on the existing path line:

1. Woodcutter near `x=10, y=21`.
2. Farm near `x=13, y=21`.
3. Bakery near `x=16, y=21`.
4. House near `x=19, y=21`.
5. House near `x=21, y=21`.
6. Second woodcutter near `x=23, y=21`.
7. Second farm near `x=26, y=21`.
8. Second bakery near `x=29, y=21`.

One chain cannot support 25 residents. The selected-building production section
and settlement daily bread need should make the shortfall visible.

The exact coordinates are not meant to be final gameplay advice. They are a control route matching the automated milestone test.

## Feedback To Capture

When I ask for detailed input, please report these points:

- First confusion: what did you expect, what did you click, and what did the UI show?
- Bottlenecks: could you tell whether the problem was food, materials, workers, builders, paths, or haulers?
- Logistics: could you understand what goods were reserved, moving, or already stored?
- Construction: could you tell which site was next and whether builders were working?
- Pacing: which speed did you use, and where did waiting feel too slow or too fast?
- Controls: any accidental mode switches, placement mistakes, camera issues, or unclear buttons.
- Visuals: any text overlap, unreadable rows, missing contrast, or map markers that were too subtle.
- Endpoint: did the objective summary and milestone-complete banner make the goal clear?

Helpful evidence:

- Current day and time.
- Selected building summary from the window title or inspector.
- Screenshot if the UI looked wrong.
- Short note of the last 3-5 actions before the issue happened.

## Known Rough Edges To Ignore For Now

- The graphics are intentionally placeholder.
- There is no undo or bulldoze.
- The UI does not yet have mouse tooltips.
- The game has no costs for placing construction sites beyond delivered materials.
- Workers do not visibly commute yet; reachability and assignment are simulated.
- The 100-building benchmark is still a prototype stress case, despite the first pathfinding optimization.
