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
