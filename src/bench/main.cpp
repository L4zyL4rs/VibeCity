#include "game/GameSession.hpp"
#include "game/Scenario.hpp"

#include <chrono>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace {

struct BenchmarkResult {
    std::string_view name;
    vibecity::Tick ticks = 0;
    double milliseconds = 0.0;
    double ticks_per_second = 0.0;
    std::size_t buildings = 0;
    std::size_t active_transport_jobs = 0;
    vibecity::Quantity transported = 0;
    std::size_t map_resource_tiles = 0;
    vibecity::Quantity map_resource_quantity = 0;
    int constructed = 0;
};

vibecity::CommandResult require(vibecity::GameSession& game, const vibecity::GameCommand& command)
{
    auto result = game.execute(command);
    if (!result.success) {
        throw std::runtime_error(result.message);
    }
    return result;
}

vibecity::BuildingId require_building(vibecity::GameSession& game, const vibecity::GameCommand& command)
{
    auto result = require(game, command);
    if (!result.building.has_value()) {
        throw std::runtime_error("benchmark command did not create a building");
    }
    return *result.building;
}

void add_path_line(vibecity::GameSession& game, int y, int start_x, int end_x)
{
    for (int x = start_x; x <= end_x; ++x) {
        require(game, vibecity::PlacePathCommand{.position = vibecity::GridPosition{x, y}});
    }
}

vibecity::GridPosition placement_for_kind(
    const vibecity::GameSession& game,
    vibecity::BuildingKind kind,
    vibecity::GridPosition preferred)
{
    if (game.simulation().can_place_building_at(kind, preferred)) {
        return preferred;
    }

    const auto& map = game.simulation().map();
    for (int y = 1; y < map.height(); ++y) {
        for (int x = 1; x < map.width(); ++x) {
            const auto candidate = vibecity::GridPosition{x, y};
            if (game.simulation().can_place_building_at(kind, candidate)) {
                return candidate;
            }
        }
    }

    throw std::runtime_error("benchmark could not place building");
}

vibecity::Quantity total_transported(const vibecity::ResourceStats& stats)
{
    auto total = vibecity::Quantity{0};
    for (const auto quantity : stats.transported) {
        total += quantity;
    }
    return total;
}

vibecity::Quantity total_map_resource_quantity(
    const std::vector<vibecity::MapResourceDeposit>& deposits)
{
    auto total = vibecity::Quantity{0};
    for (const auto& deposit : deposits) {
        total += deposit.quantity;
    }
    return total;
}

void queue_self_sufficient_village(vibecity::GameSession& game)
{
    require_building(game, vibecity::PlaceConstructionCommand{
        .target_kind = vibecity::BuildingKind::Woodcutter,
        .position = vibecity::GridPosition{10, vibecity::starting_village_building_y}
    });
    require_building(game, vibecity::PlaceConstructionCommand{
        .target_kind = vibecity::BuildingKind::Farm,
        .position = vibecity::GridPosition{13, vibecity::starting_village_building_y}
    });
    require_building(game, vibecity::PlaceConstructionCommand{
        .target_kind = vibecity::BuildingKind::Bakery,
        .position = vibecity::GridPosition{16, vibecity::starting_village_building_y}
    });
    require_building(game, vibecity::PlaceConstructionCommand{
        .target_kind = vibecity::BuildingKind::House,
        .position = vibecity::GridPosition{19, vibecity::starting_village_building_y}
    });
    require_building(game, vibecity::PlaceConstructionCommand{
        .target_kind = vibecity::BuildingKind::House,
        .position = vibecity::GridPosition{21, vibecity::starting_village_building_y}
    });
}

vibecity::GameSession starting_village()
{
    auto game = vibecity::GameSession{};
    [[maybe_unused]] const auto ids = vibecity::create_starting_village(game);
    return game;
}

vibecity::GameSession construction_village()
{
    auto game = starting_village();
    queue_self_sufficient_village(game);
    return game;
}

