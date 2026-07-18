# Next Playtest Roadmap

This roadmap targets the next manual playtest after the current village
milestone. The goal is not a new era yet. The goal is to make the existing
village loop physically legible through pottery, brickmaking, and the first road
upgrade.

## Target Playtest Loop

The player should be able to:

1. Build and stabilize the existing bread/timber/stone village.
2. Discover pottery from a house near clay.
3. Build a potter and produce pottery from firewood and clay.
4. Discover brickmaking from a potter.
5. Build a brickyard and produce bricks from firewood and clay.
6. Use bricks to improve roads and understand how rain pressure changes.

## Required Before Playtest

- Construction clarity: selected construction sites must show the target,
  required materials, delivered materials, incoming deliveries, missing
  materials, builders, and labor left.
- Roadwork: paved roads are queued as persistent roadwork jobs, consume one
  connected brick when started, and complete through builder labor. The
  remaining simplification is that bricks do not yet travel to the road tile as
  explicit logistics jobs.
- Discovery UI: replace the selected-host `P` shortcut with a small project UI
  before adding many more projects.
- Playtest checklist: update the reference route so it includes pottery,
  brickmaking, brickyard construction, brick production, and road paving.

## Backlog Until After Playtest

- Flour and milling.
- Grain spoilage or storage-quality simulation.
- Tool condition and tool-maintenance productivity pressure.
- Water services for houses, bakeries, wells, or aqueducts.
- Copper, bronze, iron, and broader metallurgy.

## Storage Research Notes

Storage pressure should be grounded in post-harvest loss mechanisms, not just
generic decay.

- FAO material describes traditional grain or legume storage losses from insects
  as highly variable, but gives 10-30 percent over a full storage season as a
  plausible range under traditional tropical conditions.
- Moisture is central. FAO notes that fungal deterioration is mainly caused by
  excessive water and gives approximate 70 percent relative-humidity equilibrium
  moisture values such as wheat/maize 13.5 percent, paddy rice 14 percent, and
  sorghum/millet 16 percent.
- For gameplay, this suggests storage classes instead of per-item decay:
  open/ground storage is emergency storage with high losses, basic house storage
  is mediocre, raised granaries improve bulk grain, and sealed pottery/clay-lined
  bins improve small-stock protection from pests and moisture.

Sources:

- FAO, Prevention of post-harvest food losses, storage pests:
  https://www.fao.org/4/x5039e/x5039E02.HTM
- FAO, Prevention of post-harvest food losses, moisture and safe storage:
  https://www.fao.org/4/x0039e/X0039E01.htm

## Milling Research Notes

Flour should eventually exist because intact grain stores better than milled
flour, and milling is a major labor and infrastructure step.

- Whole grain kernels store comparatively well when dry and protected.
- Milled whole-wheat flour is more vulnerable because milling exposes germ and
  bran oils and enzymes; extension guidance commonly warns that whole wheat
  flour can go rancid quickly at room temperature.
- For gameplay, early bread can remain `grain + firewood + labor -> bread`.
  Later, a milling step can split this into `grain -> flour -> bread`, with
  hand milling as a labor bottleneck and mills as major upgrades.

Sources:

- Utah State University Extension, Storing Wheat:
  https://extension.usu.edu/preserve-the-harvest/research/storing-wheat
- Utah State University Extension, Gearing Up for Holiday Baking:
  https://extension.usu.edu/news_sections/home_family_and_food/gearing-up-for-holiday-baking
