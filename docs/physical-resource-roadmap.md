# Physical Resource Roadmap

## Goal

Make settlement placement and expansion depend on visible geography before the
economy gains many more production chains.

The milestone is successful when a player can identify a resource on the map,
place a suitable gathering building near it, watch the deposit deplete, and
understand when production stops or must move elsewhere.

## Implemented: Forest Slice

- Deterministic fixed-seed forest groves on new maps.
- Finite quantity stored on each forest tile.
- Data-driven building gathering rules with map resource, radius, and units per
  cycle.
- Woodcutters consume the nearest forest unit when a cycle starts.
- Roads and buildings clear forest on occupied tiles.
- Selected woodcutters highlight their collection area and usable forest tiles.
- Woodcutter placement preview highlights the proposed collection radius,
  shows usable forest tiles, marks trees that would be cleared by the
  footprint, and reports the reachable forest total before placement.
- The inspector reports collection radius and total forest remaining in range.
- Depleted forest state is deterministic and persisted in save files.
- Demolition and path removal free occupied tiles without refunds or terrain
  recovery. Removed buildings persist as inactive ID tombstones in saves.

## Implemented: Terrain Foundation

- New maps generate deterministic fertile and rocky terrain bands from explicit
  world-generation settings.
- Terrain is stored as authoritative simulation data and persisted in saves.
- Shallow water is supported as blocked terrain for roads and buildings.
- Map resources validate their terrain support: forest on grass/fertile tiles
  and clay on grass/fertile tiles, stone on rocky tiles.
- Stone deposits are generated on rocky terrain and rendered separately from
  forest.
- Hovering map tiles reports terrain, resource quantity, path/building
  occupancy, and common placement blockers.
- Building definitions can require a terrain under their footprint; farms now
  require fertile terrain.

## Implemented: Stone Slice

- The default catalog includes a data-defined quarry.
- Quarries require rocky terrain under their footprint.
- Quarries harvest finite nearby stone deposits with the same gathering
  contract used by woodcutters.
- Stone is a core transported resource with storage, palette, inspector, and
  build-menu support.
- Storehouses can store stone, and new storehouse construction requires
  delivered stone.
- Save format version 6 rejects older resource-array layouts rather than
  silently misreading them.

## Implemented: Terrain Cost Slice

- Building definitions can declare per-terrain construction material surcharges
  with `terrain.resource` keys.
- Construction sites compute their required material inventory from the actual
  placement footprint and persist those per-site requirements through save/load.
- Houses, bakeries, woodcutters, and storehouses now pay one extra stone per
  rocky footprint tile.
- The build menu reports terrain surcharges, and build-hover status reports the
  actual material cost for the currently hovered tile.

## Implemented: Clay And Generation Controls

- World generation now accepts seed and patch settings for fertile terrain,
  rocky terrain, forest groves, clay deposits, and stone deposits.
- Scenarios can disable clay or stone generation without changing save/load
  code; generated terrain and deposits remain authoritative saved state.
- Clay is a finite map resource on grass/fertile terrain and has its own map
  rendering, hover text, and placement collection overlay.
- Bricks are a transported resource with palette, inspector, storage, and
  save/load support.
- The default catalog includes a data-defined brickyard that gathers clay and
  consumes firewood to produce bricks.
- Storehouses can store bricks.

## Implemented: Logistics Inspection

- Selected buildings show active incoming and outgoing reservations.
- The inspector reports supplier and customer counts plus active goods volume
  for the selected building.
- Active routes touching the selected building list direction, resource,
  counterpart building, job state, and remaining minutes.
- Selecting a building brightens its active transport routes on the map while
  dimming unrelated transport visuals.

## Implemented: Map Visibility Slice

- The client has default, resource, and terrain map lenses, cycled with `L` or
  `Tab`.
- The resource lens brightens finite deposits while muting terrain, buildings,
  paths, and grid lines.
- The terrain lens brightens fertile, rocky, and water terrain while muting
  resource deposits and settlement overlays.
