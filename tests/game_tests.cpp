#include "TestSupport.hpp"

#include "game/GameSession.hpp"
#include "game/Scenario.hpp"

#include <iostream>

namespace {

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

}

int main()
{
    command_layer_places_path_and_building();
    invalid_command_reports_failure();
    starting_village_runs_through_command_layer();

    std::cout << "game tests passed\n";
    return 0;
}
