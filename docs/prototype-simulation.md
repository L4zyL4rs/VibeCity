# Prototype Simulation Model

This document pins down the first playable simulation target. It is not the final game design. Values here are provisional and should be changed whenever testing shows that they make the prototype less understandable or less fun.

The purpose of the prototype is to prove that a tiny settlement can build, feed, work, transport, and diagnose shortages without using hidden global resource pools.

## 1. Prototype Goal

The first prototype should answer one question:

Can the player understand and influence a small physical economy where houses, workers, food, timber, stone, construction sites, and paths interact?

The prototype should support this loop:

1. Houses contain people.
2. People consume bread.
3. Farms produce grain.
4. Bakeries turn grain and firewood into bread.
5. Woodcutters produce timber and firewood.
6. Quarries produce stone from rocky deposits.
7. Construction sites require timber, optional stone/tools, and labor.
8. Goods are carried by visible transport jobs.
9. Paths determine reachability and travel time.
10. Inspectors explain why something is blocked.

## 2. Scale

### 2.1 World Scale

Use a square tile grid for the first prototype.

Initial target:

- Map size: 128 x 128 tiles.
- Tile shape: square.
- Tile meaning: approximately 10 meters per tile, but gameplay clarity wins over realism.
- Rendering: one tile maps cleanly to pixel-art cells.

Square grid is the simplest choice for early pathfinding, building footprints, and UI inspection. Hexes can be revisited later if the square grid creates design problems.

### 2.2 Simulation Unit Scale

At prototype scale, represented units can remain small:

- One resident is one resident.
- One worker slot is one worker.
- One visible hauler is one person carrying goods.
- One cart, once added, is one actual cart.

This is intentionally not the final city-scale model. Later stages will aggregate residents, haulers, and shipments, but the prototype should start at human scale so the physical economy is easy to inspect.

## 3. Time

### 3.1 Fixed Tick

Use a deterministic fixed simulation tick.

Prototype decision:

- One simulation tick = one in-game minute.
- 60 ticks = one in-game hour.
- 1,440 ticks = one in-game day.

Reasoning:

- Minute ticks make walking, hauling, and delivery delays legible.
- Daily consumption and production can still be scheduled at day boundaries.
- Time compression can run many ticks per rendered frame while the map is small.

### 3.2 Scheduled Updates

Not every system needs to perform full work every minute.

Suggested frequencies:

- Movement: every tick.
- Production progress: every tick for active buildings.
- Construction progress: every tick for active sites.
- Logistics dispatch: every 10 ticks or when a new urgent request appears.
- Worker assignment: daily at dawn, plus on demand when buildings are placed or disabled.
- Household consumption: daily.
- Statistics rollup: hourly and daily.

### 3.3 Determinism

The same initial state and command sequence should produce the same result.

Guidelines:

- Use integer ticks.
- Use stable integer IDs.
- Use deterministic tie-breaking, usually lower ID first.
- Avoid floating-point state in authoritative simulation where practical.
- Keep random events out of the first prototype.

## 4. Map

### 4.1 Tiles

Each tile stores:

- Terrain type.
- Occupant building ID, if any.
- Path or road type, if any.
- Passability flags.
- Optional finite natural resource marker.

Prototype terrain types:

- Grass.
- Fertile soil.
- Rocky ground.
- Shallow water, blocked for now.

Prototype map resources:

- Forest on grass or fertile terrain.
- Stone on rocky terrain.
- Clay on grass or fertile terrain.

Forests can be harvested by woodcutters. Stone can be harvested by quarries.
Clay can be harvested by potters and brickyards, then fired into pottery or
bricks with firewood.

### 4.2 Buildings And Access

Buildings occupy rectangular footprints.

For the prototype, a building is connected to the transport network if at least one access edge touches a path tile.

Open question:

- Should every building have explicit door/access tiles, or is any adjacent path enough for the prototype?

Initial recommendation:

- Use any adjacent path for the first prototype.
- Add explicit access points later when road layout becomes more important.

### 4.3 Pathfinding

Use path-network pathfinding for the first prototype:

- Goods and workers move along path tiles.
- Buildings connect to adjacent path tiles.
- Off-road movement is not needed initially.

BFS is sufficient at 128 x 128. Logistics uses pairwise searches for small source sets and a reusable destination-rooted distance field for larger source sets. The field retains scratch storage but is repopulated from current map topology, avoiding persistent cache invalidation. More persistent path-network caching should wait until benchmarks justify the added complexity.

## 5. Resources

### 5.1 Resource Units

Use integer quantities for resources.

Prototype resources:

- Grain.
- Bread.
- Timber.
- Firewood.
- Stone.
- Tools.
- Labor.

Labor is not stored like timber. It is produced by assigned workers over time and applied to construction or production.

### 5.2 Initial Meanings

The exact real-world unit is less important than consistency.

Prototype meanings:

- 1 bread feeds 1 resident for 1 day.
- 1 grain is one bakery input unit.
- 1 timber is one construction material unit.
- 1 firewood is one fuel unit.
- 1 tool is a durable or semi-durable construction/work efficiency item.

