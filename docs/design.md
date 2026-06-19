# VibeCity Design Document

## 1. Working Vision

VibeCity is a simulation-first city and nation building game about growing a settlement from a small early-medieval community into an enormous industrial and eventually modern economy.

The game should appeal to players who enjoy production chains, logistics, spreadsheets, and optimization. The intended audience is comfortable with complexity and should enjoy discovering bottlenecks, planning infrastructure, and making economic systems more efficient over long time spans.

The player is the ruler of the settlement or nation. Politics are out of scope for the initial game. The central challenge is economic: people need to live, work, eat, travel, build, research, and move goods through increasingly large and demanding systems.

## 2. Core Pillars

### Simulation First

The game is built around a deterministic economic simulation. Graphics and presentation exist to expose the simulation clearly, not to hide it behind decoration.

### Nothing Teleports

Goods, workers, construction materials, and services must exist somewhere, move through capacity-limited networks, and arrive after time has passed. The simulation may aggregate movement at larger scales, but production and logistics should remain physically understandable.

### Scale Through Aggregation

The game should support growth from a handful of people to millions. This requires abstraction. The simulation should not depend on individually simulating every person, meal, loaf of bread, plank of wood, or worker trip forever.

Instead, the game should allow the size of represented units to grow:

- Early village: one person dot may be one person, and one cart may be one actual cart.
- Medieval town: one household unit may represent several households, and one cart icon may represent a delivery group.
- Industrial city: one train may represent a major scheduled shipment, and one residential block may represent thousands of residents.
- Modern nation: many flows are modeled as batches, routes, districts, and networks.

### Data Is Gameplay

Tables, ledgers, graphs, overlays, inspectors, and trace tools are not secondary UI. They are core gameplay tools. The player should be able to answer:

- What is missing?
- Where is it produced?
- Where is it consumed?
- Why is this building idle?
- Which route is overloaded?
- Which input limits this production chain?
- How much reserve capacity exists?

### Gradual Complexity

The game starts simple around 500 AD or earlier. Early systems should be understandable through direct observation: farms, houses, woodcutters, basic storage, dirt paths, and local labor.

New eras should add new economic shapes, not only better numbers. Rail, electricity, fossil fuels, chemistry, mass education, electronics, and semiconductors should change what planning means.

## 3. Intended Feel

VibeCity should sit between Anno and Workers & Resources: Soviet Republic.

Anno has attractive production chains, but construction and distribution can feel too abstract. Resources often feel available across an entire island once unlocked.

Workers & Resources has strong physicality, but the pace can become slow if every construction and shipment requires too much manual setup and waiting.

VibeCity should take a middle path:

- Construction requires actual materials, workers, access, and time.
- Goods move through logistics capacity instead of appearing globally.
- Small-scale systems can be followed visually.
- Mature systems can be automated, batched, and summarized.
- The player spends most of their time planning, diagnosing, and optimizing rather than waiting.

## 4. Simulation Model

### 4.1 Time

The simulation should be tick-based and deterministic.

Open questions:

- What does one tick represent?
- Should the game use fixed ticks internally and expose days/months/years in UI?
- Should different systems update at different frequencies?

Initial recommendation:

- Use a fixed simulation tick.
- Accumulate ticks into in-game hours, days, months, and years.
- Allow slow systems such as population growth, research, and long-term maintenance to update less frequently than logistics and production.

### 4.2 Authoritative State

The core simulation owns the truth. Rendering and UI read snapshots or queried state.

Authoritative state includes:

- World map tiles and terrain.
- Buildings and construction sites.
- Inventories.
- Resource definitions.
- Population units.
- Workforce availability and demand.
- Transport networks.
- Transport jobs and routes.
- Research state.
- Statistics and history buffers.

The renderer should not own gameplay logic.

### 4.3 Resources

Resources are typed quantities stored in buildings, vehicles, depots, construction sites, districts, or other inventory holders.

Early resources may include:

- Food categories such as grain, bread, vegetables, meat, fish.
- Construction materials such as timber, stone, clay, bricks.
- Tools.
- Fuel such as firewood and charcoal.
- Labor-days or work capacity.

Later resources may include:

- Coal, iron ore, steel, cement.
- Electricity, oil, refined fuels.
- Chemicals, plastics, machine parts.
- Electronics, precision machinery, wafers, semiconductors.