- The default view keeps all layers visible but tones finite deposits down so
  future resource types have room without turning the base map into a legend.

## Implemented: Starter Scenario Map Tuning

- The starting village now uses explicit world-generation settings instead of
  the generic default map.
- The reference route has authored nearby fertile land for the two farms,
  forest pockets for the two woodcutters, a rocky ridge for the quarry spur,
  a visible clay pocket for future brickyard play, and nearby surface water for
  future water-service experiments.
- The SDL client, headless scenario, starting-village benchmarks, and
  command-layer milestone tests all construct the starter scenario with the
  same generation settings.
- The route-support test asserts that the reference farm, woodcutter, quarry,
  and brickyard opportunities remain viable when generation is retuned.

## Implemented: Surface Water Generation

- World generation settings can now create shallow-water lake patches.
- World generation settings can now create one deterministic straight or bent
  shallow-water river.
- Generated water overrides earlier fertile or rocky terrain and blocks roads,
  buildings, and finite map resources.
- The starter scenario uses both a lake patch and a bent river as visible
  surface-water groundwork without adding hauled water demands yet.

## Design Direction: Goods, Services, And Maintenance

The resource model should become more detailed than a classic city-builder
input/output chain, but only where the detail creates a clear placement,
logistics, or scaling decision.

Physical goods are stored, reserved, hauled, and visible. Early examples are
grain, bread, timber, firewood, stone, clay, bricks, and tools. Later examples
can include iron ore, charcoal, iron, steel, glass, machinery, and precision
parts.

Local services affect buildings without necessarily becoming one transported
unit per consumer per day. Water is the first candidate:

- Early settlements may need access to rivers, lakes, or other surface water
  before the first wells are practical.
- Wells can provide local water service to nearby houses and workshops.
- Aqueducts, canals, cisterns, pipes, or pumps can later extend water capacity
  and support denser settlement.
- Houses, bakeries, breweries, tanneries, and similar buildings may require
  water access, but early play should not become per-house bucket logistics.

Maintenance and capability should add long-term pressure without random
production failures. Tools are the first candidate:

- Primitive gathering can work with no tools, but poorly.
- Crude tools improve low-skill extraction and farming.
- Iron tools enable or strongly improve quarrying, forestry, farming, masonry,
  and workshop output.
- Specialized tools should be required for advanced workshops such as
  blacksmiths or precision crafts.
- Tool condition should degrade deterministically through work and reduce
  productivity or block advanced buildings when not maintained.

Material progression should support a long primitive-to-medieval climb before
electricity or industry:

- Rough stone can support simple construction and foundations.
- Dressed stone can require a stonemason, tools, and labor, then gate larger
  buildings, bridges, aqueducts, and civic infrastructure.
- Clay and firewood can produce bricks for better storage, denser housing,
  improved roads, or water infrastructure.
- More specific stone or ore types can wait until regional specialization
  creates interesting map decisions.

## Design Direction: Historical Capability Ladder

Progression should be capability-based instead of point-gated. A settlement
does not unlock bronze because a research counter fills; it becomes capable of
bronze when it has the ores, fuel, furnace, molds, skilled workers, and enough
surplus labor to experiment and teach the process.

Research can later cost labor, materials, failed production batches, and time
in workshops. Knowledge may eventually belong to people or buildings: a skilled
potter, smith, mason, or teacher can spread capabilities to other workers. This
is intentionally later than the current village milestone, but the economy
should be shaped so it can support that model.

Suggested early-to-medieval ladder:

- Late Neolithic settled farming:
  - Key blockers: food surplus, water access, local timber, workable stone,
    clay, firewood, and storage.
  - Resources: grain, timber, firewood, rough stone, clay, pottery.
  - Unlocks: huts, granaries, simple storage, basic stone tools, wells or
    surface-water dependence, pottery, dirt paths.