Tools should probably exist in starting stock but not be deeply simulated in the first pass. If tools create too much early complexity, they can be removed from the first implementation and restored in prototype phase 2.

### 5.3 Storage

Resources are stored only in explicit inventory holders:

- Houses.
- Production buildings.
- Storehouses.
- Construction sites.
- Haulers in transit.

No global settlement inventory exists.

Inventories need:

- Current quantity by resource.
- Maximum quantity by resource or total capacity.
- Reserved outgoing quantity.
- Reserved incoming capacity.

Reservations prevent double-spending resources that are already promised to a construction site, building, or household.

Transport jobs keep the route duration chosen at dispatch. A job therefore does not rerun pathfinding when it changes from going to pickup to carrying goods.

## 6. Population And Labor

### 6.1 Houses

Houses contain residents and a small local food inventory.

Prototype house:

- Capacity: 5 residents.
- Workers: 3 adult workers, or a simpler fixed worker count.
- Food storage: 10 bread.
- Resupply threshold: request bread when below 5.

For the first implementation, children, age cohorts, health, education, and wealth are out of scope.

### 6.2 Worker Assignment

The first prototype should use explicit worker slots, not a fully abstract labor pool.

Each workplace has required worker slots. At dawn, the simulation assigns available workers from reachable houses to workplaces by priority.

Assignment rules:

- A worker must live in a reachable house.
- A worker can hold one workplace assignment at a time.
- Workplace priority determines assignment order.
- Lower building ID breaks ties.
- If no worker is available, the workplace records "not enough workers" as a blocking reason.

Workers do not need to be physically simulated during every commute in the first pass. The important prototype rule is reachability. Visible worker dots can be added as a presentation layer once assignment works.

Reachability uses pairwise path checks in small settlements and one connected-component map for larger house/workplace candidate sets. This changes only how connectivity is calculated, not deterministic assignment order.

### 6.3 Labor For Construction

Construction sites request builder workers.

Assigned builders generate labor progress while they are working:

- 1 builder contributes 1 labor-minute per simulation minute.
- 1 labor-day = 720 labor-minutes, assuming a 12-hour workday.

The prototype may simplify this and let builders contribute continuously. The UI should still display progress in readable units.

## 7. Buildings

### 7.1 Prototype Building Set

Minimum set:

- House.
- Farm.
- Bakery.
- Woodcutter.
- Storehouse.
- Construction site.
- Dirt path.

Optional if time permits:

- Town hall or starting depot.
- Well.
- Stonecutter.

The town hall can be implemented as a storehouse plus starting marker, not as a special political building.

### 7.2 Building Definitions

Buildings should already be data-driven in the prototype.

Each definition should include:

- ID.
- Name.
- Footprint.
- Construction materials.
- Construction labor.
- Worker slots.
- Input resources.
- Output resources.
- Production cycle duration.
- Internal storage.
- Access requirement.

Example:

```toml
[building.woodcutter]
name = "Woodcutter"
footprint = [2, 2]
construction = { labor_days = 5 }
workers = 1
cycle_minutes = 360
outputs = { timber = 1, firewood = 3 }
internal_storage = { timber = 20, firewood = 20 }
requires_access = ["path"]
```

```toml
[building.bakery]
name = "Bakery"
footprint = [2, 2]
construction = { timber = 14, tools = 1, labor_days = 8 }
workers = 2
cycle_minutes = 360
inputs = { grain = 6, firewood = 2 }
outputs = { bread = 4 }
internal_storage = { grain = 24, firewood = 12, bread = 18 }
requires_access = ["path"]
```

### 7.3 Blocking Reasons

Every active building or construction site should expose a primary blocking reason.

Prototype reasons:

- No path access.
- Not enough workers.
- Missing input resource.
- Output storage full.
- Missing construction material.
- Waiting for hauler.
- Waiting for builder labor.
- No reachable source.
- Destination storage full.

This is critical. The prototype should not require the player to infer hidden state from silence.

## 8. Production

### 8.1 Recipe Execution

Use cycle-based production.

Recommended first implementation:

1. Check workers, inputs, output capacity, and access.
2. Reserve and consume inputs at cycle start.
3. Reserve output capacity at cycle start.
4. Accumulate progress each tick.
5. Add outputs at cycle completion.
6. Clear output reservation.

Consuming inputs at cycle start avoids awkward cases where the same grain appears available for multiple bakeries during a cycle.

### 8.2 Farms

Farms are historically seasonal, but the first prototype should not begin with full seasonal agriculture.

Prototype farm:

- Requires workers.
- Produces grain on a fixed cycle.
- May require adjacency to grass.
- Does not require seed, tools, fertility, weather, or seasons yet.

Seasonality should be added later because it is a major gameplay system, not a tiny detail.

## 9. Logistics

### 9.1 Transport Jobs

Goods move through transport jobs.

A transport job contains:

- Resource type.
- Quantity.
- Source inventory.
- Destination inventory.
- Carrier type.
- Carrier capacity.
- Path.
- Current path position.
- State.

Job states:

