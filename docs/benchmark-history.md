# Benchmark History

Benchmarks are non-gating. Use them to spot large performance regressions before changing logistics, pathfinding, construction, or storage behavior.

Run the benchmark from the repository root:

```bash
./build/vibecity_bench
./build/vibecity_bench --csv
```

The human output is for quick inspection. The CSV output is for pasting into this file or comparing in a spreadsheet.

## Reading Results

- `milliseconds` is wall-clock time for the scenario on the current machine.
- `ticks_per_second` is useful for rough comparisons across scenarios on the same machine.
- `buildings`, `active_transport_jobs`, `transported`, `map_resource_tiles`,
  `map_resource_quantity`, and `constructed` are sanity checks. Older baselines
  predate the map-resource columns. If these change unexpectedly, the benchmark
  may be measuring a gameplay change instead of a pure performance change.
- Local timings are noisy. Compare repeated runs or medians before treating a small difference as meaningful.

## Baselines

### 2026-06-21 Local Default Build

Command:

```bash
./build/vibecity_bench --csv
```

| Case | Ticks | Milliseconds | Ticks/s | Buildings | Active Jobs | Transported | Constructed |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| starting village 30d | 43,200 | 12.92 | 3,343,365 | 4 | 0 | 24 | 0 |
| construction village 30d | 43,200 | 95.60 | 451,865 | 9 | 0 | 2,121 | 5 |
| 100 buildings 10d | 14,400 | 13,696.43 | 1,051 | 100 | 40 | 72,458 | 0 |

CSV:

```csv
case,ticks,milliseconds,ticks_per_second,buildings,active_transport_jobs,transported,constructed
starting village 30d,43200,12.92,3343365,4,0,24,0
construction village 30d,43200,95.60,451865,9,0,2121,5
100 buildings 10d,14400,13696.43,1051,100,40,72458,0
```

Notes:

- The 100-building case is intentionally logistics-heavy and currently slow. It is useful as a pathfinding/logistics regression detector, not as a target for immediate optimization.
- This baseline was recorded before dedicated path-network caching or request batching.

### 2026-06-24 Adaptive Path Distance Fields

Logistics source selection now counts viable sources before pathfinding. Small candidate sets retain pairwise shortest-path searches, while larger sets build one destination-rooted distance field and reuse it for every candidate.

Representative default-build sample:

| Case | Ticks | Milliseconds | Ticks/s | Buildings | Active Jobs | Transported | Constructed |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| starting village 30d | 43,200 | 13.34 | 3,237,663 | 4 | 0 | 24 | 0 |
| construction village 30d | 43,200 | 84.34 | 512,214 | 9 | 0 | 2,121 | 5 |
| 100 buildings 10d | 14,400 | 1,885.39 | 7,638 | 100 | 40 | 72,458 | 0 |

Notes:

- A second post-change sample put the 100-building case at 1,760.77 ms. This is a 7.1-7.6x improvement over the 13,441.72 ms pre-change sample.
- Starting and construction village timings remain within existing run-to-run noise.
- All scenario sanity fields are unchanged, so this comparison measures pathfinding work rather than a gameplay behavior change.

### 2026-06-24 Dispatch Route Reuse

Transport jobs now keep the delivery duration selected during dispatch instead of running pathfinding again when pickup completes. Consecutive resource requests for the same destination also reuse one dispatch-local distance field.

Representative default-build sample:

| Case | Ticks | Milliseconds | Ticks/s | Buildings | Active Jobs | Transported | Constructed |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| starting village 30d | 43,200 | 12.74 | 3,390,798 | 4 | 0 | 24 | 0 |
| construction village 30d | 43,200 | 61.32 | 704,505 | 9 | 0 | 2,121 | 5 |
| 100 buildings 10d | 14,400 | 1,199.52 | 12,005 | 100 | 40 | 72,458 | 0 |

Notes:

- A second sequential sample put the 100-building case at 1,212.83 ms.
- Compared with the previous 1,743.49 ms sample, the large case is roughly 31% faster.
- `perf` confirmed that pathfinding no longer runs during the pickup-to-carrying transition.
- Scenario sanity fields remain unchanged.

### 2026-06-24 Reusable Path Workspace

The logistics distance field now retains its distance storage and BFS frontier between dispatches. Resetting clears only tiles visited by the previous search instead of allocating and filling a full map-sized vector.

Representative default-build sample:

