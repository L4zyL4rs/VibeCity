#include "core/Simulation.hpp"

#include <cassert>
#include <iostream>

namespace {

void farm_produces_grain_when_staffed()
{
    vibecity::Simulation simulation;

    const auto house = simulation.add_building(vibecity::BuildingKind::House);
    const auto farm = simulation.add_building(vibecity::BuildingKind::Farm);

    simulation.set_residents(house, 5);
    simulation.run_for(720);

    const auto& farm_instance = simulation.building(farm);
    assert(farm_instance.inventory.quantity(vibecity::ResourceId::Grain) == 20);
    assert(farm_instance.blocking_reason == vibecity::BlockingReason::None);
    assert(simulation.stats().produced[vibecity::resource_index(vibecity::ResourceId::Grain)] == 20);
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
    assert(bakery_instance.inventory.quantity(vibecity::ResourceId::Grain) == 0);
    assert(bakery_instance.inventory.quantity(vibecity::ResourceId::Firewood) == 0);
    assert(bakery_instance.inventory.quantity(vibecity::ResourceId::Bread) == 12);
    assert(bakery_instance.blocking_reason == vibecity::BlockingReason::None);
}

void house_consumes_bread_daily()
{
    vibecity::Simulation simulation;

    const auto house = simulation.add_building(vibecity::BuildingKind::House);
    simulation.set_residents(house, 5);
    simulation.building(house).inventory.add(vibecity::ResourceId::Bread, 5);

    simulation.run_for(vibecity::ticks_per_day);

    const auto& house_instance = simulation.building(house);
    assert(house_instance.inventory.quantity(vibecity::ResourceId::Bread) == 0);
    assert(house_instance.hunger_days == 0);
    assert(simulation.stats().consumed[vibecity::resource_index(vibecity::ResourceId::Bread)] == 5);
}

void house_records_hunger_when_bread_is_missing()
{
    vibecity::Simulation simulation;

    const auto house = simulation.add_building(vibecity::BuildingKind::House);
    simulation.set_residents(house, 5);
    simulation.building(house).inventory.add(vibecity::ResourceId::Bread, 2);

    simulation.run_for(vibecity::ticks_per_day);

    const auto& house_instance = simulation.building(house);
    assert(house_instance.inventory.quantity(vibecity::ResourceId::Bread) == 0);
    assert(house_instance.hunger_days == 1);
    assert(house_instance.blocking_reason == vibecity::BlockingReason::MissingBread);
    assert(simulation.stats().consumed[vibecity::resource_index(vibecity::ResourceId::Bread)] == 2);
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
    assert(farm_instance.inventory.quantity(vibecity::ResourceId::Grain) == 40);
    assert(farm_instance.blocking_reason == vibecity::BlockingReason::OutputStorageFull);
}

}

int main()
{
    farm_produces_grain_when_staffed();
    bakery_consumes_inputs_and_produces_bread();
    house_consumes_bread_daily();
    house_records_hunger_when_bread_is_missing();
    output_storage_full_blocks_production();

    std::cout << "simulation tests passed\n";
    return 0;
}
