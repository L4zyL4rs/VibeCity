# Village Playtest Checklist

Use this checklist when the first manual playtest gate starts. The goal is not to prove you can optimize the village; the goal is to find the first places where the game state is unclear, too slow, or misleading.

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
- `3`: place farm construction site
- `4`: place woodcutter construction site
- `5`: place bakery construction site
- `6`: place house construction site
- `7`: place storehouse construction site
- `Space`: pause or run
- `+` / `-`: simulation speed
- Arrow keys or `WASD`: pan
- Mouse wheel over inspector: scroll details

## Playtest Goal

Starting state:

- 3 full houses.
- 1 storehouse.
- Limited bread, timber, and one tool.
- No farm, woodcutter, or bakery built yet.

Milestone:

- Build the missing production chain.
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

1. Woodcutter near `x=10, y=1`.
2. Farm near `x=13, y=1`.
3. Bakery near `x=16, y=1`.
4. House near `x=19, y=1`.
5. House near `x=21, y=1`.

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