Resource definitions should be data-driven.

Useful properties:

- Name and category.
- Unit type, such as items, mass, volume, energy, or abstract service units.
- Storage requirements.
- Spoilage or decay rules, if any.
- Transport requirements.
- Era availability.

### 4.4 Inventories

Inventories are explicit. A building can only consume resources that are in its local inventory or are delivered through an allowed supply mechanism.

Inventory holders include:

- Production buildings.
- Houses and residential blocks.
- Markets.
- Warehouses and depots.
- Vehicles and transport batches.
- Construction sites.
- District-level abstractions, when scale requires it.

Inventories should support reservations. A resource promised to a construction site or route should not also be consumed by another building.

### 4.5 Population

Population begins as small, visible units and becomes aggregated as scale increases.

At minimum, population units need:

- Home location.
- Food and goods demand.
- Workforce contribution.
- Workplace assignment or labor pool membership.
- Travel/access constraints.
- Satisfaction or survival state.

The initial prototype can avoid deep demographic simulation. It only needs people who live in houses, consume food, and provide labor if they can reach workplaces.

Long-term possible population dimensions:

- Age cohorts.
- Education.
- Skill level.
- Health.
- Wealth or consumption tier.
- Commuting tolerance.
- Cultural or service expectations.

### 4.6 Buildings

Buildings are the main interface between the player and the simulation.

Common building properties:

- Footprint.
- Construction requirements.
- Workers required.
- Input resources.
- Output resources.
- Cycle time or continuous production rate.
- Internal storage.
- Allowed transport modes.
- Maintenance needs.
- Research prerequisites.
- Era.
- Upgrade paths.

Most buildings should be expressible through data. Custom C++ behavior should be reserved for buildings that need special simulation logic.

Example data shape:

```toml
[building.farm]
name = "Farm"
era = "early_medieval"
footprint = [3, 3]
workers = 5
cycle_days = 7
outputs = { grain = 30 }
internal_storage = { grain = 60 }
requires_access = ["path"]
```

```toml
[building.bakery]
name = "Bakery"
era = "early_medieval"
footprint = [2, 2]
workers = 3
cycle_days = 1
inputs = { grain = 8, firewood = 1 }
outputs = { bread = 12 }
internal_storage = { grain = 24, firewood = 6, bread = 36 }
requires_access = ["path"]
```

### 4.7 Construction

Construction is a real economic process, not an instant placement action.

A construction site behaves like a temporary building with:

- Location and footprint.
- Required materials.
- Required labor.
- Required access.
- Internal delivery inventory.
- Construction progress.
- Optional tool or equipment requirements.

Example:

```text
Build bakery:
- 20 timber
- 10 stone
- 4 tools reserved during construction
- 40 labor-days
```

The site is completed when materials and labor have been supplied.

Early game construction may be slow and local. Later systems should allow construction offices, depots, logistics routes, prefabricated materials, machinery, and automation to reduce manual burden without removing physical requirements.

### 4.8 Production

Production consumes inputs, labor, and time to produce outputs.

Simple production model:

```text
If required workers are available
and inputs are present
and output storage has capacity:
    progress production cycle
    consume inputs
    produce outputs
else:
    record blocking reason
```

Buildings should expose their limiting factor in UI.

Examples:

- Missing grain.
- Missing firewood.
- Not enough workers.
- Output storage full.
- No route to warehouse.
- Maintenance too low.
- Insufficient power.

### 4.9 Logistics

Logistics move resources between inventory holders. The system should preserve physical meaning while allowing aggregation.

Core concepts:

- Transport job: move a resource quantity from source to destination.
- Route: a repeated or scheduled set of transport jobs.
- Carrier: person, cart, wagon, ship, train, truck, aircraft, pipeline, power line, or abstracted convoy.
- Network: path, road, rail, canal, sea lane, air corridor, pipe, wire.
- Capacity: how much can move per time.
- Travel time: how long movement takes.
- Reservation: source goods and carrier capacity committed to a job.

At small scale, transport jobs can map to visible units. At large scale, jobs can merge into batches or flows.

Example early transport:

```text
Move 20 bread
from Bakery A
to Market B
carrier: handcart
route: dirt path
travel time: 30 minutes
```

Example later transport:

