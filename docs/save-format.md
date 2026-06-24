# Save Format

VibeCity currently uses one local save slot:

```text
vibecity-save.vcs
```

Client controls:

- `F5`: save
- `F9`: load

Loading pauses the client and clears transient selection and drag state. The simulation speed setting and camera position are client preferences and are not saved.

## Guarantees

The save contains all authoritative gameplay state required for deterministic continuation:

- simulation time and ID generators
- paths and building placement
- building inventories, capacities, and reservations
- residents, workers, hunger, production, and construction progress
- active transport jobs and selected route durations
- accumulated production, consumption, transport, and construction statistics
- village objective history

Saving writes a temporary file and then replaces the previous save. Loading parses and validates a complete replacement session before changing the live game. A failed load leaves the current session untouched.

## Binary Layout

Version 1 is an explicitly little-endian binary format:

1. Eight-byte `VIBECITY` magic.
2. Unsigned 32-bit format version.
3. Unsigned 64-bit payload size.
4. Unsigned 64-bit FNV-1a payload checksum.
5. Versioned payload.

The checksum detects accidental corruption; it is not a security or authenticity mechanism.

## Validation

The loader rejects:

- unknown format versions
- invalid checksums or truncated/trailing data
- excessive file, map, building, path, or job counts
- invalid enum values, counters, IDs, or references
- overlapping paths and buildings
- inventory quantities or reservations outside capacity
- transport jobs whose reservations do not match inventories
- building state that does not match the current building definitions

## Version Policy

There is no migration layer yet. Any incompatible payload or building-definition change must increment the save version and either add an explicit migration or reject older saves with a clear error.

This is intentional for the prototype: silent reinterpretation of economic state would be worse than an explicit incompatibility.