vibecity::GameSession generated_hundred_buildings()
{
    auto game = vibecity::GameSession{};

    for (int row = 0; row < 4; ++row) {
        add_path_line(game, row * 4, 1, 126);
    }

    for (int index = 0; index < 100; ++index) {
        const auto row = index / 25;
        const auto column = index % 25;
        auto position = vibecity::GridPosition{1 + column * 5, row * 4 + 1};

        const auto kind_index = index % 10;
        auto kind = vibecity::BuildingKind::House;
        if (kind_index == 5) {
            kind = vibecity::BuildingKind::Farm;
        } else if (kind_index == 6) {
            kind = vibecity::BuildingKind::Woodcutter;
        } else if (kind_index == 7 || kind_index == 9) {
            kind = vibecity::BuildingKind::Bakery;
        } else if (kind_index == 8) {
            kind = vibecity::BuildingKind::Storehouse;
        }
        position = placement_for_kind(game, kind, position);

        const auto building = require_building(game, vibecity::PlaceBuildingCommand{
            .kind = kind,
            .position = position
        });

        if (kind == vibecity::BuildingKind::House) {
            require(game, vibecity::SetResidentsCommand{.building = building, .residents = 5});
        } else if (kind == vibecity::BuildingKind::Storehouse) {
            require(game, vibecity::AddInventoryCommand{
                .building = building,
                .resource = vibecity::ResourceId::Bread,
                .quantity = 100
            });
            require(game, vibecity::AddInventoryCommand{
                .building = building,
                .resource = vibecity::ResourceId::Grain,
                .quantity = 100
            });
            require(game, vibecity::AddInventoryCommand{
                .building = building,
                .resource = vibecity::ResourceId::Firewood,
                .quantity = 100
            });
        }
    }

    return game;
}

template <typename Setup>
BenchmarkResult run_case(std::string_view name, vibecity::Tick ticks, Setup setup)
{
    auto game = setup();
    const auto started = std::chrono::steady_clock::now();
    require(game, vibecity::AdvanceTimeCommand{.ticks = ticks});
    const auto ended = std::chrono::steady_clock::now();

    const auto elapsed = std::chrono::duration<double, std::milli>(ended - started).count();
    const auto& simulation = game.simulation();
    const auto map_resources = simulation.map().map_resource_deposits();
    return BenchmarkResult{
        .name = name,
        .ticks = ticks,
        .milliseconds = elapsed,
        .ticks_per_second = elapsed > 0.0 ? static_cast<double>(ticks) * 1'000.0 / elapsed : 0.0,
        .buildings = simulation.buildings().size(),
        .active_transport_jobs = simulation.transport_jobs().size(),
        .transported = total_transported(simulation.stats()),
        .map_resource_tiles = map_resources.size(),
        .map_resource_quantity = total_map_resource_quantity(map_resources),
        .constructed = simulation.stats().constructed_buildings
    };
}

void print_result(const BenchmarkResult& result)
{
    std::cout << std::left << std::setw(28) << result.name
              << " ticks=" << std::right << std::setw(7) << result.ticks
              << " ms=" << std::setw(9) << std::fixed << std::setprecision(2) << result.milliseconds
              << " ticks/s=" << std::setw(11) << std::fixed << std::setprecision(0) << result.ticks_per_second
              << " buildings=" << std::setw(4) << result.buildings
              << " jobs=" << std::setw(3) << result.active_transport_jobs
              << " transported=" << std::setw(6) << result.transported
              << " resource_tiles=" << std::setw(5) << result.map_resource_tiles
              << " resources=" << std::setw(6) << result.map_resource_quantity
              << " constructed=" << std::setw(3) << result.constructed
              << "\n";
}

void print_csv_header()
{
    std::cout << "case,ticks,milliseconds,ticks_per_second,buildings,active_transport_jobs,transported,map_resource_tiles,map_resource_quantity,constructed\n";
}

void print_csv_result(const BenchmarkResult& result)
{
    std::cout << result.name
              << "," << result.ticks
              << "," << std::fixed << std::setprecision(2) << result.milliseconds
              << "," << std::fixed << std::setprecision(0) << result.ticks_per_second
              << "," << result.buildings
              << "," << result.active_transport_jobs
              << "," << result.transported
              << "," << result.map_resource_tiles
              << "," << result.map_resource_quantity
              << "," << result.constructed
              << "\n";
}

bool wants_csv(int argc, char** argv)
{
    for (int index = 1; index < argc; ++index) {
        if (std::string_view{argv[index]} == "--csv") {
            return true;
        }
    }
    return false;
}

}

int main(int argc, char** argv)
{
    try {
        const auto csv = wants_csv(argc, argv);
        if (csv) {
            print_csv_header();
        }

        const auto starting = run_case("starting village 30d", 30 * vibecity::ticks_per_day, starting_village);
        const auto construction = run_case("construction village 30d", 30 * vibecity::ticks_per_day, construction_village);
        const auto generated = run_case("100 buildings 10d", 10 * vibecity::ticks_per_day, generated_hundred_buildings);

        if (csv) {
            print_csv_result(starting);
            print_csv_result(construction);
            print_csv_result(generated);
        } else {
            print_result(starting);
            print_result(construction);
            print_result(generated);
        }
    } catch (const std::exception& exception) {
        std::cerr << "benchmark failed: " << exception.what() << "\n";
        return 1;
    }

    return 0;
}
