# Engineering Quality Guidelines

This project is still a prototype, but the code should already preserve the properties that matter for a large simulation game: deterministic behavior, inspectable state, clear module boundaries, and measurable performance.

## Required Checks

Run these before committing behavior changes:

```bash
cmake --build build
ctest --test-dir build --output-on-failure
SDL_VIDEODRIVER=dummy ./build/vibecity_client --smoke-test
```

If SDL2 is unavailable, document that the client smoke check could not run.

For client UI changes, capture a smoke-render screenshot and inspect it for
text overlap, missing markers, and unreadable contrast:

```bash
SDL_VIDEODRIVER=dummy SDL_RENDER_DRIVER=software ./build/vibecity_client --screenshot /tmp/vibecity-ui-smoke.bmp
```

For performance-sensitive changes, also run the non-gating benchmark:

```bash
./build/vibecity_bench
./build/vibecity_bench --csv
```

Record meaningful benchmark snapshots in `docs/benchmark-history.md`.

## Quality Bar

- Simulation logic stays deterministic. No wall-clock time, hidden randomness, renderer state, or input state may affect core results.
- The core simulation stays independent from SDL and client code. The client reads simulation state and submits commands through the game layer.
- Resource movement must be explicit. Goods should be in inventory, reserved for transport, in a transport job, or consumed/produced by a documented rule.
- Every new mechanic needs at least one focused simulation or command-layer test. UI-only changes need at minimum the SDL smoke test.
- Building definitions should remain data-shaped: adding a building should usually mean adding definition data plus narrow behavior only when required.
- Public APIs should expose game facts, not renderer conveniences. Renderer-specific transforms belong in the client.
- Hot simulation code should avoid per-entity heap allocation in long-term designs. Temporary allocation is acceptable in the prototype only when visible in review notes.
- Errors shown to players should point at the bottleneck: no workers, no source, no hauler, no capacity, or missing material.

## Metrics To Track

Snapshot from 2026-06-20:

| Metric | Current | Quality Signal |
| --- | ---: | --- |
| Total repository text lines counted | 5,237 | Small enough for manual review |
| Core simulation implementation | 844 lines | Split pressure is building |
| SDL client implementation | 1,119 lines | Needs decomposition before many more UI features |
| Automated CTest targets | 3 | Core, game, and client smoke are covered |
| Non-gating benchmark targets | 1 | CSV output and baseline history exist for logistics/pathfinding work |
| Named simulation scenarios | 13 | Good coverage for current prototype mechanics |
| Named command-layer scenarios | 3 | Thin but appropriate for current command layer |
| Informal headless 2-day demo runtime | ~0.02s | Useful only as a rough local smell test |

The line counts are not quality by themselves, but they are useful warning lights:

- Any single implementation file over 600 lines needs an explicit split plan.
- Any single implementation file over 1,000 lines should not receive large new feature work until it is split.
- Any function over roughly 80 lines should be reviewed for extraction unless it is a simple table or switch.
- Any hot-path loop with nested scans over buildings, resources, jobs, or map tiles needs either a test-sized justification or a follow-up performance note.

## Review Checklist

Use this checklist for periodic self-review:

- Correctness: Are resources conserved? Are reservations released on failure? Are state transitions tested?
- Determinism: Does the same command sequence produce the same result every run?
- Observability: Can the player or headless output explain why something is blocked?
- Scale: Which loops are O(buildings), O(requests * buildings), or O(map tiles)? Which allocate?
- Modularity: Can a new building or UI view be added without editing unrelated systems?
- Test reliability: Do tests still fail in release builds? Are smoke tests actually running?
- Client boundary: Is the renderer only presenting state, or has gameplay leaked into UI code?
- Data growth: Are enums, resource arrays, and building definitions still easy to extend?
