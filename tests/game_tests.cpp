#include "game/GameSession.hpp"
#include "game/Scenario.hpp"

#include <cassert>
#include <iostream>

namespace {

void command_layer_places_path_and_building()
{
    vibecity::GameSession game;

    auto result = game.execute(vibecity::PlacePathCommand{.position = vibecity::GridPosition{1, 0}});
    assert(result.success);

    result = game.execute(vibecity::PlaceBuildingCommand{
        .kind = vibecity::BuildingKind::House,
        .position = vibecity::GridPosition{1, 1}
    });
    assert(result.success);
    assert(result.building.has_value());
    assert(game.simulation().building(*result.building).kind == vibecity::BuildingKind::House);
}

void invalid_command_reports_failure()
{
    vibecity::GameSession game;

    const auto result = game.execute(vibecity::AdvanceTimeCommand{.ticks = -1});
    assert(!result.success);
}

void starting_village_runs_through_command_layer()
{
    vibecity::GameSession game;
    const auto ids = vibecity::create_starting_village(game);

    assert(ids.houses.size() == 3);
    assert(game.simulation().building(ids.storehouse).inventory.quantity(vibecity::ResourceId::Timber) == 46);

    auto result = game.execute(vibecity::AdvanceTimeCommand{.ticks = 2 * vibecity::ticks_per_day});
    assert(result.success);
    assert(game.simulation().stats().constructed_buildings == 1);
    assert(game.simulation().building(ids.farm_site).kind == vibecity::BuildingKind::Farm);
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
