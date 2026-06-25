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
- paths, finite map resources, and building placement
- building inventories, capacities, and reservations
- residents, workers, hunger, production, and construction progress
- active transport jobs and selected route durations
- accumulated production, consumption, transport, and construction statistics
- village objective history
- the simulation-relevant building-definition catalog fingerprint

Saving writes a temporary file and then replaces the previous save. Loading parses and validates a complete replacement session before changing the live game. A failed load leaves the current session untouched.

## Binary Layout

Version 3 is an explicitly little-endian binary format:

1. Eight-byte `VIBECITY` magic.
2. Unsigned 32-bit format version.
3. Unsigned 64-bit payload size.
4. Unsigned 64-bit FNV-1a payload checksum.
5. Versioned payload.

The checksum detects accidental corruption; it is not a security or authenticity mechanism.

Buildings and transport-job resources use stable string IDs in the payload.
Fixed-size resource arrays remain in the documented core resource order. The
payload also stores a deterministic fingerprint of every simulation-relevant
building field.

## Validation

The loader rejects:

- unknown format versions
- invalid checksums or truncated/trailing data
- excessive file, map, building, path, or job counts
- invalid enum values, counters, IDs, or references
- overlapping paths and buildings
- invalid, duplicate, or overlapping map-resource deposits
- inventory quantities or reservations outside capacity
- transport jobs whose reservations do not match inventories
- building state that does not match the current building definitions
- a building-definition catalog fingerprint that differs from the current data

## Version Policy

There is no migration layer yet. Versions 1 and 2 are rejected. Any incompatible
payload change must increment the save version and either add an explicit
migration or reject older saves with a clear error. Simulation-relevant building
definition changes are detected by the catalog fingerprint without silently
reinterpreting the old economy.

This is intentional for the prototype: silent reinterpretation of economic state would be worse than an explicit incompatibility.
