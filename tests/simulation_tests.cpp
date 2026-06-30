#include "TestSupport.hpp"

#include "core/Simulation.hpp"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <stdexcept>

namespace {

void clear_map_resources(vibecity::Simulation& simulation)
{
    const auto deposits = simulation.map().map_resource_deposits();
    for (const auto& deposit : deposits) {
        VIBECITY_CHECK(simulation.set_map_resource(
            deposit.position,
            deposit.resource,
            0));
    }
}

void default_map_resources_are_deterministic()
{
    const auto first = vibecity::Simulation{};
    const auto second = vibecity::Simulation{};
    const auto first_terrain = first.map().terrain_tiles();
    const auto second_terrain = second.map().terrain_tiles();
    const auto first_deposits = first.map().map_resource_deposits();
    const auto second_deposits = second.map().map_resource_deposits();

    VIBECITY_CHECK(!first_terrain.empty());
    VIBECITY_CHECK(first_terrain == second_terrain);
    VIBECITY_CHECK(std::any_of(
        first_terrain.begin(),
        first_terrain.end(),
        [](const vibecity::TerrainTile& tile) {
            return tile.terrain == vibecity::TerrainId::Fertile;
        }));
    VIBECITY_CHECK(std::any_of(
        first_terrain.begin(),
        first_terrain.end(),
        [](const vibecity::TerrainTile& tile) {
            return tile.terrain == vibecity::TerrainId::Rocky;
        }));
    VIBECITY_CHECK(!first_deposits.empty());
    VIBECITY_CHECK(first_deposits == second_deposits);
    VIBECITY_CHECK(std::any_of(
        first_deposits.begin(),
        first_deposits.end(),
        [](const vibecity::MapResourceDeposit& deposit) {
            return deposit.resource == vibecity::MapResourceId::Stone;
        }));
}

void terrain_blocks_paths_and_buildings()
{
    auto map = vibecity::TileMap{8, 8};

    VIBECITY_CHECK(map.set_terrain(
        vibecity::GridPosition{2, 2},
        vibecity::TerrainId::ShallowWater));
    VIBECITY_CHECK(!map.add_path(vibecity::GridPosition{2, 2}));
    VIBECITY_CHECK(!map.can_place_building(
        vibecity::GridPosition{2, 2},
        vibecity::Footprint{1, 1}));
    VIBECITY_CHECK(!map.place_building(
        1,
        vibecity::GridPosition{2, 2},
        vibecity::Footprint{1, 1}));

    VIBECITY_CHECK(map.add_path(vibecity::GridPosition{3, 3}));
    VIBECITY_CHECK(!map.set_terrain(
        vibecity::GridPosition{3, 3},
        vibecity::TerrainId::ShallowWater));

    VIBECITY_CHECK(map.place_building(
        2,
        vibecity::GridPosition{4, 4},
        vibecity::Footprint{1, 1}));
    VIBECITY_CHECK(!map.set_terrain(
        vibecity::GridPosition{4, 4},
        vibecity::TerrainId::Rocky));
}

void map_resources_follow_supported_terrain()
{
    auto map = vibecity::TileMap{8, 8};
    constexpr auto rocky = vibecity::GridPosition{2, 2};
    constexpr auto grass = vibecity::GridPosition{3, 3};
    constexpr auto water = vibecity::GridPosition{4, 4};

    VIBECITY_CHECK(map.set_terrain(rocky, vibecity::TerrainId::Rocky));
    VIBECITY_CHECK(map.set_map_resource(
        rocky,
        vibecity::MapResourceId::Stone,
        vibecity::stone_tile_capacity));
    VIBECITY_CHECK(!map.set_map_resource(
        rocky,
        vibecity::MapResourceId::Forest,
        vibecity::forest_tile_capacity));

    VIBECITY_CHECK(map.set_map_resource(
        grass,
        vibecity::MapResourceId::Forest,
        vibecity::forest_tile_capacity));
    VIBECITY_CHECK(!map.set_map_resource(
        grass,
        vibecity::MapResourceId::Stone,
        vibecity::stone_tile_capacity));

    VIBECITY_CHECK(map.set_terrain(water, vibecity::TerrainId::ShallowWater));
    VIBECITY_CHECK(!map.set_map_resource(
        water,
        vibecity::MapResourceId::Forest,
        1));
}

void building_placement_can_require_terrain()
{
    auto simulation = vibecity::Simulation{};
    VIBECITY_CHECK(simulation.definition(vibecity::BuildingKind::Farm).required_terrain
        == vibecity::TerrainId::Fertile);

    VIBECITY_CHECK(!simulation.can_place_building_at(
        vibecity::BuildingKind::Farm,
        vibecity::GridPosition{60, 1}));
    VIBECITY_CHECK(simulation.can_place_building_at(
        vibecity::BuildingKind::Farm,
        vibecity::GridPosition{8, 1}));

    const auto farm = simulation.add_building_at(
        vibecity::BuildingKind::Farm,
        vibecity::GridPosition{8, 1});
    VIBECITY_CHECK(simulation.building(farm).kind == vibecity::BuildingKind::Farm);
}

void paths_and_buildings_clear_map_resources()
{
    auto map = vibecity::TileMap{12, 12};
    VIBECITY_CHECK(map.set_map_resource(
        vibecity::GridPosition{2, 2},
        vibecity::MapResourceId::Forest,
        vibecity::forest_tile_capacity));
    VIBECITY_CHECK(map.add_path(vibecity::GridPosition{2, 2}));
    VIBECITY_CHECK(map.map_resource_quantity(vibecity::GridPosition{2, 2}) == 0);

    VIBECITY_CHECK(map.set_map_resource(
        vibecity::GridPosition{5, 5},
        vibecity::MapResourceId::Forest,
        vibecity::forest_tile_capacity));
    VIBECITY_CHECK(map.place_building(
        1,
        vibecity::GridPosition{5, 5},
        vibecity::Footprint{2, 2}));
    VIBECITY_CHECK(map.map_resource_quantity(vibecity::GridPosition{5, 5}) == 0);
}

void path_and_building_removal_free_tiles_without_regrowing_resources()
{
    auto map = vibecity::TileMap{12, 12};
    VIBECITY_CHECK(map.set_map_resource(
        vibecity::GridPosition{2, 2},
        vibecity::MapResourceId::Forest,
        vibecity::forest_tile_capacity));
    VIBECITY_CHECK(map.add_path(vibecity::GridPosition{2, 2}));
    VIBECITY_CHECK(map.remove_path(vibecity::GridPosition{2, 2}));
    VIBECITY_CHECK(!map.has_path(vibecity::GridPosition{2, 2}));
    VIBECITY_CHECK(map.map_resource_quantity(vibecity::GridPosition{2, 2}) == 0);
    VIBECITY_CHECK(!map.remove_path(vibecity::GridPosition{2, 2}));

    VIBECITY_CHECK(map.set_map_resource(
        vibecity::GridPosition{5, 5},
        vibecity::MapResourceId::Forest,
        vibecity::forest_tile_capacity));
    VIBECITY_CHECK(map.place_building(
        7,
        vibecity::GridPosition{5, 5},
        vibecity::Footprint{2, 2}));
    VIBECITY_CHECK(map.remove_building(
        7,
        vibecity::GridPosition{5, 5},
        vibecity::Footprint{2, 2}));
    VIBECITY_CHECK(map.can_place_building(
        vibecity::GridPosition{5, 5},
        vibecity::Footprint{2, 2}));
    VIBECITY_CHECK(map.map_resource_quantity(vibecity::GridPosition{5, 5}) == 0);
    VIBECITY_CHECK(!map.remove_building(
        7,
        vibecity::GridPosition{5, 5},
        vibecity::Footprint{2, 2}));
}

void harvesting_prefers_nearest_then_topmost_resource()
{
    auto map = vibecity::TileMap{12, 12};
    VIBECITY_CHECK(map.tiles_within_radius(
            vibecity::GridPosition{100, 100},
            vibecity::Footprint{1, 1},
            3)
        .empty());
    constexpr auto building_position = vibecity::GridPosition{5, 5};
    constexpr auto footprint = vibecity::Footprint{1, 1};
    constexpr auto top_resource = vibecity::GridPosition{5, 3};
    constexpr auto left_resource = vibecity::GridPosition{3, 5};

    VIBECITY_CHECK(map.set_map_resource(
        top_resource,
        vibecity::MapResourceId::Forest,
        1));
    VIBECITY_CHECK(map.set_map_resource(
        left_resource,
        vibecity::MapResourceId::Forest,
        1));
    VIBECITY_CHECK(map.harvest_map_resource_within_radius(
        building_position,
        footprint,
        vibecity::MapResourceId::Forest,
        3,
        1));

    VIBECITY_CHECK(map.map_resource_quantity(top_resource) == 0);
    VIBECITY_CHECK(map.map_resource_quantity(left_resource) == 1);
}

void path_distance_field_matches_pairwise_pathfinding()
{
    vibecity::TileMap map{20, 10};

    for (int x = 1; x <= 15; ++x) {
        VIBECITY_CHECK(map.add_path(vibecity::GridPosition{x, 2}));
    }
    for (int y = 3; y <= 6; ++y) {
        VIBECITY_CHECK(map.add_path(vibecity::GridPosition{8, y}));
    }

    constexpr auto source_position = vibecity::GridPosition{1, 3};
    constexpr auto source_footprint = vibecity::Footprint{2, 2};
    constexpr auto far_position = vibecity::GridPosition{14, 3};
    constexpr auto far_footprint = vibecity::Footprint{2, 2};
    constexpr auto branch_position = vibecity::GridPosition{7, 7};
    constexpr auto branch_footprint = vibecity::Footprint{2, 1};
    constexpr auto disconnected_position = vibecity::GridPosition{17, 3};

    const auto field = map.path_distances_from_building(source_position, source_footprint);

    VIBECITY_CHECK(field.distance_to_building(far_position, far_footprint)
        == map.path_distance_between_buildings(
            source_position,
            source_footprint,
            far_position,
            far_footprint));
    VIBECITY_CHECK(field.distance_to_building(branch_position, branch_footprint)
        == map.path_distance_between_buildings(
            source_position,
            source_footprint,
            branch_position,
            branch_footprint));
    VIBECITY_CHECK(!field.distance_to_building(disconnected_position, far_footprint).has_value());
    VIBECITY_CHECK(!field.distance_to_building(far_position, vibecity::Footprint{0, 1}).has_value());
}

void path_distance_field_can_be_reused_without_stale_distances()
{
    vibecity::TileMap map{20, 10};
    for (int x = 1; x <= 8; ++x) {
        VIBECITY_CHECK(map.add_path(vibecity::GridPosition{x, 2}));
    }
    for (int x = 12; x <= 17; ++x) {
        VIBECITY_CHECK(map.add_path(vibecity::GridPosition{x, 6}));
    }

    auto field = vibecity::PathDistanceField{};
    constexpr auto first_source = vibecity::GridPosition{1, 3};
    constexpr auto first_destination = vibecity::GridPosition{7, 3};
    constexpr auto second_source = vibecity::GridPosition{12, 7};
    constexpr auto second_destination = vibecity::GridPosition{16, 7};
    constexpr auto footprint = vibecity::Footprint{1, 1};

    map.populate_path_distances_from_building(field, first_source, footprint);
    VIBECITY_CHECK(field.distance_to_building(first_destination, footprint).has_value());
    VIBECITY_CHECK(!field.distance_to_building(second_destination, footprint).has_value());

    map.populate_path_distances_from_building(field, second_source, footprint);
    VIBECITY_CHECK(!field.distance_to_building(first_destination, footprint).has_value());
    VIBECITY_CHECK(field.distance_to_building(second_destination, footprint).has_value());
}

void path_route_follows_connected_road_tiles()
{
    vibecity::TileMap map{20, 10};
    for (int x = 1; x <= 15; ++x) {
        VIBECITY_CHECK(map.add_path(vibecity::GridPosition{x, 2}));
    }
    for (int y = 3; y <= 6; ++y) {
        VIBECITY_CHECK(map.add_path(vibecity::GridPosition{8, y}));
    }

    constexpr auto source_position = vibecity::GridPosition{1, 3};
    constexpr auto source_footprint = vibecity::Footprint{2, 2};
    constexpr auto destination_position = vibecity::GridPosition{7, 7};
    constexpr auto destination_footprint = vibecity::Footprint{2, 1};

    const auto route = map.path_between_buildings(
        source_position,
        source_footprint,
        destination_position,
        destination_footprint);
    const auto distance = map.path_distance_between_buildings(
        source_position,
        source_footprint,
        destination_position,
        destination_footprint);

    VIBECITY_CHECK(distance.has_value());
    VIBECITY_CHECK(route.size() == static_cast<std::size_t>(*distance + 1));
    for (std::size_t index = 0; index < route.size(); ++index) {
        VIBECITY_CHECK(map.has_path(route[index]));
        if (index > 0) {
            const auto step = std::abs(route[index].x - route[index - 1].x)
                + std::abs(route[index].y - route[index - 1].y);
            VIBECITY_CHECK(step == 1);
        }
    }

    VIBECITY_CHECK(map.path_between_buildings(
            source_position,
            source_footprint,
            vibecity::GridPosition{17, 3},
            vibecity::Footprint{1, 1})
        .empty());
}

void path_connectivity_labels_building_access_components()
{
    vibecity::TileMap map{20, 10};
    for (int x = 1; x <= 8; ++x) {
        VIBECITY_CHECK(map.add_path(vibecity::GridPosition{x, 2}));
    }
    for (int x = 12; x <= 17; ++x) {
        VIBECITY_CHECK(map.add_path(vibecity::GridPosition{x, 6}));
    }
    VIBECITY_CHECK(map.add_path(vibecity::GridPosition{10, 4}));
    VIBECITY_CHECK(map.add_path(vibecity::GridPosition{10, 6}));

    const auto connectivity = map.path_connectivity();
    const auto first_source = connectivity.components_touching_building(
        vibecity::GridPosition{1, 3},
        vibecity::Footprint{1, 1});
    const auto first_destination = connectivity.components_touching_building(
        vibecity::GridPosition{7, 3},
        vibecity::Footprint{1, 1});
    const auto second_source = connectivity.components_touching_building(
        vibecity::GridPosition{12, 7},
        vibecity::Footprint{1, 1});
    const auto multi_access = connectivity.components_touching_building(
        vibecity::GridPosition{10, 5},
        vibecity::Footprint{1, 1});

    VIBECITY_CHECK(first_source.size() == 1);
    VIBECITY_CHECK(first_source == first_destination);
    VIBECITY_CHECK(second_source.size() == 1);
    VIBECITY_CHECK(first_source != second_source);
    VIBECITY_CHECK(multi_access.size() == 2);
}

void demolishing_building_frees_tiles_and_cancels_transport()
{
    auto simulation = vibecity::Simulation{};
    clear_map_resources(simulation);
    for (int x = 1; x <= 8; ++x) {
        VIBECITY_CHECK(simulation.add_path(vibecity::GridPosition{x, 0}));
    }

    const auto house = simulation.add_building_at(
        vibecity::BuildingKind::House,
        vibecity::GridPosition{1, 1});
    const auto storehouse = simulation.add_building_at(
        vibecity::BuildingKind::Storehouse,
        vibecity::GridPosition{5, 1});
    simulation.set_residents(house, 1);
    VIBECITY_CHECK(simulation.building(storehouse).inventory.add(
        vibecity::ResourceId::Bread,
        5));

    simulation.tick();
    VIBECITY_CHECK(!simulation.transport_jobs().empty());
    VIBECITY_CHECK(simulation.building(storehouse).inventory.reserved_outgoing(
        vibecity::ResourceId::Bread) > 0);

    VIBECITY_CHECK(simulation.demolish_building(house));
    VIBECITY_CHECK(simulation.transport_jobs().empty());
    VIBECITY_CHECK(simulation.building(storehouse).inventory.reserved_outgoing(
        vibecity::ResourceId::Bread) == 0);
    VIBECITY_CHECK(simulation.total_residents() == 0);
    VIBECITY_CHECK(simulation.total_housing_capacity() == 0);
    VIBECITY_CHECK(simulation.map().can_place_building(
        vibecity::GridPosition{1, 1},
        vibecity::Footprint{1, 1}));
    VIBECITY_CHECK(!simulation.demolish_building(house));

    auto threw = false;
    try {
        [[maybe_unused]] const auto& removed = simulation.building(house);
    } catch (const std::out_of_range&) {
        threw = true;
    }
    VIBECITY_CHECK(threw);
}

void farm_produces_grain_when_staffed()
{
    vibecity::Simulation simulation;

    const auto house = simulation.add_building(vibecity::BuildingKind::House);
    const auto farm = simulation.add_building(vibecity::BuildingKind::Farm);

    simulation.set_residents(house, 5);
    simulation.run_for(480);

    const auto& farm_instance = simulation.building(farm);
    VIBECITY_CHECK(farm_instance.inventory.quantity(vibecity::ResourceId::Grain) == 12);
    VIBECITY_CHECK(farm_instance.blocking_reason == vibecity::BlockingReason::None);
    VIBECITY_CHECK(simulation.stats().produced[vibecity::resource_index(vibecity::ResourceId::Grain)] == 12);
}

void bakery_consumes_inputs_and_produces_bread()
{
    vibecity::Simulation simulation;

    const auto house = simulation.add_building(vibecity::BuildingKind::House);
    const auto bakery = simulation.add_building(vibecity::BuildingKind::Bakery);

    simulation.set_residents(house, 5);
    simulation.building(bakery).inventory.add(vibecity::ResourceId::Grain, 6);
    simulation.building(bakery).inventory.add(vibecity::ResourceId::Firewood, 2);

    simulation.run_for(360);

    const auto& bakery_instance = simulation.building(bakery);
    VIBECITY_CHECK(bakery_instance.inventory.quantity(vibecity::ResourceId::Grain) == 0);
    VIBECITY_CHECK(bakery_instance.inventory.quantity(vibecity::ResourceId::Firewood) == 0);
    VIBECITY_CHECK(bakery_instance.inventory.quantity(vibecity::ResourceId::Bread) == 4);
    VIBECITY_CHECK(bakery_instance.blocking_reason == vibecity::BlockingReason::None);
}

void house_consumes_bread_daily()
{
    vibecity::Simulation simulation;

    const auto house = simulation.add_building(vibecity::BuildingKind::House);
    simulation.set_residents(house, 5);
    simulation.building(house).inventory.add(vibecity::ResourceId::Bread, 5);

    simulation.run_for(vibecity::ticks_per_day);

    const auto& house_instance = simulation.building(house);
    VIBECITY_CHECK(house_instance.inventory.quantity(vibecity::ResourceId::Bread) == 0);
    VIBECITY_CHECK(house_instance.hunger_days == 0);
    VIBECITY_CHECK(simulation.stats().consumed[vibecity::resource_index(vibecity::ResourceId::Bread)] == 5);
}

void house_records_hunger_when_bread_is_missing()
{
    vibecity::Simulation simulation;

    const auto house = simulation.add_building(vibecity::BuildingKind::House);
    simulation.set_residents(house, 5);
    simulation.building(house).inventory.add(vibecity::ResourceId::Bread, 2);

    simulation.run_for(vibecity::ticks_per_day);

    const auto& house_instance = simulation.building(house);
    VIBECITY_CHECK(house_instance.inventory.quantity(vibecity::ResourceId::Bread) == 0);
    VIBECITY_CHECK(house_instance.hunger_days == 1);
    VIBECITY_CHECK(house_instance.blocking_reason == vibecity::BlockingReason::MissingBread);
    VIBECITY_CHECK(simulation.stats().consumed[vibecity::resource_index(vibecity::ResourceId::Bread)] == 2);
}

void settlement_population_facts_track_housing_and_food_need()
{
    vibecity::Simulation simulation;

    const auto first_house = simulation.add_building(vibecity::BuildingKind::House);
    const auto second_house = simulation.add_building(vibecity::BuildingKind::House);
    simulation.place_construction(vibecity::BuildingKind::House);
    simulation.add_building(vibecity::BuildingKind::Farm);

    simulation.set_residents(first_house, 5);
    simulation.set_residents(second_house, 3);

    VIBECITY_CHECK(simulation.total_residents() == 8);
    VIBECITY_CHECK(simulation.total_housing_capacity() == 10);
    VIBECITY_CHECK(simulation.free_housing_capacity() == 2);
    VIBECITY_CHECK(simulation.daily_bread_need() == 8);
    VIBECITY_CHECK(simulation.bread_required_for_population_growth() == 9);
    VIBECITY_CHECK(simulation.stored_bread() == 0);
    VIBECITY_CHECK(simulation.bread_days_remaining() == 0);
    VIBECITY_CHECK(simulation.population_growth_blocker() == vibecity::PopulationGrowthBlocker::NotEnoughBread);
}

void population_grows_into_free_housing_when_bread_is_available()
{
    vibecity::Simulation simulation;

    const auto house = simulation.add_building(vibecity::BuildingKind::House);
    simulation.set_residents(house, 4);
    VIBECITY_CHECK(simulation.building(house).inventory.add(vibecity::ResourceId::Bread, 10));
    VIBECITY_CHECK(simulation.population_growth_blocker() == vibecity::PopulationGrowthBlocker::None);

    simulation.run_for(vibecity::ticks_per_day);

    VIBECITY_CHECK(simulation.building(house).residents == 5);
    VIBECITY_CHECK(simulation.total_residents() == 5);
    VIBECITY_CHECK(simulation.daily_bread_need() == 5);
    VIBECITY_CHECK(simulation.stored_bread() == 6);
    VIBECITY_CHECK(simulation.bread_days_remaining() == 1);
    VIBECITY_CHECK(simulation.building(house).inventory.quantity(vibecity::ResourceId::Bread) == 6);
}

void population_does_not_grow_when_housing_is_full()
{
    vibecity::Simulation simulation;

    const auto house = simulation.add_building(vibecity::BuildingKind::House);
    simulation.set_residents(house, 5);
    VIBECITY_CHECK(simulation.building(house).inventory.add(vibecity::ResourceId::Bread, 10));
    VIBECITY_CHECK(simulation.population_growth_blocker() == vibecity::PopulationGrowthBlocker::NoHousing);

    simulation.run_for(vibecity::ticks_per_day);

    VIBECITY_CHECK(simulation.building(house).residents == 5);
    VIBECITY_CHECK(simulation.free_housing_capacity() == 0);
}

void population_does_not_grow_when_bread_is_missing()
{
    vibecity::Simulation simulation;

    const auto house = simulation.add_building(vibecity::BuildingKind::House);
    simulation.set_residents(house, 4);
    simulation.building(house).inventory.add(vibecity::ResourceId::Bread, 3);
    VIBECITY_CHECK(simulation.population_growth_blocker() == vibecity::PopulationGrowthBlocker::NotEnoughBread);

    simulation.run_for(vibecity::ticks_per_day);

    VIBECITY_CHECK(simulation.building(house).residents == 4);
    VIBECITY_CHECK(simulation.building(house).hunger_days == 1);
    VIBECITY_CHECK(simulation.free_housing_capacity() == 1);
    VIBECITY_CHECK(simulation.population_growth_blocker() == vibecity::PopulationGrowthBlocker::HungryHouse);
}

void population_does_not_grow_when_houses_are_hungry()
{
    vibecity::Simulation simulation;

    const auto house = simulation.add_building_at(vibecity::BuildingKind::House, vibecity::GridPosition{1, 1});
    const auto storehouse = simulation.add_building_at(vibecity::BuildingKind::Storehouse, vibecity::GridPosition{8, 1});

    simulation.set_residents(house, 4);
    simulation.building(storehouse).inventory.add(vibecity::ResourceId::Bread, 100);

    simulation.run_for(vibecity::ticks_per_day);

    VIBECITY_CHECK(simulation.building(house).residents == 4);
    VIBECITY_CHECK(simulation.building(house).hunger_days == 1);
    VIBECITY_CHECK(simulation.total_inventory()[vibecity::resource_index(vibecity::ResourceId::Bread)] == 100);
}

void logistics_delivers_bread_from_storehouse_to_house()
{
    vibecity::Simulation simulation;

    const auto house = simulation.add_building(vibecity::BuildingKind::House);
    const auto storehouse = simulation.add_building(vibecity::BuildingKind::Storehouse);

    simulation.set_residents(house, 5);
    simulation.building(storehouse).inventory.add(vibecity::ResourceId::Bread, 10);

    simulation.run_for(25);

    const auto& house_instance = simulation.building(house);
    const auto& storehouse_instance = simulation.building(storehouse);
    VIBECITY_CHECK(house_instance.inventory.quantity(vibecity::ResourceId::Bread) == 10);
    VIBECITY_CHECK(storehouse_instance.inventory.quantity(vibecity::ResourceId::Bread) == 0);
    VIBECITY_CHECK(simulation.transport_jobs().empty());
    VIBECITY_CHECK(simulation.stats().transported[vibecity::resource_index(vibecity::ResourceId::Bread)] == 10);
}

void logistics_summary_tracks_reservations_and_in_transit_goods()
{
    vibecity::Simulation simulation;

    const auto house = simulation.add_building(vibecity::BuildingKind::House);
    const auto storehouse = simulation.add_building(vibecity::BuildingKind::Storehouse);

    simulation.set_residents(house, 5);
    simulation.building(storehouse).inventory.add(vibecity::ResourceId::Bread, 10);

    simulation.tick();

    auto summary = simulation.logistics_summary();
    VIBECITY_CHECK(summary.active_jobs == 1);
    VIBECITY_CHECK(summary.going_to_pickup == 1);
    VIBECITY_CHECK(summary.carrying_goods == 0);
    VIBECITY_CHECK(summary.reserved_incoming[vibecity::resource_index(vibecity::ResourceId::Bread)] == 5);
    VIBECITY_CHECK(summary.reserved_outgoing[vibecity::resource_index(vibecity::ResourceId::Bread)] == 5);
    VIBECITY_CHECK(summary.in_transit[vibecity::resource_index(vibecity::ResourceId::Bread)] == 0);

    for (auto attempts = 0; attempts < 20 && !simulation.transport_jobs().empty()
        && simulation.transport_jobs().front().state != vibecity::TransportJobState::CarryingGoods; ++attempts) {
        simulation.tick();
    }

    summary = simulation.logistics_summary();
    VIBECITY_CHECK(summary.active_jobs == 1);
    VIBECITY_CHECK(summary.going_to_pickup == 0);
    VIBECITY_CHECK(summary.carrying_goods == 1);
    VIBECITY_CHECK(summary.reserved_incoming[vibecity::resource_index(vibecity::ResourceId::Bread)] == 5);
    VIBECITY_CHECK(summary.reserved_outgoing[vibecity::resource_index(vibecity::ResourceId::Bread)] == 0);
    VIBECITY_CHECK(summary.in_transit[vibecity::resource_index(vibecity::ResourceId::Bread)] == 5);
}

void transport_job_keeps_dispatched_delivery_duration()
{
    vibecity::Simulation simulation;

    const auto storehouse = simulation.add_building_at(
        vibecity::BuildingKind::Storehouse,
        vibecity::GridPosition{1, 1});
    const auto house = simulation.add_building_at(
        vibecity::BuildingKind::House,
        vibecity::GridPosition{8, 1});
    for (int x = 1; x <= 8; ++x) {
        VIBECITY_CHECK(simulation.add_path(vibecity::GridPosition{x, 0}));
    }

    simulation.set_residents(house, 5);
    simulation.building(storehouse).inventory.add(vibecity::ResourceId::Bread, 10);

    const auto expected_distance = simulation.map().path_distance_between_buildings(
        *simulation.building(storehouse).position,
        vibecity::building_definition(vibecity::BuildingKind::Storehouse).footprint,
        *simulation.building(house).position,
        vibecity::building_definition(vibecity::BuildingKind::House).footprint);
    VIBECITY_CHECK(expected_distance.has_value());

    simulation.tick();
    VIBECITY_CHECK(simulation.transport_jobs().size() == 1);
    VIBECITY_CHECK(simulation.transport_jobs().front().delivery_ticks == *expected_distance);
    VIBECITY_CHECK(simulation.transport_jobs().front().state == vibecity::TransportJobState::GoingToPickup);

    for (auto attempts = 0; attempts < 20
        && !simulation.transport_jobs().empty()
        && simulation.transport_jobs().front().state == vibecity::TransportJobState::GoingToPickup; ++attempts) {
        simulation.tick();
    }

    VIBECITY_CHECK(simulation.transport_jobs().size() == 1);
    VIBECITY_CHECK(simulation.transport_jobs().front().state == vibecity::TransportJobState::CarryingGoods);
    VIBECITY_CHECK(simulation.transport_jobs().front().ticks_remaining == *expected_distance);
    VIBECITY_CHECK(simulation.transport_jobs().front().leg_ticks_total == *expected_distance);
}

void disconnected_buildings_cannot_exchange_goods()
{
    vibecity::Simulation simulation;

    const auto house = simulation.add_building_at(vibecity::BuildingKind::House, vibecity::GridPosition{1, 1});
    const auto storehouse = simulation.add_building_at(vibecity::BuildingKind::Storehouse, vibecity::GridPosition{8, 1});

    simulation.set_residents(house, 5);
    simulation.building(storehouse).inventory.add(vibecity::ResourceId::Bread, 10);

    simulation.run_for(40);

    const auto& house_instance = simulation.building(house);
    const auto& storehouse_instance = simulation.building(storehouse);
    VIBECITY_CHECK(house_instance.inventory.quantity(vibecity::ResourceId::Bread) == 0);
    VIBECITY_CHECK(storehouse_instance.inventory.quantity(vibecity::ResourceId::Bread) == 10);
    VIBECITY_CHECK(house_instance.blocking_reason == vibecity::BlockingReason::NoReachableSource);
    VIBECITY_CHECK(simulation.transport_jobs().empty());
}

void connected_paths_allow_goods_exchange()
{
    vibecity::Simulation simulation;

    const auto house = simulation.add_building_at(vibecity::BuildingKind::House, vibecity::GridPosition{1, 1});
    const auto storehouse = simulation.add_building_at(vibecity::BuildingKind::Storehouse, vibecity::GridPosition{8, 1});

    for (int x = 1; x <= 9; ++x) {
        VIBECITY_CHECK(simulation.add_path(vibecity::GridPosition{x, 0}));
    }

    simulation.set_residents(house, 5);
    simulation.building(storehouse).inventory.add(vibecity::ResourceId::Bread, 10);

    simulation.run_for(40);

    const auto& house_instance = simulation.building(house);
    const auto& storehouse_instance = simulation.building(storehouse);
    VIBECITY_CHECK(house_instance.inventory.quantity(vibecity::ResourceId::Bread) == 10);
    VIBECITY_CHECK(storehouse_instance.inventory.quantity(vibecity::ResourceId::Bread) == 0);
    VIBECITY_CHECK(simulation.stats().transported[vibecity::resource_index(vibecity::ResourceId::Bread)] == 10);
}

void logistics_prefers_nearest_reachable_source()
{
    vibecity::Simulation simulation;

    const auto far_bakery = simulation.add_building_at(vibecity::BuildingKind::Bakery, vibecity::GridPosition{1, 1});
    const auto worker_house = simulation.add_building_at(vibecity::BuildingKind::House, vibecity::GridPosition{5, 1});
    const auto house = simulation.add_building_at(vibecity::BuildingKind::House, vibecity::GridPosition{8, 1});
    const auto near_bakery = simulation.add_building_at(vibecity::BuildingKind::Bakery, vibecity::GridPosition{12, 1});

    for (int x = 1; x <= 13; ++x) {
        VIBECITY_CHECK(simulation.add_path(vibecity::GridPosition{x, 0}));
    }

    simulation.set_residents(house, 5);
    simulation.set_residents(worker_house, 5);
    simulation.building(worker_house).inventory.add(vibecity::ResourceId::Bread, 10);
    simulation.building(far_bakery).inventory.add(vibecity::ResourceId::Bread, 10);
    simulation.building(near_bakery).inventory.add(vibecity::ResourceId::Bread, 10);

    simulation.run_for(40);

    VIBECITY_CHECK(simulation.building(house).inventory.quantity(vibecity::ResourceId::Bread) == 10);
    VIBECITY_CHECK(simulation.building(near_bakery).inventory.quantity(vibecity::ResourceId::Bread) == 0);
    VIBECITY_CHECK(simulation.building(far_bakery).inventory.quantity(vibecity::ResourceId::Bread) == 10);
}

void logistics_breaks_equal_distance_ties_by_building_id()
{
    vibecity::Simulation simulation;

    const auto lower_id_bakery = simulation.add_building_at(
        vibecity::BuildingKind::Bakery,
        vibecity::GridPosition{3, 1});
    const auto destination_house = simulation.add_building_at(
        vibecity::BuildingKind::House,
        vibecity::GridPosition{8, 1});
    const auto higher_id_bakery = simulation.add_building_at(
        vibecity::BuildingKind::Bakery,
        vibecity::GridPosition{13, 1});
    const auto worker_house = simulation.add_building_at(
        vibecity::BuildingKind::House,
        vibecity::GridPosition{7, 4});

    for (int x = 3; x <= 14; ++x) {
        VIBECITY_CHECK(simulation.add_path(vibecity::GridPosition{x, 0}));
    }
    for (int y = 1; y <= 3; ++y) {
        VIBECITY_CHECK(simulation.add_path(vibecity::GridPosition{7, y}));
    }

    simulation.set_residents(destination_house, 5);
    simulation.set_residents(worker_house, 5);
    simulation.building(worker_house).inventory.add(vibecity::ResourceId::Bread, 10);
    simulation.building(lower_id_bakery).inventory.add(vibecity::ResourceId::Bread, 10);
    simulation.building(higher_id_bakery).inventory.add(vibecity::ResourceId::Bread, 10);

    simulation.run_for(30);

    VIBECITY_CHECK(simulation.building(destination_house).inventory.quantity(vibecity::ResourceId::Bread) == 10);
    VIBECITY_CHECK(simulation.building(lower_id_bakery).inventory.quantity(vibecity::ResourceId::Bread) == 0);
    VIBECITY_CHECK(simulation.building(higher_id_bakery).inventory.quantity(vibecity::ResourceId::Bread) == 10);
}

void disconnected_workers_do_not_staff_workplaces()
{
    vibecity::Simulation simulation;

    const auto house = simulation.add_building_at(vibecity::BuildingKind::House, vibecity::GridPosition{1, 1});
    const auto farm = simulation.add_building_at(vibecity::BuildingKind::Farm, vibecity::GridPosition{8, 1});

    simulation.set_residents(house, 5);
    simulation.run_for(720);

    const auto& farm_instance = simulation.building(farm);
    VIBECITY_CHECK(farm_instance.assigned_workers == 0);
    VIBECITY_CHECK(farm_instance.inventory.quantity(vibecity::ResourceId::Grain) == 0);
    VIBECITY_CHECK(farm_instance.blocking_reason == vibecity::BlockingReason::NotEnoughWorkers);
}

void bakery_fetches_inputs_before_producing()
{
    vibecity::Simulation simulation;

    const auto house = simulation.add_building(vibecity::BuildingKind::House);
    const auto storehouse = simulation.add_building(vibecity::BuildingKind::Storehouse);
    const auto bakery = simulation.add_building(vibecity::BuildingKind::Bakery);

    simulation.set_residents(house, 5);
    simulation.building(storehouse).inventory.add(vibecity::ResourceId::Grain, 6);
    simulation.building(storehouse).inventory.add(vibecity::ResourceId::Firewood, 2);

    simulation.run_for(420);

    const auto& bakery_instance = simulation.building(bakery);
    VIBECITY_CHECK(bakery_instance.inventory.quantity(vibecity::ResourceId::Grain) == 0);
    VIBECITY_CHECK(bakery_instance.inventory.quantity(vibecity::ResourceId::Firewood) == 0);
    VIBECITY_CHECK(simulation.stats().produced[vibecity::resource_index(vibecity::ResourceId::Bread)] == 4);
    VIBECITY_CHECK(simulation.stats().transported[vibecity::resource_index(vibecity::ResourceId::Grain)] == 6);
    VIBECITY_CHECK(simulation.stats().transported[vibecity::resource_index(vibecity::ResourceId::Firewood)] == 2);
}

void farm_and_woodcutter_supply_bakery_chain()
{
    vibecity::Simulation simulation;

    const auto first_house = simulation.add_building(vibecity::BuildingKind::House);
    const auto second_house = simulation.add_building(vibecity::BuildingKind::House);
    const auto third_house = simulation.add_building(vibecity::BuildingKind::House);
    const auto farm = simulation.add_building(vibecity::BuildingKind::Farm);
    const auto woodcutter = simulation.add_building(vibecity::BuildingKind::Woodcutter);
    const auto bakery = simulation.add_building(vibecity::BuildingKind::Bakery);

    simulation.set_residents(first_house, 5);
    simulation.set_residents(second_house, 5);
    simulation.set_residents(third_house, 5);

    simulation.run_for(1'100);

    const auto& farm_instance = simulation.building(farm);
    const auto& woodcutter_instance = simulation.building(woodcutter);
    const auto& bakery_instance = simulation.building(bakery);
    VIBECITY_CHECK(farm_instance.assigned_workers == 1);
    VIBECITY_CHECK(woodcutter_instance.assigned_workers == 1);
    VIBECITY_CHECK(bakery_instance.assigned_workers == 2);
    VIBECITY_CHECK(simulation.stats().produced[vibecity::resource_index(vibecity::ResourceId::Grain)] >= 12);
    VIBECITY_CHECK(simulation.stats().produced[vibecity::resource_index(vibecity::ResourceId::Firewood)] >= 3);
    VIBECITY_CHECK(simulation.stats().produced[vibecity::resource_index(vibecity::ResourceId::Bread)] >= 4);
    VIBECITY_CHECK(simulation.stats().transported[vibecity::resource_index(vibecity::ResourceId::Grain)] >= 6);
    VIBECITY_CHECK(simulation.stats().transported[vibecity::resource_index(vibecity::ResourceId::Firewood)] >= 2);
}

void woodcutter_harvests_finite_nearby_forest()
{
    vibecity::Simulation simulation;
    clear_map_resources(simulation);

    const auto house = simulation.add_building_at(
        vibecity::BuildingKind::House,
        vibecity::GridPosition{1, 1});
    const auto woodcutter = simulation.add_building_at(
        vibecity::BuildingKind::Woodcutter,
        vibecity::GridPosition{3, 1});
    for (int x = 1; x <= 5; ++x) {
        VIBECITY_CHECK(simulation.add_path(vibecity::GridPosition{x, 0}));
    }
    VIBECITY_CHECK(simulation.set_map_resource(
        vibecity::GridPosition{5, 2},
        vibecity::MapResourceId::Forest,
        2));
    simulation.set_residents(house, 1);

    simulation.run_for(720);

    const auto& instance = simulation.building(woodcutter);
    VIBECITY_CHECK(simulation.map().map_resource_quantity(vibecity::GridPosition{5, 2}) == 0);
    VIBECITY_CHECK(instance.inventory.quantity(vibecity::ResourceId::Timber) == 2);
    VIBECITY_CHECK(instance.inventory.quantity(vibecity::ResourceId::Firewood) == 6);

    simulation.tick();
    VIBECITY_CHECK(instance.blocking_reason == vibecity::BlockingReason::NoNearbyMapResource);
}

void woodcutter_ignores_forest_outside_collection_radius()
{
    vibecity::Simulation simulation;
    clear_map_resources(simulation);

    const auto house = simulation.add_building_at(
        vibecity::BuildingKind::House,
        vibecity::GridPosition{1, 1});
    const auto woodcutter = simulation.add_building_at(
        vibecity::BuildingKind::Woodcutter,
        vibecity::GridPosition{3, 1});
    for (int x = 1; x <= 5; ++x) {
        VIBECITY_CHECK(simulation.add_path(vibecity::GridPosition{x, 0}));
    }
    VIBECITY_CHECK(simulation.set_map_resource(
        vibecity::GridPosition{20, 20},
        vibecity::MapResourceId::Forest,
        vibecity::forest_tile_capacity));
    simulation.set_residents(house, 1);

    simulation.tick();

    const auto& instance = simulation.building(woodcutter);
    VIBECITY_CHECK(instance.inventory.quantity(vibecity::ResourceId::Timber) == 0);
    VIBECITY_CHECK(instance.blocking_reason == vibecity::BlockingReason::NoNearbyMapResource);
}

void quarry_harvests_finite_nearby_stone()
{
    vibecity::Simulation simulation;
    clear_map_resources(simulation);

    const auto quarry_kind = simulation.building_catalog().find_kind("quarry");
    VIBECITY_CHECK(quarry_kind.has_value());
    const auto& quarry_definition = simulation.definition(*quarry_kind);
    VIBECITY_CHECK(quarry_definition.gathering.has_value());
    VIBECITY_CHECK(quarry_definition.gathering->resource == vibecity::MapResourceId::Stone);

    auto quarry_position = std::optional<vibecity::GridPosition>{};
    auto stone_position = std::optional<vibecity::GridPosition>{};
    auto house_position = vibecity::GridPosition{};
    auto path_position = vibecity::GridPosition{};
    const auto inside_footprint = [](
                                      vibecity::GridPosition tile,
                                      vibecity::GridPosition origin,
                                      vibecity::Footprint footprint) {
        return tile.x >= origin.x
            && tile.y >= origin.y
            && tile.x < origin.x + footprint.width
            && tile.y < origin.y + footprint.height;
    };
    for (int y = 1; y < simulation.map().height() && !quarry_position.has_value(); ++y) {
        for (int x = 1; x < simulation.map().width() && !quarry_position.has_value(); ++x) {
            const auto candidate = vibecity::GridPosition{x, y};
            if (!simulation.can_place_building_at(*quarry_kind, candidate)) {
                continue;
            }

            path_position = vibecity::GridPosition{x, y + quarry_definition.footprint.height};
            house_position = vibecity::GridPosition{x, path_position.y + 1};
            if (!simulation.map().in_bounds(path_position)
                || !simulation.can_place_building_at(vibecity::BuildingKind::House, house_position)) {
                continue;
            }

            for (const auto stone_candidate : simulation.map().tiles_within_radius(
                     candidate,
                     quarry_definition.footprint,
                     quarry_definition.gathering->radius)) {
                if (!inside_footprint(stone_candidate, candidate, quarry_definition.footprint)
                    && !inside_footprint(
                        stone_candidate,
                        house_position,
                        simulation.definition(vibecity::BuildingKind::House).footprint)
                    && !(stone_candidate == path_position)
                    && simulation.map().terrain_at(stone_candidate) == vibecity::TerrainId::Rocky) {
                    quarry_position = candidate;
                    stone_position = stone_candidate;
                    break;
                }
            }
        }
    }
    VIBECITY_CHECK(quarry_position.has_value());
    VIBECITY_CHECK(stone_position.has_value());

    const auto house = simulation.add_building_at(
        vibecity::BuildingKind::House,
        house_position);
    const auto quarry = simulation.add_building_at(*quarry_kind, *quarry_position);
    VIBECITY_CHECK(simulation.add_path(path_position));
    VIBECITY_CHECK(simulation.set_map_resource(
        *stone_position,
        vibecity::MapResourceId::Stone,
        2));
    simulation.set_residents(house, 1);

    simulation.run_for(960);

    const auto& instance = simulation.building(quarry);
    VIBECITY_CHECK(simulation.map().map_resource_quantity(*stone_position) == 0);
    VIBECITY_CHECK(instance.inventory.quantity(vibecity::ResourceId::Stone) == 8);
    VIBECITY_CHECK(simulation.stats().produced[vibecity::resource_index(vibecity::ResourceId::Stone)] == 8);

    simulation.tick();
    VIBECITY_CHECK(instance.blocking_reason == vibecity::BlockingReason::NoNearbyMapResource);
}

void construction_site_fetches_materials_and_completes()
{
    vibecity::Simulation simulation;

    const auto house = simulation.add_building(vibecity::BuildingKind::House);
    const auto storehouse = simulation.add_building(vibecity::BuildingKind::Storehouse);
    const auto site = simulation.place_construction(vibecity::BuildingKind::Farm);

    simulation.set_residents(house, 5);
    simulation.building(storehouse).inventory.add(vibecity::ResourceId::Timber, 8);

    simulation.run_for(1'300);

    const auto& completed = simulation.building(site);
    const auto& storehouse_instance = simulation.building(storehouse);
    VIBECITY_CHECK(completed.kind == vibecity::BuildingKind::Farm);
    VIBECITY_CHECK(completed.position.has_value());
    VIBECITY_CHECK(completed.construction_target == std::nullopt);
    VIBECITY_CHECK(storehouse_instance.inventory.quantity(vibecity::ResourceId::Timber) == 0);
    VIBECITY_CHECK(simulation.stats().constructed_buildings == 1);
    VIBECITY_CHECK(simulation.stats().transported[vibecity::resource_index(vibecity::ResourceId::Timber)] == 8);
}

void construction_summary_reports_queue_focus()
{
    vibecity::Simulation simulation;

    const auto house = simulation.add_building(vibecity::BuildingKind::House);
    const auto storehouse = simulation.add_building(vibecity::BuildingKind::Storehouse);
    const auto first_site = simulation.place_construction(vibecity::BuildingKind::House);
    const auto second_site = simulation.place_construction(vibecity::BuildingKind::Farm);

    simulation.set_residents(house, 5);
    simulation.building(storehouse).inventory.add(vibecity::ResourceId::Timber, 20);

    auto summary = simulation.construction_summary();
    VIBECITY_CHECK(summary.sites == 2);
    VIBECITY_CHECK(summary.next_site == first_site);
    VIBECITY_CHECK(summary.next_target == vibecity::BuildingKind::House);
    VIBECITY_CHECK(summary.next_labor_remaining
        == simulation.definition(vibecity::BuildingKind::House).construction_labor_minutes);
    VIBECITY_CHECK(summary.active_builders == 0);

    for (auto attempts = 0; attempts < 4'000
        && simulation.building(first_site).kind == vibecity::BuildingKind::ConstructionSite; ++attempts) {
        simulation.tick();
    }

    VIBECITY_CHECK(simulation.building(first_site).kind == vibecity::BuildingKind::House);
    VIBECITY_CHECK(simulation.building(second_site).kind == vibecity::BuildingKind::ConstructionSite);

    summary = simulation.construction_summary();
    VIBECITY_CHECK(summary.sites == 1);
    VIBECITY_CHECK(summary.next_site == second_site);
    VIBECITY_CHECK(summary.next_target == vibecity::BuildingKind::Farm);
    VIBECITY_CHECK(summary.next_labor_remaining > 0);
}

void construction_site_reports_missing_materials()
{
    vibecity::Simulation simulation;

    const auto house = simulation.add_building(vibecity::BuildingKind::House);
    const auto site = simulation.place_construction(vibecity::BuildingKind::Farm);

    simulation.set_residents(house, 5);
    simulation.run_for(20);

    const auto& construction_site = simulation.building(site);
    VIBECITY_CHECK(construction_site.kind == vibecity::BuildingKind::ConstructionSite);
    VIBECITY_CHECK(construction_site.blocking_reason == vibecity::BlockingReason::NoReachableSource);
    VIBECITY_CHECK(simulation.stats().constructed_buildings == 0);

    const auto summary = simulation.construction_summary();
    VIBECITY_CHECK(summary.sites == 1);
    VIBECITY_CHECK(summary.waiting_materials == 1);
    VIBECITY_CHECK(summary.waiting_logistics == 0);
    VIBECITY_CHECK(summary.waiting_builders == 0);
}

void output_storage_full_blocks_production()
{
    vibecity::Simulation simulation;

    const auto house = simulation.add_building(vibecity::BuildingKind::House);
    const auto farm = simulation.add_building(vibecity::BuildingKind::Farm);

    simulation.set_residents(house, 5);
    simulation.run_for(3 * 480);
    simulation.tick();

    const auto& farm_instance = simulation.building(farm);
    VIBECITY_CHECK(farm_instance.inventory.quantity(vibecity::ResourceId::Grain) == 36);
    VIBECITY_CHECK(farm_instance.blocking_reason == vibecity::BlockingReason::OutputStorageFull);
}

}

int main()
{
    default_map_resources_are_deterministic();
    terrain_blocks_paths_and_buildings();
    map_resources_follow_supported_terrain();
    building_placement_can_require_terrain();
    paths_and_buildings_clear_map_resources();
    path_and_building_removal_free_tiles_without_regrowing_resources();
    harvesting_prefers_nearest_then_topmost_resource();
    path_distance_field_matches_pairwise_pathfinding();
    path_distance_field_can_be_reused_without_stale_distances();
    path_route_follows_connected_road_tiles();
    path_connectivity_labels_building_access_components();
    demolishing_building_frees_tiles_and_cancels_transport();
    farm_produces_grain_when_staffed();
    bakery_consumes_inputs_and_produces_bread();
    house_consumes_bread_daily();
    house_records_hunger_when_bread_is_missing();
    settlement_population_facts_track_housing_and_food_need();
    population_grows_into_free_housing_when_bread_is_available();
    population_does_not_grow_when_housing_is_full();
    population_does_not_grow_when_bread_is_missing();
    population_does_not_grow_when_houses_are_hungry();
    logistics_delivers_bread_from_storehouse_to_house();
    logistics_summary_tracks_reservations_and_in_transit_goods();
    transport_job_keeps_dispatched_delivery_duration();
    disconnected_buildings_cannot_exchange_goods();
    connected_paths_allow_goods_exchange();
    logistics_prefers_nearest_reachable_source();
    logistics_breaks_equal_distance_ties_by_building_id();
    disconnected_workers_do_not_staff_workplaces();
    bakery_fetches_inputs_before_producing();
    farm_and_woodcutter_supply_bakery_chain();
    woodcutter_harvests_finite_nearby_forest();
    woodcutter_ignores_forest_outside_collection_radius();
    quarry_harvests_finite_nearby_stone();
    construction_site_fetches_materials_and_completes();
    construction_summary_reports_queue_focus();
    construction_site_reports_missing_materials();
    output_storage_full_blocks_production();

    std::cout << "simulation tests passed\n";
    return 0;
}
