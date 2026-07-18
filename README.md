# VibeCity

VibeCity is an experimental simulation-first city and nation building game.

The current focus is the design of a C++ economic simulation where goods, labor,
construction, transport, and growth remain physically understandable while still
scaling from a small early-medieval settlement to very large modern economies.

See [docs/design.md](docs/design.md) for the current design document.

See [docs/prototype-simulation.md](docs/prototype-simulation.md) for the first
playable simulation target.

See [docs/self-sufficient-village-roadmap.md](docs/self-sufficient-village-roadmap.md)
for the next implementation roadmap.

See [docs/physical-resource-roadmap.md](docs/physical-resource-roadmap.md) for
the current map-resource milestone.

See [docs/playtest-checklist.md](docs/playtest-checklist.md) for the current
village playtest script and feedback checklist.

See [docs/next-playtest-roadmap.md](docs/next-playtest-roadmap.md) for the
roadmap toward the next pottery/brickmaking playtest.

See [docs/playtest-2026-06-25.md](docs/playtest-2026-06-25.md) for the first
manual playtest findings and implemented response.

See [docs/engineering-quality.md](docs/engineering-quality.md) for the current
engineering quality bar and review metrics.

See [docs/save-format.md](docs/save-format.md) for persistence behavior and
save-version policy.

See [docs/building-definitions.md](docs/building-definitions.md) for the external
building schema and extension boundary.

## Build

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
./build/vibecity_headless
./build/vibecity_headless --milestone
```

If SDL2 is available, CMake also builds the first windowed client:

```bash
./build/vibecity_client
```

If the client target is missing, check the CMake configure output. The build
first tries SDL2's CMake package and then `pkg-config sdl2`; if both fail, it
prints a warning and skips the client. Install the SDL2 development package or
set `SDL2_DIR` / `CMAKE_PREFIX_PATH` so CMake can find `SDL2Config.cmake`.

Client controls:

- `1`: select
- `2`: place path
- `R`: road upgrade mode; queue roadwork on dirt paths with connected bricks
- `X`: demolish mode
- Hold left mouse in path or road upgrade mode: drag-place or drag-upgrade paths
- Left click in demolish mode: remove a building, otherwise remove a path tile
- `Delete` / `Backspace`: demolish the selected building
- Click a construction-menu entry to select a building
- `3` through `9`: select the first seven listed buildings
- `Space`: pause or run
- `+` / `-`: simulation speed
- Arrow keys or `WASD`: pan
- Mouse wheel over the map: zoom toward the cursor
- Mouse wheel over the construction menu or inspector: scroll that panel
- `Esc`: cancel placement or demolition, then clear the current selection
- `F5`: save to `vibecity-save.vcs`
- `F9`: load from `vibecity-save.vcs`
- `P`: start the selected building's available discovery project
