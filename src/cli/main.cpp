#include "core/Simulation.hpp"

#include <iostream>

namespace {

void print_inventory(const vibecity::BuildingInstance& building)
{
    bool printed_any = false;
    for (std::size_t index = 0; index < vibecity::resource_count; ++index) {
        const auto resource = static_cast<vibecity::ResourceId>(index);
        const auto quantity = building.inventory.quantity(resource);
        if (quantity == 0) {
            continue;
        }

        if (printed_any) {
            std::cout << ", ";
        }
        std::cout << vibecity::resource_name(resource) << "=" << quantity;
        printed_any = true;
    }

    if (!printed_any) {
        std::cout << "empty";
    }
}

void print_resource_array(const vibecity::ResourceArray& values)
{
    bool printed_any = false;
    for (std::size_t index = 0; index < vibecity::resource_count; ++index) {
        const auto quantity = values[index];
        if (quantity == 0) {
            continue;
        }

        if (printed_any) {
            std::cout << ", ";
        }
        std::cout << vibecity::resource_name(static_cast<vibecity::ResourceId>(index)) << "=" << quantity;
        printed_any = true;
    }

    if (!printed_any) {
        std::cout << "none";
    }
}

}

int main()
{
    using namespace vibecity;

    Simulation simulation;

    const auto house_a = simulation.add_building(BuildingKind::House);
    const auto house_b = simulation.add_building(BuildingKind::House);
    const auto farm = simulation.add_building(BuildingKind::Farm);
    const auto woodcutter = simulation.add_building(BuildingKind::Woodcutter);
    const auto bakery = simulation.add_building(BuildingKind::Bakery);
    const auto storehouse = simulation.add_building(BuildingKind::Storehouse);

    simulation.set_residents(house_a, 5);
    simulation.set_residents(house_b, 5);

    simulation.building(house_a).inventory.add(ResourceId::Bread, 10);
    simulation.building(house_b).inventory.add(ResourceId::Bread, 10);
    simulation.building(bakery).inventory.add(ResourceId::Grain, 18);
    simulation.building(bakery).inventory.add(ResourceId::Firewood, 3);
    simulation.building(storehouse).inventory.add(ResourceId::Timber, 40);
    simulation.building(storehouse).inventory.add(ResourceId::Tools, 5);

    simulation.run_for(ticks_per_day);

    std::cout << "VibeCity headless simulation\n";
    std::cout << "day=" << simulation.current_day() << ", tick=" << simulation.current_tick() << "\n\n";

    for (const auto& building : simulation.buildings()) {
        std::cout << "#" << building.id << " " << building_kind_name(building.kind)
                  << " workers=" << building.assigned_workers
                  << " residents=" << building.residents
                  << " block=" << blocking_reason_text(building.blocking_reason)
                  << " inventory=[";
        print_inventory(building);
        std::cout << "]\n";
    }

    std::cout << "\nProduced: ";
    print_resource_array(simulation.stats().produced);
    std::cout << "\nConsumed: ";
    print_resource_array(simulation.stats().consumed);
    std::cout << "\nStored: ";
    print_resource_array(simulation.total_inventory());
    std::cout << "\n";

    return 0;
}
