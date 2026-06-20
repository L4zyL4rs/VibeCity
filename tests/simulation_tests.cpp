#include "TestSupport.hpp"

#include "core/Simulation.hpp"

#include <iostream>
#include <optional>

namespace {

void farm_produces_grain_when_staffed()
{
    vibecity::Simulation simulation;

    const auto house = simulation.add_building(vibecity::BuildingKind::House);
    const auto farm = simulation.add_building(vibecity::BuildingKind::Farm);

    simulation.set_residents(house, 5);
    simulation.run_for(720);

    const auto& farm_instance = simulation.building(farm);
    VIBECITY_CHECK(farm_instance.inventory.quantity(vibecity::ResourceId::Grain) == 20);
    VIBECITY_CHECK(farm_instance.blocking_reason == vibecity::BlockingReason::None);
    VIBECITY_CHECK(simulation.stats().produced[vibecity::resource_index(vibecity::ResourceId::Grain)] == 20);
}

void bakery_consumes_inputs_and_produces_bread()
{
    vibecity::Simulation simulation;

    const auto house = simulation.add_building(vibecity::BuildingKind::House);
    const auto bakery = simulation.add_building(vibecity::BuildingKind::Bakery);

    simulation.set_residents(house, 5);
    simulation.building(bakery).inventory.add(vibecity::ResourceId::Grain, 6);
    simulation.building(bakery).inventory.add(vibecity::ResourceId::Firewood, 1);

    simulation.run_for(180);

    const auto& bakery_instance = simulation.building(bakery);
    VIBECITY_CHECK(bakery_instance.inventory.quantity(vibecity::ResourceId::Grain) == 0);
    VIBECITY_CHECK(bakery_instance.inventory.quantity(vibecity::ResourceId::Firewood) == 0);
    VIBECITY_CHECK(bakery_instance.inventory.quantity(vibecity::ResourceId::Bread) == 12);
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
    simulation.building(storehouse).inventory.add(vibecity::ResourceId::Firewood, 1);

    simulation.run_for(240);

    const auto& bakery_instance = simulation.building(bakery);
    VIBECITY_CHECK(bakery_instance.inventory.quantity(vibecity::ResourceId::Grain) == 0);
    VIBECITY_CHECK(bakery_instance.inventory.quantity(vibecity::ResourceId::Firewood) == 0);
    VIBECITY_CHECK(simulation.stats().produced[vibecity::resource_index(vibecity::ResourceId::Bread)] == 12);
    VIBECITY_CHECK(simulation.stats().transported[vibecity::resource_index(vibecity::ResourceId::Grain)] == 6);
    VIBECITY_CHECK(simulation.stats().transported[vibecity::resource_index(vibecity::ResourceId::Firewood)] == 1);
}

void construction_site_fetches_materials_and_completes()
{
    vibecity::Simulation simulation;

    const auto house = simulation.add_building(vibecity::BuildingKind::House);
    const auto storehouse = simulation.add_building(vibecity::BuildingKind::Storehouse);
    const auto site = simulation.place_construction(vibecity::BuildingKind::Woodcutter);

    simulation.set_residents(house, 5);
    simulation.building(storehouse).inventory.add(vibecity::ResourceId::Timber, 8);

    simulation.run_for(1'100);

    const auto& completed = simulation.building(site);
    const auto& storehouse_instance = simulation.building(storehouse);
    VIBECITY_CHECK(completed.kind == vibecity::BuildingKind::Woodcutter);
    VIBECITY_CHECK(completed.position.has_value());
    VIBECITY_CHECK(completed.construction_target == std::nullopt);
    VIBECITY_CHECK(storehouse_instance.inventory.quantity(vibecity::ResourceId::Timber) == 0);
    VIBECITY_CHECK(simulation.stats().constructed_buildings == 1);
    VIBECITY_CHECK(simulation.stats().transported[vibecity::resource_index(vibecity::ResourceId::Timber)] == 8);
}

void construction_site_reports_missing_materials()
{
    vibecity::Simulation simulation;

    const auto house = simulation.add_building(vibecity::BuildingKind::House);
    const auto site = simulation.place_construction(vibecity::BuildingKind::Woodcutter);

    simulation.set_residents(house, 5);
    simulation.run_for(20);

    const auto& construction_site = simulation.building(site);
    VIBECITY_CHECK(construction_site.kind == vibecity::BuildingKind::ConstructionSite);
    VIBECITY_CHECK(construction_site.blocking_reason == vibecity::BlockingReason::NoReachableSource);
    VIBECITY_CHECK(simulation.stats().constructed_buildings == 0);
}

void output_storage_full_blocks_production()
{
    vibecity::Simulation simulation;

    const auto house = simulation.add_building(vibecity::BuildingKind::House);
    const auto farm = simulation.add_building(vibecity::BuildingKind::Farm);

    simulation.set_residents(house, 5);
    simulation.run_for(2 * 720);
    simulation.tick();

    const auto& farm_instance = simulation.building(farm);
    VIBECITY_CHECK(farm_instance.inventory.quantity(vibecity::ResourceId::Grain) == 40);
    VIBECITY_CHECK(farm_instance.blocking_reason == vibecity::BlockingReason::OutputStorageFull);
}

}

int main()
{
    farm_produces_grain_when_staffed();
    bakery_consumes_inputs_and_produces_bread();
    house_consumes_bread_daily();
    house_records_hunger_when_bread_is_missing();
    settlement_population_facts_track_housing_and_food_need();
    logistics_delivers_bread_from_storehouse_to_house();
    disconnected_buildings_cannot_exchange_goods();
    connected_paths_allow_goods_exchange();
    logistics_prefers_nearest_reachable_source();
    disconnected_workers_do_not_staff_workplaces();
    bakery_fetches_inputs_before_producing();
    construction_site_fetches_materials_and_completes();
    construction_site_reports_missing_materials();
    output_storage_full_blocks_production();

    std::cout << "simulation tests passed\n";
    return 0;
}
