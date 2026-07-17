#include "game/Scenario.hpp"

#include <exception>
#include <iostream>
#include <string_view>

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

void print_capabilities(const vibecity::Simulation& simulation)
{
    bool printed_any = false;
    for (std::size_t index = 0; index < vibecity::capability_count; ++index) {
        const auto capability = static_cast<vibecity::CapabilityId>(index);
        if (!simulation.has_capability(capability)) {
            continue;
        }
        if (printed_any) {
            std::cout << ", ";
        }
        std::cout << vibecity::capability_name(capability);
        printed_any = true;
    }
    if (!printed_any) {
        std::cout << "none";
    }
}

void print_discovery_projects(const vibecity::Simulation& simulation)
{
    if (simulation.discovery_projects().empty()) {
        std::cout << "none\n";
        return;
    }

    for (const auto& project : simulation.discovery_projects()) {
        const auto& definition = vibecity::discovery_project_definition(project.project);
        std::cout << definition.name
                  << " host=#" << project.host
                  << " workers=" << project.assigned_workers
                  << " labor=" << project.labor_completed
                  << "/" << definition.labor_minutes
                  << "\n";
    }
}

void print_objectives(const vibecity::VillageObjectiveTracker& objectives)
{
    std::cout << "completed=" << objectives.completed_count()
              << "/" << vibecity::village_objective_count << "\n";
    if (objectives.all_complete()) {
        std::cout << "milestone=complete\n";
    } else if (const auto* active = objectives.active_status()) {
        const auto current = active->current > active->target ? active->target : active->current;
        std::cout << "active=" << active->label;
        if (active->target > 1) {
            std::cout << " " << current << "/" << active->target;
        }
        std::cout << "\n";
    }

    for (const auto& status : objectives.statuses()) {
        const auto current = status.current > status.target ? status.target : status.current;
        std::cout << (status.complete ? "ok " : "-- ") << status.label;
        if (status.target > 1) {
            std::cout << " " << current << "/" << status.target;
        }
        std::cout << "\n";
    }
}

void print_construction_details(
    const vibecity::Simulation& simulation,
    const vibecity::BuildingInstance& building)
{
    if (!simulation.definition(building.kind).internal_construction_site
        || !building.construction_target.has_value()) {
        return;
    }

    std::cout << " target=" << simulation.definition(*building.construction_target).name
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

bool wants_milestone(int argc, char** argv)
{
    for (int index = 1; index < argc; ++index) {
        if (std::string_view{argv[index]} == "--milestone") {
            return true;
        }
    }
    return false;
}

}

int main(int argc, char** argv)
{
    using namespace vibecity;

    try {
        GameSession game{starting_village_world_generation_settings()};
        const auto scenario = create_starting_village(game);
        const auto milestone = wants_milestone(argc, argv);
        if (milestone) {
            [[maybe_unused]] const auto milestone_ids = queue_reference_village_milestone(game);
        }

        const auto advance = game.execute(AdvanceTimeCommand{
            .ticks = (milestone ? 30 : 2) * ticks_per_day
        });
        if (!advance.success) {
            std::cerr << "failed to advance scenario: " << advance.message << "\n";
            return 1;
        }

        const auto& simulation = game.simulation();

        std::cout << "VibeCity headless simulation\n";
        std::cout << "mode=" << (milestone ? "reference milestone" : "starting village 2d") << "\n";
        std::cout << "scenario houses=" << scenario.houses.size()
                  << ", storehouse=#" << scenario.storehouse << "\n";
        std::cout << "day=" << simulation.current_day()
                  << ", tick=" << simulation.current_tick()
                  << ", weather=" << weather_name(simulation.current_weather())
                  << "\n\n";

        for (const auto& building : simulation.buildings()) {
            std::cout << "#" << building.id << " " << simulation.definition(building.kind).name
                      << " workers=" << building.assigned_workers
                      << " residents=" << building.residents
                      << " block=" << blocking_reason_text(building.blocking_reason);
            print_position(building);
            print_construction_details(simulation, building);
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
        std::cout << "\nCapabilities: ";
        print_capabilities(simulation);
        std::cout << "\nDiscovery projects:\n";
        print_discovery_projects(simulation);
        std::cout << "\nStored: ";
        print_resource_array(simulation.total_inventory());
        std::cout << "\nObjectives:\n";
        print_objectives(game.objectives());
        std::cout << "\n";
    } catch (const std::exception& exception) {
        std::cerr << "headless failed: " << exception.what() << "\n";
        return 1;
    }

    return 0;
}