```text
Move 800 steel
from Steel Mill District
to Rail Depot 4
carrier: scheduled freight route
route: rail corridor
travel time: 3 hours
visible unit: Train #12
```

Key rule:

Goods do not need to be individually embodied, but they must be accounted for.

### 4.10 Consumption

Consumption can happen in aggregated events.

Example:

```text
House block:
- 100 residents
- consumes 100 bread per day
- local storage: 120 bread
- requests resupply below 50 bread
```

In a village, the game may show individual households or people collecting food. In a city, the residential block consumes in batches. Both use the same accounting principles.

### 4.11 Traceability

The player should be able to inspect where goods came from and why they failed to arrive.

Trace mode should eventually answer:

- Where was this resource produced?
- Which building consumed it?
- Which route carried it?
- How long did delivery take?
- Which bottleneck limited delivery?

The simulation does not need to store full history forever. It can keep recent provenance records, sampled traces, or debug-oriented route histories.

## 5. Era Progression

Eras are not just technology tiers. Each era introduces new constraints and planning concepts.

### Early Medieval, Around 500 AD

Focus:

- Food.
- Shelter.
- Timber.
- Stone.
- Basic tools.
- Walking distance.
- Dirt paths.
- Local production.
- Small storage.

Main questions:

- Can people eat?
- Can workers reach their workplace?
- Can construction materials reach the site?
- Is enough timber being produced?

### Medieval

Focus:

- Specialized workshops.
- Mills.
- Better roads.
- Animal power.
- Masonry.
- Larger markets.
- Trade routes.

New planning pressure:

- Town centers and villages need coordinated food distribution.
- More jobs compete for the same labor pool.
- Storage and transport become visibly important.

### Early Industrial

Focus:

- Coal.
- Iron.
- Steam power.
- Larger mines.
- Workshops becoming factories.
- Canals and early rail.

New planning pressure:

- Distance becomes more manageable, but throughput becomes critical.
- Fuel supply starts to dominate industrial planning.

### Industrial

Focus:

- Steel.
- Rail networks.
- Electricity.
- Chemicals.
- Cement.
- Dense cities.
- Mass production.

New planning pressure:

- Infrastructure networks interact.
- Power and transport failures can cascade.
- Education and skilled labor may become important.

### Modern

Focus:

- Oil and refined fuels.
- Trucks.
- Advanced machinery.
- Electronics.
- Large-scale education.
- Air and sea logistics.

New planning pressure:

- Supply chains become long and fragile.
- Production depends on many specialized inputs.
- National-scale logistics matter more than local cart movement.

### Very Late Game

Focus:

- Precision manufacturing.
- Semiconductors.
- Clean rooms.
- Rare materials.
- High-skill labor.
- Advanced machinery.
- Massive energy demand.

New planning pressure:

- Extremely deep dependency chains.
- High quality requirements.
- Large capital investment.
- Tiny output quantities with enormous economic impact.

Semiconductors should feel like an almost impossible capstone, not a routine late-game unlock.

## 6. Technical Architecture

### 6.1 Language

The main game should be C++.

The core simulation should be written with performance, determinism, profiling, and long-term maintainability in mind.

### 6.2 Layering

Suggested structure:

```text
core/
  simulation/
  economy/
  logistics/
  population/
  research/
  world/
  save/

game/
  rules/
  scenarios/
  modules/
  data_loading/

render/
  simple_2d/
  camera/
  sprites/

ui/
  panels/
  inspectors/
  graphs/
  build_menu/
```

The core simulation should not depend on rendering. The renderer and UI should consume snapshots, queries, or event streams from the simulation.

### 6.3 Renderer

The first renderer should be minimal.

Acceptable visual style:

- Pixel graphics.
- Top-down 2D.
- Colored tiles.
- Simple building sprites.
- People as dots.
- Vehicles as small icons or sprites.
- Overlay-heavy presentation.

The renderer should be replaceable. A future renderer should not require rewriting the simulation.

### 6.4 UI

The UI should be dense and utilitarian.

Likely technology:

- Dear ImGui for early development.

Important UI surfaces:

- Building inspector.
- Resource ledger.
- Production chain view.
- Route list.
- Bottleneck view.
- Population/workforce view.
- Research view.
- Construction queue.
- Storage and inventory panels.
- Map overlays for access, traffic, shortages, and throughput.