| Case | Ticks | Milliseconds | Ticks/s | Buildings | Active Jobs | Transported | Constructed |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| starting village 30d | 43,200 | 12.63 | 3,419,783 | 4 | 0 | 24 | 0 |
| construction village 30d | 43,200 | 60.89 | 709,528 | 9 | 0 | 2,121 | 5 |
| 100 buildings 10d | 14,400 | 567.46 | 25,376 | 100 | 40 | 72,458 | 0 |

Notes:

- A second sample put the 100-building case at 569.96 ms.
- Compared with the previous 1,199.52 ms sample, the large case is roughly 2.1x faster.
- Compared with the original 13,441.72 ms baseline, the large case is roughly 24x faster.
- A regression test repopulates one field from disconnected road components and verifies that old distances are cleared.
- Scenario sanity fields remain unchanged.

### 2026-06-24 Adaptive Worker Connectivity

Worker assignment now labels path connected components once when the number of house/workplace pairs is large. Small settlements retain pairwise reachability checks because a full map pass costs more than a few short searches.

Representative default-build sample:

| Case | Ticks | Milliseconds | Ticks/s | Buildings | Active Jobs | Transported | Constructed |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| starting village 30d | 43,200 | 12.56 | 3,438,184 | 4 | 0 | 24 | 0 |
| construction village 30d | 43,200 | 64.03 | 674,697 | 9 | 0 | 2,121 | 5 |
| 100 buildings 10d | 14,400 | 336.26 | 42,824 | 100 | 40 | 72,458 | 0 |

Notes:

- Another sample put the 100-building case at 305.44 ms.
- Compared with the previous 567.46 ms sample, the large case is roughly 1.7-1.9x faster.
- Compared with the original 13,441.72 ms baseline, the large case is roughly 40x faster.
- `perf` confirmed that worker assignment no longer contributes pairwise path searches in the large scenario.
- Small scenario timings remain within their prior run-to-run range, and all sanity fields are unchanged.

### 2026-06-25 External Building Catalog

Building definitions now load from strict data files. Runtime kinds use a
constant-time catalog lookup, while each building instance caches a derived
five-bit source eligibility mask for the logistics candidate hot path.

Representative default-build samples:

| Case | Ticks | Milliseconds | Ticks/s | Buildings | Active Jobs | Transported | Constructed |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| starting village 30d | 43,200 | 11.66 | 3,706,029 | 4 | 0 | 24 | 0 |
| construction village 30d | 43,200 | 50.10 | 862,222 | 9 | 0 | 2,121 | 5 |
| 100 buildings 10d | 14,400 | 316.18 | 45,543 | 100 | 40 | 72,458 | 0 |

Notes:

- A second large-case sample measured 305.06 ms.
- An initial implementation repeatedly inspected catalog policy and recipes
  during source selection and measured roughly 380-394 ms.
- Caching only the derived source mask removed that regression without placing
  definition ownership or behavior back into hardcoded building switches.
- Scenario sanity fields remain unchanged.

### 2026-06-25 First Playtest Balance

The first manual playtest led to lower worker requirements, tighter recipes,
longer construction, and substantially more active logistics in the generated
case. These results establish a new gameplay baseline and are not directly
comparable to the previous timings because the sanity counters changed.

Representative default-build sample:

| Case | Ticks | Milliseconds | Ticks/s | Buildings | Active Jobs | Transported | Constructed |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| starting village 30d | 43,200 | 12.00 | 3,598,794 | 4 | 0 | 60 | 0 |
| construction village 30d | 43,200 | 53.99 | 800,109 | 9 | 0 | 1,759 | 5 |
| 100 buildings 10d | 14,400 | 759.44 | 18,961 | 100 | 79 | 101,420 | 0 |

Notes:

- The large case now moves roughly 40% more goods and ends with nearly twice as
  many active jobs.
- Lower farm and woodcutter staffing leaves more workers available for hauling,
  increasing the amount of logistics work performed per simulated day.
- Future performance comparisons should use these counters as the baseline
  unless the generated scenario is deliberately frozen independently of balance.

### 2026-06-25 Finite Forest Resources

New simulations now contain deterministic finite forest deposits. Woodcutters
query and consume nearby forest at recipe boundaries. Map resources are stored
in a parallel compact tile array so pathfinding retains its previous hot tile
layout.

Representative Release-build sample:

| Case | Ticks | Milliseconds | Ticks/s | Buildings | Active Jobs | Transported | Resource Tiles | Resource Quantity | Constructed |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| starting village 30d | 43,200 | 0.96 | 44,838,069 | 4 | 0 | 60 | 4,669 | 28,014 | 0 |
| construction village 30d | 43,200 | 2.51 | 17,227,814 | 9 | 0 | 1,759 | 4,647 | 27,881 | 5 |
| 100 buildings 10d | 14,400 | 57.39 | 250,898 | 100 | 76 | 100,685 | 4,385 | 26,278 | 0 |

Notes:

- Repeated large-case samples measured roughly 57-63 ms.
- Transport and active-job counters changed because woodcutters now depend on
  finite local supply; this is a new gameplay baseline.
- The benchmark now records remaining map-resource tiles and quantity as sanity
  counters.
- An initial combined gameplay/resource tile expanded the pathfinding working
  set and measured roughly 61-70 ms. Separating resource state restored the hot
  path layout.

### 2026-06-29 Terrain and Stone Foundation

Terrain is now authoritative simulation state and new maps generate deterministic
fertile and rocky terrain. Stone deposits are generated on rocky tiles using the
same finite map-resource storage as forests. This sample is from the local
default build, so compare sanity counters first and treat timings as
machine/build-mode context rather than a release-speed baseline.

Representative default-build sample:

| Case | Ticks | Milliseconds | Ticks/s | Buildings | Active Jobs | Transported | Resource Tiles | Resource Quantity | Constructed |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| starting village 30d | 43,200 | 16.44 | 2,627,609 | 4 | 0 | 60 | 5,320 | 34,156 | 0 |
| construction village 30d | 43,200 | 97.00 | 445,370 | 9 | 0 | 1,759 | 5,298 | 34,023 | 5 |
| 100 buildings 10d | 14,400 | 956.88 | 15,049 | 100 | 77 | 100,435 | 5,004 | 32,052 | 0 |

Notes:

- Resource tiles and resource quantity increased because stone deposits now
  count alongside forests.
- The large scenario's active-job and transported counters remain close to the
  finite-forest gameplay baseline; stone is not consumed by default buildings
  yet.

### 2026-06-29 Fertile Farm Placement

Farms now require fertile terrain under their full footprint. The generated
benchmark scenario still prefers its old grid positions, but farms scan for a
valid fertile fallback when the preferred tile is unsuitable. This changes the
large-case layout and sanity counters, so compare it as a gameplay/layout
baseline rather than a pure performance change.

Representative default-build sample:

| Case | Ticks | Milliseconds | Ticks/s | Buildings | Active Jobs | Transported | Resource Tiles | Resource Quantity | Constructed |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| starting village 30d | 43,200 | 17.33 | 2,493,225 | 4 | 0 | 60 | 5,320 | 34,156 | 0 |
| construction village 30d | 43,200 | 98.02 | 440,733 | 9 | 0 | 1,759 | 5,298 | 34,023 | 5 |
| 100 buildings 10d | 14,400 | 2,000.09 | 7,200 | 100 | 45 | 91,184 | 5,020 | 32,169 | 0 |

Notes:

- Starting and scripted construction scenarios keep their sanity counters.
- The generated 100-building case now places farms only on fertile terrain, so
  active jobs and transported totals are intentionally different.

### 2026-06-30 Quarry And Stone Resource

Stone is now a core resource and the default catalog includes a data-defined
quarry that harvests rocky stone deposits. Storehouse construction requires
stone, but the current benchmark scenarios do not construct extra storehouses or
place quarries, so the sanity counters match the fertile-farm baseline.

Representative default-build sample:

| Case | Ticks | Milliseconds | Ticks/s | Buildings | Active Jobs | Transported | Resource Tiles | Resource Quantity | Constructed |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| starting village 30d | 43,200 | 18.17 | 2,378,198 | 4 | 0 | 60 | 5,320 | 34,156 | 0 |
| construction village 30d | 43,200 | 94.07 | 459,239 | 9 | 0 | 1,759 | 5,298 | 34,023 | 5 |
| 100 buildings 10d | 14,400 | 1,975.40 | 7,290 | 100 | 45 | 91,184 | 5,020 | 32,169 | 0 |

Notes:

- The large-case counters are unchanged from the fertile-farm placement sample.
- The new quarry behavior is covered by core tests rather than this benchmark.

