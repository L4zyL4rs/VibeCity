#include "TestSupport.hpp"

#include "game/GameSession.hpp"
#include "game/Scenario.hpp"

#include <iostream>

namespace {

vibecity::CommandResult require(vibecity::GameSession& game, const vibecity::GameCommand& command)
{
    auto result = game.execute(command);
    VIBECITY_CHECK(result.success);
    return result;
}

vibecity::BuildingId require_building(vibecity::GameSession& game, const vibecity::GameCommand& command)
{
    auto result = require(game, command);
    VIBECITY_CHECK(result.building.has_value());
    return *result.building;
}

int total_hunger_days(const vibecity::Simulation& simulation)
{
    auto hunger_days = 0;
    for (const auto& building : simulation.buildings()) {
        hunger_days += building.hunger_days;
    }
    return hunger_days;
}

void command_layer_places_path_and_building()
{
    vibecity::GameSession game;

    auto result = game.execute(vibecity::PlacePathCommand{.position = vibecity::GridPosition{1, 0}});
    VIBECITY_CHECK(result.success);

    result = game.execute(vibecity::PlaceBuildingCommand{
        .kind = vibecity::BuildingKind::House,
        .position = vibecity::GridPosition{1, 1}
    });
    VIBECITY_CHECK(result.success);
    VIBECITY_CHECK(result.building.has_value());
    VIBECITY_CHECK(game.simulation().building(*result.building).kind == vibecity::BuildingKind::House);
}

void invalid_command_reports_failure()
{
    vibecity::GameSession game;

    const auto result = game.execute(vibecity::AdvanceTimeCommand{.ticks = -1});
    VIBECITY_CHECK(!result.success);
}

void starting_village_runs_through_command_layer()
{
    vibecity::GameSession game;
    const auto ids = vibecity::create_starting_village(game);

    VIBECITY_CHECK(ids.houses.size() == 3);
    VIBECITY_CHECK(game.simulation().building(ids.storehouse).inventory.quantity(vibecity::ResourceId::Timber) == 46);

    auto result = game.execute(vibecity::AdvanceTimeCommand{.ticks = 2 * vibecity::ticks_per_day});
    VIBECITY_CHECK(result.success);
    VIBECITY_CHECK(game.simulation().stats().constructed_buildings == 1);
    VIBECITY_CHECK(game.simulation().building(ids.farm_site).kind == vibecity::BuildingKind::Farm);
}

void self_sufficient_village_reaches_25_residents()
{
    vibecity::GameSession game;
    const auto ids = vibecity::create_starting_village(game);
    VIBECITY_CHECK(ids.houses.size() == 3);

    for (int x = 22; x <= 25; ++x) {
        require(game, vibecity::PlacePathCommand{.position = vibecity::GridPosition{x, 0}});
    }

    const auto first_house = require_building(game, vibecity::PlaceConstructionCommand{
        .target_kind = vibecity::BuildingKind::House,
        .position = vibecity::GridPosition{22, 1}
    });
    const auto second_house = require_building(game, vibecity::PlaceConstructionCommand{
        .target_kind = vibecity::BuildingKind::House,
        .position = vibecity::GridPosition{24, 1}
    });

    for (int day = 0; day < 16; ++day) {
        require(game, vibecity::AdvanceTimeCommand{.ticks = vibecity::ticks_per_day});
    }

    const auto& simulation = game.simulation();
    VIBECITY_CHECK(simulation.building(first_house).kind == vibecity::BuildingKind::House);
    VIBECITY_CHECK(simulation.building(second_house).kind == vibecity::BuildingKind::House);
    VIBECITY_CHECK(simulation.stats().constructed_buildings >= 3);
    VIBECITY_CHECK(simulation.total_residents() == 25);
    VIBECITY_CHECK(simulation.total_housing_capacity() == 25);
    VIBECITY_CHECK(simulation.free_housing_capacity() == 0);
    VIBECITY_CHECK(simulation.daily_bread_need() == 25);
    VIBECITY_CHECK(simulation.population_growth_blocker() == vibecity::PopulationGrowthBlocker::NoHousing);
    VIBECITY_CHECK(total_hunger_days(simulation) == 0);
    VIBECITY_CHECK(simulation.total_inventory()[vibecity::resource_index(vibecity::ResourceId::Bread)] >= 25);
}

}

int main()
{
    command_layer_places_path_and_building();
    invalid_command_reports_failure();
    starting_village_runs_through_command_layer();
    self_sufficient_village_reaches_25_residents();

    std::cout << "game tests passed\n";
    return 0;
}
