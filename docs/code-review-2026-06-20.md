# Code Review 2026-06-20

Review target: `74da058 Add drag path placement`.

Review basis: [engineering-quality.md](engineering-quality.md).

## Findings

### Medium: Release builds can compile out test assertions

At the start of the review, the tests relied on `assert(...)`. In a build configured with `NDEBUG`, those checks disappear, so CTest can pass without validating behavior.

Action: replace test assertions with an always-on check helper or a small test framework. The current fix is [tests/TestSupport.hpp](/home/Lazy/Projects/VibeCity/tests/TestSupport.hpp:1).

Status: fixed in this quality pass.

### Medium: Logistics source selection repeats pathfinding work

`Simulation::find_source_for_request` evaluates every candidate source and calls path distance logic for each candidate in [src/core/Simulation.cpp](/home/Lazy/Projects/VibeCity/src/core/Simulation.cpp:618). Each distance call allocates map-sized helper vectors in [src/core/Map.cpp](/home/Lazy/Projects/VibeCity/src/core/Map.cpp:100). This is fine for the tiny prototype, but it will not scale to large settlements.

Action: before adding larger maps or hundreds of buildings, introduce a path-network cache, connected-component ids, or a one-to-many distance pass per destination request. Add a benchmark target before optimizing this.

Status: fixed on 2026-06-24. Logistics now uses an adaptive source search: small candidate sets retain pairwise pathfinding, while larger sets reuse one destination-rooted distance field. The 100-building benchmark improved by roughly 7x without changing scenario results.

### Medium: The SDL client is now too monolithic for sustained feature work

[src/client/main.cpp](/home/Lazy/Projects/VibeCity/src/client/main.cpp:1) is 1,119 lines and contains text rasterization, colors, input handling, renderer transforms, HUD, inspector, placement preview, transport overlay, and the main loop. This is still workable, but adding more tools or panels here will make regressions harder to isolate.

Action: split into client modules such as `Text`, `Palette`, `Hud`, `Inspector`, `MapView`, and `InputController` before adding the next broad UI feature.

Status: fixed. The client responsibilities now live in the proposed modules, and `src/client/main.cpp` is 202 lines.

### Medium: Client rendering scans whole data sets every frame

The map renderer scans all 128x128 tiles each frame in [src/client/main.cpp](/home/Lazy/Projects/VibeCity/src/client/main.cpp:850). Transport job drawing resolves source and destination centers by scanning all buildings per job in [src/client/main.cpp](/home/Lazy/Projects/VibeCity/src/client/main.cpp:704). This is acceptable at current scale, but it conflicts with the long-term target of very large cities.

Action: render only visible tile ranges and keep a per-frame or simulation-owned building-id lookup for overlays.

Status: fixed on 2026-06-24. The map renderer clips tile traversal and building draws to the visible viewport. Simulation building lookup is constant-time for the append-only dense ID layout, and transport overlays use that lookup directly.

### Low: No real performance benchmark exists yet

The headless demo completes a tiny two-day scenario quickly, but this is an informal smoke check, not a repeatable benchmark. There is no CMake target that exercises a larger settlement or reports ticks per second.

Action: add a non-gating `vibecity_bench` executable once logistics grows beyond prototype size.

Status: fixed. `vibecity_bench` includes CSV output and benchmark history.

### Low: Inspector layout will not scale to more resources or building details

The inspector writes rows directly downward from fixed positions in [src/client/main.cpp](/home/Lazy/Projects/VibeCity/src/client/main.cpp:616). Current content fits at 1280x800, but more resources, warnings, or job lists will overflow without scrolling or section collapse.

Action: when splitting the client, introduce a small vertical layout helper and either clipped scrolling or collapsible sections.

Status: open.

## Positive Notes

- Core simulation and SDL client are still cleanly separated by library targets and the command layer.
- Resource reservations make transport state inspectable and avoid hidden global pools.
- Current simulation tests cover production, consumption, hunger, logistics, path reachability, worker reachability, construction, and full output storage.
- Build targets already use `-Wall -Wextra -Wpedantic` on GNU/Clang.
- The client smoke test runs under SDL's dummy video driver, which catches basic startup/render-path regressions.

## Next Quality Actions

1. Add inspector scrolling or section collapse before substantially increasing inspector content.
2. Profile the next simulation bottleneck before adding more path or storage caching.
3. Keep Release CTest runs in periodic quality reviews.
