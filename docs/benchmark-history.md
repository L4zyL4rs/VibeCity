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
- `buildings`, `active_transport_jobs`, `transported`, and `constructed` are sanity checks. If these change unexpectedly, the benchmark may be measuring a gameplay change instead of a pure performance change.
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
