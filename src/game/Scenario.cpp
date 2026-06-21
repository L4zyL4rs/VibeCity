#include "game/Scenario.hpp"

#include <stdexcept>

namespace vibecity {
namespace {

constexpr Quantity starting_house_bread = 6;
constexpr Quantity starting_storehouse_bread = 24;
constexpr Quantity starting_timber = 28;
constexpr Quantity starting_tools = 1;

CommandResult require(GameSession& game, const GameCommand& command)
{
    auto result = game.execute(command);
    if (!result.success) {
        throw std::runtime_error(result.message);
    }
    return result;
}

BuildingId require_building(GameSession& game, const GameCommand& command)
{
    auto result = require(game, command);
    if (!result.building.has_value()) {
        throw std::runtime_error("scenario command did not create a building");
    }
    return *result.building;
}

void add_path_line(GameSession& game, int y, int start_x, int end_x)
{
    for (int x = start_x; x <= end_x; ++x) {
        require(game, PlacePathCommand{.position = GridPosition{x, y}});
    }
}

void add_stock(GameSession& game, BuildingId building, ResourceId resource, Quantity quantity)
{
    require(game, AddInventoryCommand{
        .building = building,
        .resource = resource,
        .quantity = quantity
    });
}

}

StartingVillageIds create_starting_village(GameSession& game)
{
    auto ids = StartingVillageIds{};

    add_path_line(game, 0, 1, 30);

    ids.houses.push_back(require_building(game, PlaceBuildingCommand{
        .kind = BuildingKind::House,
        .position = GridPosition{1, 1}
    }));
    ids.houses.push_back(require_building(game, PlaceBuildingCommand{
        .kind = BuildingKind::House,
        .position = GridPosition{3, 1}
    }));
    ids.houses.push_back(require_building(game, PlaceBuildingCommand{
        .kind = BuildingKind::House,
        .position = GridPosition{5, 1}
    }));

    ids.storehouse = require_building(game, PlaceBuildingCommand{
        .kind = BuildingKind::Storehouse,
        .position = GridPosition{7, 1}
    });

    for (const auto house : ids.houses) {
        require(game, SetResidentsCommand{.building = house, .residents = 5});
        add_stock(game, house, ResourceId::Bread, starting_house_bread);
    }

    add_stock(game, ids.storehouse, ResourceId::Bread, starting_storehouse_bread);
    add_stock(game, ids.storehouse, ResourceId::Timber, starting_timber);
    add_stock(game, ids.storehouse, ResourceId::Tools, starting_tools);

    return ids;
}

}