### 2026-07-01 Stone-Gated Village Objective

The village objective tracker now requires a quarry and two completed
storehouses, and the scripted milestone route constructs a second storehouse
using delivered stone. The benchmark scenarios still do not run that full
milestone route, so their sanity counters remain unchanged.

Representative default-build sample:

| Case | Ticks | Milliseconds | Ticks/s | Buildings | Active Jobs | Transported | Resource Tiles | Resource Quantity | Constructed |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| starting village 30d | 43,200 | 21.65 | 1,995,604 | 4 | 0 | 60 | 5,320 | 34,156 | 0 |
| construction village 30d | 43,200 | 94.59 | 456,713 | 9 | 0 | 1,759 | 5,298 | 34,023 | 5 |
| 100 buildings 10d | 14,400 | 1,987.61 | 7,245 | 100 | 45 | 91,184 | 5,020 | 32,169 | 0 |

Notes:

- Starting stock now includes five tools instead of two, but the benchmark CSV
  does not report stored tools.
- Use the command-layer milestone test for quarry/storehouse route coverage.

### 2026-07-10 Terrain Build-Cost Modifiers

Construction sites now store position-dependent material requirements, and
generic village buildings can add a small stone surcharge on rocky footprint
tiles. The scripted benchmark routes do not place those construction sites on
rocky terrain, so their sanity counters remain unchanged.

Representative default-build sample:

| Case | Ticks | Milliseconds | Ticks/s | Buildings | Active Jobs | Transported | Resource Tiles | Resource Quantity | Constructed |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| starting village 30d | 43,200 | 17.96 | 2,405,442 | 4 | 0 | 60 | 5,320 | 34,156 | 0 |
| construction village 30d | 43,200 | 91.64 | 471,428 | 9 | 0 | 1,759 | 5,298 | 34,023 | 5 |
| 100 buildings 10d | 14,400 | 1,907.73 | 7,548 | 100 | 45 | 91,184 | 5,020 | 32,169 | 0 |

Notes:

- The reference milestone still completes with all eight objectives.
- Save/load validation now accepts construction-site material capacities derived
  from the site's terrain footprint rather than only the target definition's
  base cost.

### 2026-07-10 Clay And World Generation Settings

Clay deposits now generate on grass/fertile terrain, bricks are a core
transported resource, and the default catalog includes a brickyard that gathers
clay and consumes firewood. New simulations can also pass explicit world
generation settings for seed, terrain patch layout, forest/clay patch layout,
and stone generation.

Representative default-build sample:

| Case | Ticks | Milliseconds | Ticks/s | Buildings | Active Jobs | Transported | Resource Tiles | Resource Quantity | Constructed |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| starting village 30d | 43,200 | 19.14 | 2,256,568 | 4 | 0 | 60 | 6,036 | 41,316 | 0 |
| construction village 30d | 43,200 | 99.98 | 432,073 | 9 | 0 | 1,759 | 6,014 | 41,183 | 5 |
| 100 buildings 10d | 14,400 | 2,053.00 | 7,014 | 100 | 45 | 91,184 | 5,702 | 38,989 | 0 |

Notes:

- Resource tile and quantity counters increased because clay deposits now count
  alongside forests and stone.
- The benchmark scenarios do not construct brickyards yet, so transported
  totals remain unchanged.

### 2026-07-15 Starter Scenario Map Tuning

The starting-village scenario now uses authored world-generation settings:
nearby fertile land for the reference farms, forest pockets for the reference
woodcutters, a rocky ridge for the quarry spur, and a visible clay pocket for
future brickyard play. The generated 100-building benchmark still uses the
generic default map.

Representative default-build sample:

| Case | Ticks | Milliseconds | Ticks/s | Buildings | Active Jobs | Transported | Resource Tiles | Resource Quantity | Constructed |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| starting village 30d | 43,200 | 19.03 | 2,270,552 | 4 | 0 | 60 | 492 | 3,366 | 0 |
| construction village 30d | 43,200 | 94.74 | 455,996 | 9 | 0 | 1,759 | 474 | 3,257 | 5 |
| 100 buildings 10d | 14,400 | 1,942.95 | 7,411 | 100 | 45 | 91,184 | 5,702 | 38,989 | 0 |

Notes:

- Starting/construction village resource counters dropped because those cases
  now use the smaller authored starter map resource layout.
- The reference milestone still completes with all eight objectives on the
  tuned map.
