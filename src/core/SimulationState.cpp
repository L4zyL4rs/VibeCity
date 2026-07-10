#include "core/Simulation.hpp"

#include <limits>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

namespace vibecity {
namespace {

template <typename Enum>
bool enum_less_than(Enum value, Enum limit)
{
    return static_cast<std::underlying_type_t<Enum>>(value)
        < static_cast<std::underlying_type_t<Enum>>(limit);
}

template <typename Enum>
bool enum_at_most(Enum value, Enum maximum)
{
    return static_cast<std::underlying_type_t<Enum>>(value)
        <= static_cast<std::underlying_type_t<Enum>>(maximum);
}

bool inventory_is_empty(const Inventory& inventory)
{
    const auto state = inventory.state();
    return state.quantities == empty_resources()
        && state.capacities == empty_resources()
        && state.reserved_outgoing == empty_resources()
        && state.reserved_incoming == empty_resources();
}

}

SimulationState Simulation::state() const
{
    return SimulationState{
        .map_width = map_.width(),
        .map_height = map_.height(),
        .paths = map_.path_positions(),
        .terrain = map_.terrain_tiles(),
        .map_resources = map_.map_resource_deposits(),
        .buildings = buildings_,
        .transport_jobs = transport_jobs_,
        .next_building_id = next_building_id_,
        .next_transport_job_id = next_transport_job_id_,
        .next_auto_building_x = next_auto_building_x_,
        .current_tick = current_tick_,
        .worker_assignment_dirty = worker_assignment_dirty_,
        .idle_workers = idle_workers_,
        .stats = stats_
    };
}

Simulation Simulation::from_state(
    SimulationState state,
    std::shared_ptr<const BuildingCatalog> catalog)
{
    if (catalog == nullptr) {
        throw std::invalid_argument("saved simulation requires a building catalog");
    }
    constexpr auto max_map_dimension = 4'096;
    constexpr auto max_map_tiles = 4'194'304;
    if (state.map_width <= 0
        || state.map_height <= 0
        || state.map_width > max_map_dimension
        || state.map_height > max_map_dimension
        || static_cast<std::int64_t>(state.map_width) * state.map_height > max_map_tiles) {
        throw std::invalid_argument("invalid saved map dimensions");
    }
    if (state.current_tick < 0
        || state.next_building_id == 0
        || state.next_transport_job_id == 0
        || state.next_auto_building_x < 0
        || state.idle_workers < 0
        || state.stats.constructed_buildings < 0) {
        throw std::invalid_argument("invalid saved simulation counters");
    }
    for (std::size_t index = 0; index < resource_count; ++index) {
        if (state.stats.produced[index] < 0
            || state.stats.consumed[index] < 0
            || state.stats.transported[index] < 0) {
            throw std::invalid_argument("invalid saved resource statistics");
        }
    }

    auto restored_map = TileMap{state.map_width, state.map_height};
    for (const auto terrain : state.terrain) {
        if (!enum_less_than(terrain.terrain, TerrainId::Count)
            || terrain.terrain == TerrainId::Grass
            || restored_map.terrain_at(terrain.position) != TerrainId::Grass
            || !restored_map.set_terrain(terrain.position, terrain.terrain)) {
            throw std::invalid_argument("invalid or duplicate saved terrain");
        }
    }
    for (const auto path : state.paths) {
        if (restored_map.has_path(path) || !restored_map.add_path(path)) {
            throw std::invalid_argument("invalid or duplicate saved path");
        }
    }
    for (const auto& deposit : state.map_resources) {
        if (!enum_less_than(deposit.resource, MapResourceId::Count)
            || deposit.quantity <= 0
            || deposit.quantity > map_resource_capacity(deposit.resource)
            || restored_map.map_resource_quantity(deposit.position) > 0
            || !restored_map.set_map_resource(
                deposit.position,
                deposit.resource,
                deposit.quantity)) {
            throw std::invalid_argument("invalid or duplicate saved map resource");
        }
    }

    for (std::size_t index = 0; index < state.buildings.size(); ++index) {
        auto& building = state.buildings[index];
        const auto expected_id = static_cast<BuildingId>(index + 1);
        if (building.id != expected_id) {
            throw std::invalid_argument("saved building IDs must be dense");
        }
        if (!enum_at_most(building.blocking_reason, BlockingReason::NoNearbyMapResource)) {
            throw std::invalid_argument("invalid saved building enum");
        }
        if (building.residents < 0
            || building.assigned_workers < 0
            || building.assigned_builders < 0
            || building.recipe_progress < 0
            || building.hunger_days < 0
            || building.construction_labor_required < 0
            || building.construction_labor_completed < 0) {
            throw std::invalid_argument("invalid saved building counters");
        }

        if (!building.active) {
            if (building.position.has_value()
                || !inventory_is_empty(building.inventory)
                || building.residents != 0
                || building.assigned_workers != 0
                || building.assigned_builders != 0
                || building.recipe_progress != 0
                || building.hunger_days != 0
                || building.construction_target.has_value()
                || building.construction_labor_required != 0
                || building.construction_labor_completed != 0
                || building.blocking_reason != BlockingReason::None) {
                throw std::invalid_argument("invalid saved demolished building");
            }
            building.source_mask = 0;
            continue;
        }

        if (!building.position.has_value()) {
            throw std::invalid_argument("saved active building must be positioned");
        }

        const auto& definition = catalog->definition(building.kind);
        building.source_mask = definition.source_mask;
        if (building.residents > definition.resident_capacity
            || building.assigned_workers > definition.worker_slots) {
            throw std::invalid_argument("saved building occupancy exceeds capacity");
        }
        if (definition.internal_construction_site != building.construction_target.has_value()) {
            throw std::invalid_argument("invalid saved construction target");
        }
        if (building.construction_target.has_value()
            && catalog->definition(*building.construction_target).internal_construction_site) {
            throw std::invalid_argument("invalid saved construction target");
        }

        const auto& state_definition = definition.internal_construction_site
            ? catalog->definition(*building.construction_target)
            : definition;
        if (restored_map.footprint_has_map_resource(
                *building.position,
                state_definition.footprint)) {
            throw std::invalid_argument("saved building overlaps map resource");
        }
        if (state_definition.required_terrain.has_value()
            && !restored_map.footprint_matches_terrain(
                *building.position,
                state_definition.footprint,
                *state_definition.required_terrain)) {
            throw std::invalid_argument("saved building violates terrain placement");
        }
        const auto expected_storage = definition.internal_construction_site
            ? construction_materials_for_footprint(
                state_definition,
                restored_map,
                *building.position)
            : state_definition.storage;
        if (building.inventory.capacities() != expected_storage) {
            throw std::invalid_argument("saved inventory capacities do not match building definition");
        }
        if (definition.internal_construction_site) {
            if (building.assigned_builders > prototype_builders_per_site
                || building.construction_labor_required != state_definition.construction_labor_minutes
                || building.construction_labor_completed > building.construction_labor_required
                || building.recipe_progress != 0) {
                throw std::invalid_argument("invalid saved construction progress");
            }
        } else if (building.assigned_builders != 0
            || building.construction_labor_required != 0
            || building.construction_labor_completed != 0
            || (!definition.recipe.has_value() && building.recipe_progress != 0)
            || (definition.recipe.has_value()
                && building.recipe_progress >= definition.recipe->cycle_minutes)) {
            throw std::invalid_argument("invalid saved building progress");
        }

        if (!restored_map.place_building(
                building.id,
                *building.position,
                state_definition.footprint)) {
            throw std::invalid_argument("invalid saved building placement");
        }
    }

    if (state.buildings.size() >= std::numeric_limits<BuildingId>::max()
        || state.next_building_id != static_cast<BuildingId>(state.buildings.size() + 1)) {
        throw std::invalid_argument("invalid next saved building ID");
    }

    auto expected_outgoing = std::vector<ResourceArray>(state.buildings.size());
    auto expected_incoming = std::vector<ResourceArray>(state.buildings.size());
    auto previous_job_id = TransportJobId{0};
    for (const auto& job : state.transport_jobs) {
        if (job.id == 0
            || job.id <= previous_job_id
            || job.quantity <= 0
            || job.source == 0
            || job.destination == 0
            || job.source > state.buildings.size()
            || job.destination > state.buildings.size()
            || !state.buildings[static_cast<std::size_t>(job.source - 1)].active
            || !state.buildings[static_cast<std::size_t>(job.destination - 1)].active
            || job.source == job.destination
            || !enum_less_than(job.resource, ResourceId::Count)
            || (job.state != TransportJobState::GoingToPickup
                && job.state != TransportJobState::CarryingGoods)
            || job.ticks_remaining < 0
            || job.ticks_remaining > job.leg_ticks_total
            || job.leg_ticks_total <= 0
            || job.delivery_ticks <= 0
            || (job.state == TransportJobState::GoingToPickup
                && job.leg_ticks_total != prototype_transport_leg_minutes)
            || (job.state == TransportJobState::CarryingGoods
                && job.leg_ticks_total != job.delivery_ticks)) {
            throw std::invalid_argument("invalid saved transport job");
        }

        const auto source_index = static_cast<std::size_t>(job.source - 1);
        const auto destination_index = static_cast<std::size_t>(job.destination - 1);
        const auto resource = resource_index(job.resource);
        if (job.quantity > std::numeric_limits<Quantity>::max() - expected_incoming[destination_index][resource]
            || (job.state == TransportJobState::GoingToPickup
                && job.quantity > std::numeric_limits<Quantity>::max() - expected_outgoing[source_index][resource])) {
            throw std::invalid_argument("saved transport reservations overflow");
        }

        expected_incoming[destination_index][resource] += job.quantity;
        if (job.state == TransportJobState::GoingToPickup) {
            expected_outgoing[source_index][resource] += job.quantity;
        }
        previous_job_id = job.id;
    }
    if (state.next_transport_job_id <= previous_job_id) {
        throw std::invalid_argument("invalid next saved transport job ID");
    }
    for (std::size_t building_index = 0; building_index < state.buildings.size(); ++building_index) {
        const auto& inventory = state.buildings[building_index].inventory;
        for (std::size_t resource = 0; resource < resource_count; ++resource) {
            const auto resource_id = static_cast<ResourceId>(resource);
            if (inventory.reserved_outgoing(resource_id) != expected_outgoing[building_index][resource]
                || inventory.reserved_incoming(resource_id) < expected_incoming[building_index][resource]) {
                throw std::invalid_argument("saved transport reservations do not match jobs");
            }
        }
    }

    auto simulation = Simulation{std::move(catalog)};
    simulation.buildings_ = std::move(state.buildings);
    simulation.transport_jobs_ = std::move(state.transport_jobs);
    simulation.map_ = std::move(restored_map);
    simulation.logistics_distance_field_ = PathDistanceField{};
    simulation.next_building_id_ = state.next_building_id;
    simulation.next_transport_job_id_ = state.next_transport_job_id;
    simulation.next_auto_building_x_ = state.next_auto_building_x;
    simulation.current_tick_ = state.current_tick;
    simulation.worker_assignment_dirty_ = state.worker_assignment_dirty;
    simulation.idle_workers_ = state.idle_workers;
    simulation.stats_ = state.stats;
    return simulation;
}

}
