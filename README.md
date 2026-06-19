# VibeCity

VibeCity is an experimental simulation-first city and nation building game.

The current focus is the design of a C++ economic simulation where goods, labor,
construction, transport, and growth remain physically understandable while still
scaling from a small early-medieval settlement to very large modern economies.

See [docs/design.md](docs/design.md) for the current design document.

See [docs/prototype-simulation.md](docs/prototype-simulation.md) for the first
playable simulation target.

See [docs/engineering-quality.md](docs/engineering-quality.md) for the current
engineering quality bar and review metrics.

## Build

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
./build/vibecity_headless
```

If SDL2 is available, CMake also builds the first windowed client:

```bash
./build/vibecity_client
```

Client controls:

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