- Pottery and controlled heat:
  - Key blockers: clay, steady fuel, potter labor, simple kiln.
  - Resources: pottery, fired clay, possibly tiles.
  - Unlocks: better food and water storage, cooking vessels, lower spoilage
    later, kiln-based production, brickmaking groundwork.
- Bricks, tiles, and lime:
  - Key blockers: clay throughput, fuel pressure, kiln capacity, labor.
  - Resources: bricks, roof tiles, lime or mortar later.
  - Unlocks: larger storehouses, better ovens and chimneys, denser housing,
    drains, improved roads, wells or cisterns, early masonry support.
- Copper working:
  - Key blockers: copper ore, charcoal, furnace temperature, specialist labor.
  - Resources: copper ore, charcoal, copper.
  - Unlocks: prestige goods, small fittings, limited tool improvements, first
    metalworking workshops.
- Bronze working:
  - Key blockers: copper, rare tin or trade access, charcoal, casting molds,
    skilled metalworkers.
  - Resources: tin, bronze, molds.
  - Unlocks: durable axes, chisels, saws, knives, fittings, weapons, improved
    carpentry and stoneworking. Bronze should be powerful but geographically or
    trade constrained.
- Iron working:
  - Key blockers: iron ore, large charcoal demand, bloomery furnace, repeated
    hammering, smith skill.
  - Resources: iron ore, bloom iron, wrought iron, iron tools.
  - Unlocks: cheaper widespread tools, better plows, nails, hinges, fittings,
    stronger everyday construction. Early iron is not automatically better than
    bronze; it becomes transformative once production scales.
- Classical and Roman-style engineering:
  - Key blockers: bricks or stone, lime mortar, dressed stone, iron tools,
    surveyor/engineer labor, organized material throughput.
  - Resources: dressed stone, lime, mortar, tiles, concrete ingredients later.
  - Unlocks: paved roads, bridges, aqueducts, baths, large warehouses, denser
    towns, drainage, durable civic infrastructure.
- Early medieval consolidation:
  - Key blockers: maintained iron tools, mills, animal power, local lordship or
    institutional labor organization if politics is ever modeled.
  - Resources: iron tools, charcoal, wool, hides, milled grain.
  - Unlocks: watermills, improved agriculture, smithies, churches,
    fortifications, larger villages and towns.

## Next Programming Slices

1. Prototype clay as pottery before treating bricks as just another
   construction material.
2. Add a kiln/potter chain that uses clay and fuel to improve storage or food
   handling.
3. Decide whether bricks gate a road upgrade, storage upgrade, water
   infrastructure, or the next village objective.
4. Prototype a tool-condition model where tooling changes productivity before
   adding more economic chains.
5. Prototype rough-stone to dressed-stone refinement for medieval
   infrastructure.
6. Better generation controls for water and broader map shapes.
7. Keep copper, bronze, and iron out of the active village milestone until
   pottery, kilns, fuel pressure, and tool maintenance have a playable base.

## Deliberate Limits

- Loggers are not individual moving agents yet.
- Gathering has no travel-time penalty inside the collection radius.
- Forest does not regrow.
- Roads and construction clear trees immediately without labor or recovered
  timber.
- Demolition does not refund materials or regrow cleared terrain.
- Map generation is configurable, but there is not yet an external scenario
  data file format for world-generation settings.
- Stone currently gates the second storehouse in the village milestone, but
  terrain build-cost surcharges are still intentionally small.
- The starter scenario has authored terrain/resource placement, but generic map
  generation is still blob-based and temporary.
- Water is planned as a local service/network first, not as a hauled daily
  input for every house.
- Generated water exists as terrain only; it does not yet satisfy wells,
  buildings, or any service coverage.
- Tool wear is planned as deterministic condition and productivity pressure,
  not random tool breakage.
- Copper, bronze, iron, and classical infrastructure are design targets, not
  near-term village-playtest scope.
- Research points should not be the primary gate for production chains.
  Capabilities should come from materials, buildings, heat, skilled labor,
  experimentation, and knowledge transfer.