### 6.5 Data-Driven Gameplay

Most gameplay content should live in data files:

- Resources.
- Buildings.
- Recipes.
- Research.
- Upgrades.
- Vehicles.
- Transport modes.
- Era definitions.

C++ modules should support custom behavior where data is insufficient.

Initial file format candidates:

- TOML.
- JSON.
- YAML.

TOML is attractive for hand-authored game data because it is readable and less punctuation-heavy than JSON.

### 6.6 Modules

Gameplay should be added through modules where possible.

For early development, a module can be a content package containing data files:

- Resource definitions.
- Building definitions.
- Production recipes.
- Research unlocks.
- Transport vehicles.
- Upgrades.
- UI labels and icons.

Most buildings should not require custom C++ code. A farm, bakery, mine, sawmill, warehouse, or basic factory should usually be defined as data plus standard simulation components.

Custom C++ behavior should be used when a module needs a genuinely new mechanic, such as:

- A new network type.
- A special production algorithm.
- A custom population behavior.
- A new kind of storage or decay rule.
- A new logistics planner.

The core simulation should expose stable capabilities to modules instead of allowing arbitrary mutation of internal state. Modules should ask the core to create resources, buildings, recipes, jobs, and systems through explicit APIs.

Dynamic native plugins can be considered later, but they are not required for the first prototype. Early modules can be loaded data packs plus built-in C++ systems. This avoids spending the first milestone on ABI and build-system complexity.

### 6.7 Performance Direction

The simulation should be designed for large scale from the beginning.

Guidelines:

- Prefer arrays and stable integer IDs over pointer-heavy object graphs.
- Keep hot simulation data compact.
- Separate frequently updated data from rarely changed definitions.
- Use deterministic fixed-step simulation.
- Aggregate when individual simulation is not necessary.
- Keep profiling easy from early prototypes.

Avoid:

- Per-resource heap allocation in hot loops.
- Simulating individual items at city scale.
- Renderer-owned gameplay state.
- Hidden global resource pools that undermine logistics.

## 7. First Prototype

The first playable prototype should prove the central loop with the smallest possible feature set.

Detailed prototype simulation decisions live in [prototype-simulation.md](prototype-simulation.md).

### Required Systems

- Tile map.
- Placeable buildings.
- Houses.
- Farms.
- Woodcutters.
- Storage.
- Construction sites.
- Dirt paths.
- Simple worker assignment.
- Simple resource inventories.
- Basic transport jobs.
- Basic UI inspectors.

### Required Resources

- People.
- Grain or generic food.
- Timber.
- Tools, possibly optional for the first pass.
- Labor-days.

### Required Loop

1. People live in houses.
2. Houses consume food.
3. Farms produce food if staffed.
4. Woodcutters produce timber if staffed.
5. Construction sites require timber and labor.
6. Paths determine whether workers and goods can reach places.
7. The UI explains blocked production and construction.

### Success Criteria

The prototype succeeds if the player can:

- Start with a tiny settlement.
- Build a new production building.
- Watch materials and labor be delivered to the site.
- See production begin after construction finishes.
- Diagnose a shortage using UI rather than guesswork.
- Understand that goods moved through the settlement instead of appearing globally.

## 8. Open Questions

- What is the exact map model: square grid, hex grid, or sparse world graph?
- How detailed should terrain be in the first prototype?
- Should workers be assigned to specific workplaces or drawn from local labor pools?
- How much should early transport be visible versus abstracted?
- What should one unit of food represent?
- Should population growth be automatic, immigration-based, or both?
- How should construction priority work?
- How should roads reserve or limit capacity?
- Should time compression be part of the first prototype?
- What is the first UI framework and rendering library combination?

## 9. Current Design Commitments

- C++ is the primary implementation language.
- The simulation core should be independent from renderer and UI.
- The first renderer should be simple top-down 2D.
- The game should start around 500 AD or earlier.
- The late game may reach modern semiconductors.
- Construction requires physical materials, labor, access, and time.
- Goods never teleport, but they may be batched or aggregated.
- The player should be able to inspect the economy through data-rich UI.
- Most buildings and resources should be data-driven.
- The first prototype should focus on houses, food, timber, construction, workers, paths, and readable bottlenecks.
