#include "game/Scenario.hpp"

#include <stdexcept>

namespace vibecity {
namespace {

constexpr Quantity starting_house_bread = 6;
constexpr Quantity starting_grain = 6;
constexpr Quantity starting_firewood = 1;
constexpr Quantity starting_timber = 24;
constexpr Quantity starting_tools = 2;

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

    add_path_line(game, 0, 1, 21);

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

    ids.farm = require_building(game, PlaceBuildingCommand{
        .kind = BuildingKind::Farm,
        .position = GridPosition{7, 1}
    });
    ids.woodcutter = require_building(game, PlaceBuildingCommand{
        .kind = BuildingKind::Woodcutter,
        .position = GridPosition{10, 1}
    });
    ids.bakery = require_building(game, PlaceBuildingCommand{
        .kind = BuildingKind::Bakery,
        .position = GridPosition{13, 1}
    });
    ids.storehouse = require_building(game, PlaceBuildingCommand{
        .kind = BuildingKind::Storehouse,
        .position = GridPosition{16, 1}
    });
    ids.farm_site = require_building(game, PlaceConstructionCommand{
        .target_kind = BuildingKind::Farm,
        .position = GridPosition{19, 1}
    });

    for (const auto house : ids.houses) {
        require(game, SetResidentsCommand{.building = house, .residents = 5});
        add_stock(game, house, ResourceId::Bread, starting_house_bread);
    }

    add_stock(game, ids.storehouse, ResourceId::Grain, starting_grain);
    add_stock(game, ids.storehouse, ResourceId::Firewood, starting_firewood);
    add_stock(game, ids.storehouse, ResourceId::Timber, starting_timber);
    add_stock(game, ids.storehouse, ResourceId::Tools, starting_tools);

    return ids;
}

}