- Waiting for carrier.
- Going to pickup.
- Loading.
- Carrying goods.
- Unloading.
- Complete.
- Failed.

### 9.2 Haulers

Prototype haulers are workers assigned to logistics.

Initial rule:

- A hauler carries 5 units by hand.
- A hauler moves 1 path tile per minute.
- Loading and unloading are instant for the first prototype.

This is intentionally simple. Carts, warehouses with staff, loading times, road capacity, and vehicle ownership can come later.

### 9.3 Job Creation

Inventory holders can issue requests.

Examples:

- House requests bread below threshold.
- Bakery requests grain and firewood below target stock.
- Construction site requests required timber and tools.
- Storehouse requests nothing by default but can serve as a source.

Dispatch process:

1. Collect unsatisfied requests.
2. Find reachable sources with unreserved stock.
3. Reserve source quantity and destination capacity.
4. Assign available hauler capacity.
5. Create visible transport job.

Tie-breaking should be deterministic:

1. Higher request priority.
2. Older request.
3. Shorter path.
4. Lower source ID.
5. Lower destination ID.

### 9.4 Routes

Recurring routes are out of scope for the first prototype.

However, the transport job model should not prevent routes later. A future route can simply create scheduled jobs using the same movement and reservation rules.

## 10. Construction

### 10.1 Placement

When the player places a building, it becomes a construction site.

The construction site stores:

- Target building definition.
- Footprint.
- Required resources.
- Delivered resources.
- Required labor.
- Completed labor.
- Assigned builders.
- Blocking reason.

### 10.2 Completion

Construction completes when:

- All required materials are delivered.
- Required labor has been applied.
- The site still has valid access and footprint.

On completion:

- Remove construction site state.
- Create the real building instance.
- Preserve relevant inventory if applicable.
- Mark worker assignment and logistics as dirty.

### 10.3 Construction Priority

The player should be able to set construction priority eventually.

Prototype default:

- Sites are served by placement order.
- Lower construction site ID wins ties.

## 11. Consumption

Households consume bread once per day.

Prototype rule:

- At midnight, each resident consumes 1 bread from the house inventory.
- If bread is missing, record a hunger counter for the house.
- Starvation and death are not required in the first pass.

The point of the prototype is to create food pressure and visible logistics, not to fully simulate demographics.

## 12. Update Order

The prototype simulation should use a stable update order.

Per tick:

1. Apply queued player commands.
2. Rebuild dirty path connectivity, if needed.
3. Recompute worker assignments if the assignment state is dirty or it is dawn.
4. Generate or update resource requests.
5. Dispatch logistics jobs on schedule or if urgent requests exist.
6. Move active transport jobs.
7. Run active production cycles.
8. Apply construction labor.
9. Run scheduled consumption and daily events at midnight.
10. Record statistics and recent events.

This order is allowed to change, but it should remain explicit. Silent ordering bugs are common in economic simulations.

## 13. User Interface Requirements

The prototype UI should expose simulation truth before it tries to look polished.

Required inspector data:

- Building name and state.
- Inventory and reservations.
- Worker slots filled and missing.
- Current recipe progress.
- Construction progress.
- Blocking reason.
- Pending requests.
- Active incoming and outgoing transport jobs.

Required overlays:

- Path connectivity.
- Buildings with missing workers.
- Buildings missing inputs.
- Construction sites waiting for materials.
- Houses low on food.

Required ledgers:

- Total produced per day by resource.
- Total consumed per day by resource.
- Current stored quantity by resource.
- Active transport jobs.

## 14. Prototype Start Scenario

Initial settlement:

- 3 houses.
- 15 residents.
- 1 storehouse or town hall.
- Starting stock: 90 bread, 8 timber, 5 tools.
- A few trees near the settlement.
- Enough path tiles to connect starting buildings.

First player objective:

1. Build a woodcutter.
2. Build a farm.
3. Build a bakery.
4. Compare daily bread capacity with settlement demand.
5. Expand to two production chains.
6. Build a quarry near rocky stone.
7. Build a second storehouse using delivered stone.
8. Keep 25 residents supplied with bread.
9. Diagnose at least one shortage through the UI.

## 15. What Is Explicitly Out Of Scope

For the first prototype, do not implement:

- Money.
- Trade with outside settlements.
- Politics.
- Military.
- Seasons.
- Disease.
- Education.
- Skill levels.
- Happiness.
- Pollution.
- Crime.
- Detailed road traffic capacity.
- Rail, ships, or carts.
- District-scale aggregation.
- Multi-city simulation.

These are not rejected features. They are postponed so the first prototype can prove the economic core.

## 16. Questions To Revisit After Testing

- Is one minute per tick too fine or too coarse?
- Does hand-carried logistics feel understandable or tedious?
- Should hauling consume the same labor pool as production?
- Are houses too small as simulation units?
- Should the farm produce grain continuously or seasonally as soon as possible?
- Is bread a good first consumed good, or should houses initially consume generic food?
- Does the player need a market building between bakery and houses?
- Should construction require tools immediately?
- Do explicit worker assignments create useful constraints or excessive micromanagement?
- How soon should recurring routes replace ad hoc transport jobs?
