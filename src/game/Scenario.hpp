#pragma once

#include "game/GameSession.hpp"

#include <optional>
#include <vector>

namespace vibecity {

inline constexpr int starting_village_road_y = 20;
inline constexpr int starting_village_building_y = starting_village_road_y + 1;

struct StartingVillageIds {
    std::vector<BuildingId> houses;
    std::optional<BuildingId> farm;
    std::optional<BuildingId> woodcutter;
    std::optional<BuildingId> bakery;
    BuildingId storehouse = 0;
};

struct ReferenceVillageMilestoneIds {
    BuildingId woodcutter = 0;
    BuildingId farm = 0;
    BuildingId bakery = 0;
    BuildingId first_house = 0;
    BuildingId second_house = 0;
    BuildingId second_woodcutter = 0;
    BuildingId second_farm = 0;
    BuildingId second_bakery = 0;
    BuildingId quarry = 0;
    BuildingId second_storehouse = 0;
};

[[nodiscard]] StartingVillageIds create_starting_village(GameSession& game);
[[nodiscard]] ReferenceVillageMilestoneIds queue_reference_village_milestone(GameSession& game);

}
