#include "game/Scenario.hpp"

#include <stdexcept>

namespace vibecity {
namespace {

constexpr Quantity starting_house_bread = 10;
constexpr Quantity starting_storehouse_bread = 60;
constexpr Quantity starting_timber = 8;
constexpr Quantity starting_tools = 5;

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

void add_vertical_path_line(GameSession& game, int x, int start_y, int end_y)
{
    for (int y = start_y; y <= end_y; ++y) {
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

WorldGenerationSettings starting_village_world_generation_settings()
{
    auto settings = WorldGenerationSettings{};
    settings.seed = 500;
    settings.starter_fertile_strip = false;

    settings.fertile = PatchGenerationSettings{
        .enabled = true,
        .start_x = 20,
        .start_y = 22,
        .spacing_x = 128,
        .spacing_y = 128,
        .radius = 12,
        .skip_mod = 0
    };
    settings.rocky = PatchGenerationSettings{
        .enabled = true,
        .start_x = 20,
        .start_y = 10,
        .spacing_x = 128,
        .spacing_y = 128,
        .radius = 6,
        .skip_mod = 0
    };
    settings.forest = PatchGenerationSettings{
        .enabled = true,
        .start_x = 10,
        .start_y = 16,
        .spacing_x = 14,
        .spacing_y = 128,
        .radius = 4,
        .skip_mod = 5
    };
    settings.clay = PatchGenerationSettings{
        .enabled = true,
        .start_x = 42,
        .start_y = 25,
        .spacing_x = 128,
        .spacing_y = 128,
        .radius = 5,
        .skip_mod = 4
    };
    settings.lakes = PatchGenerationSettings{
        .enabled = true,
        .start_x = 37,
        .start_y = 12,
        .spacing_x = 128,
        .spacing_y = 128,
        .radius = 3,
        .skip_mod = 0
    };
    settings.river = RiverGenerationSettings{
        .enabled = true,
        .start = GridPosition{44, 0},
        .bend = GridPosition{38, 12},
        .end = GridPosition{45, 31},
        .use_bend = true,
        .half_width = 0
    };
    settings.stone_deposits = true;
    settings.stone_skip_mod = 3;
    return settings;
}

StartingVillageIds create_starting_village(GameSession& game)
{
    auto ids = StartingVillageIds{};

    add_path_line(game, starting_village_road_y, 1, 30);

    ids.houses.push_back(require_building(game, PlaceBuildingCommand{
        .kind = BuildingKind::House,
        .position = GridPosition{1, starting_village_building_y}
    }));
    ids.houses.push_back(require_building(game, PlaceBuildingCommand{
        .kind = BuildingKind::House,
        .position = GridPosition{3, starting_village_building_y}
    }));
    ids.houses.push_back(require_building(game, PlaceBuildingCommand{
        .kind = BuildingKind::House,
        .position = GridPosition{5, starting_village_building_y}
    }));

    ids.storehouse = require_building(game, PlaceBuildingCommand{
        .kind = BuildingKind::Storehouse,
        .position = GridPosition{7, starting_village_building_y}
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

ReferenceVillageMilestoneIds queue_reference_village_milestone(GameSession& game)
{
    auto ids = ReferenceVillageMilestoneIds{};
    const auto quarry_kind = game.simulation().building_catalog().find_kind("quarry");
    if (!quarry_kind.has_value()) {
        throw std::runtime_error("reference milestone requires quarry definition");
    }

    ids.woodcutter = require_building(game, PlaceConstructionCommand{
        .target_kind = BuildingKind::Woodcutter,
        .position = GridPosition{10, starting_village_building_y}
    });
    ids.farm = require_building(game, PlaceConstructionCommand{
        .target_kind = BuildingKind::Farm,
        .position = GridPosition{13, starting_village_building_y}
    });
    ids.bakery = require_building(game, PlaceConstructionCommand{
        .target_kind = BuildingKind::Bakery,
        .position = GridPosition{16, starting_village_building_y}
    });
    ids.first_house = require_building(game, PlaceConstructionCommand{
        .target_kind = BuildingKind::House,
        .position = GridPosition{19, starting_village_building_y}
    });
    ids.second_house = require_building(game, PlaceConstructionCommand{
        .target_kind = BuildingKind::House,
        .position = GridPosition{21, starting_village_building_y}
    });
    ids.second_woodcutter = require_building(game, PlaceConstructionCommand{
        .target_kind = BuildingKind::Woodcutter,
        .position = GridPosition{23, starting_village_building_y}
    });
    ids.second_farm = require_building(game, PlaceConstructionCommand{
        .target_kind = BuildingKind::Farm,
        .position = GridPosition{26, starting_village_building_y}
    });
    ids.second_bakery = require_building(game, PlaceConstructionCommand{
        .target_kind = BuildingKind::Bakery,
        .position = GridPosition{29, starting_village_building_y}
    });

    add_vertical_path_line(game, 20, 12, 19);
    ids.quarry = require_building(game, PlaceConstructionCommand{
        .target_kind = *quarry_kind,
        .position = GridPosition{19, 10}
    });

    add_path_line(game, starting_village_road_y, 31, 34);
    ids.second_storehouse = require_building(game, PlaceConstructionCommand{
        .target_kind = BuildingKind::Storehouse,
        .position = GridPosition{32, starting_village_building_y}
    });

    return ids;
}

}
