#include "game/Scenario.hpp"

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

void print_transport_jobs(const vibecity::Simulation& simulation)
{
    if (simulation.transport_jobs().empty()) {
        std::cout << "none\n";
        return;
    }

    for (const auto& job : simulation.transport_jobs()) {
        std::cout << "#" << job.id
                  << " " << vibecity::resource_name(job.resource)
                  << "=" << job.quantity
                  << " " << job.source << "->" << job.destination
                  << " state=" << vibecity::transport_job_state_name(job.state)
                  << " eta=" << job.ticks_remaining
                  << "\n";
    }
}

void print_construction_details(const vibecity::BuildingInstance& building)
{
    if (building.kind != vibecity::BuildingKind::ConstructionSite || !building.construction_target.has_value()) {
        return;
    }

    std::cout << " target=" << vibecity::building_kind_name(*building.construction_target)
              << " builders=" << building.assigned_builders
              << " labor=" << building.construction_labor_completed
              << "/" << building.construction_labor_required;
}

void print_position(const vibecity::BuildingInstance& building)
{
    if (!building.position.has_value()) {
        std::cout << " pos=unplaced";
        return;
    }

    std::cout << " pos=(" << building.position->x << "," << building.position->y << ")";
}

}

int main()
{
    using namespace vibecity;

    GameSession game;
    const auto scenario = create_starting_village(game);
    const auto advance = game.execute(AdvanceTimeCommand{.ticks = 2 * ticks_per_day});
    if (!advance.success) {
        std::cerr << "failed to advance scenario: " << advance.message << "\n";
        return 1;
    }

    const auto& simulation = game.simulation();

    std::cout << "VibeCity headless simulation\n";
    std::cout << "scenario houses=" << scenario.houses.size()
              << ", storehouse=#" << scenario.storehouse
              << ", farm_site=#" << scenario.farm_site << "\n";
    std::cout << "day=" << simulation.current_day() << ", tick=" << simulation.current_tick() << "\n\n";

    for (const auto& building : simulation.buildings()) {
        std::cout << "#" << building.id << " " << building_kind_name(building.kind)
                  << " workers=" << building.assigned_workers
                  << " residents=" << building.residents
                  << " block=" << blocking_reason_text(building.blocking_reason);
        print_position(building);
        print_construction_details(building);
        std::cout << " inventory=[";
        print_inventory(building);
        std::cout << "]\n";
    }

    std::cout << "\nIdle workers: " << simulation.idle_workers()
              << "\nAvailable haulers: " << simulation.available_haulers()
              << "\nActive transport jobs:\n";
    print_transport_jobs(simulation);
    std::cout << "\nProduced: ";
    print_resource_array(simulation.stats().produced);
    std::cout << "\nConsumed: ";
    print_resource_array(simulation.stats().consumed);
    std::cout << "\nTransported: ";
    print_resource_array(simulation.stats().transported);
    std::cout << "\nConstructed buildings: " << simulation.stats().constructed_buildings;
    std::cout << "\nStored: ";
    print_resource_array(simulation.total_inventory());
    std::cout << "\n";

    return 0;
}
