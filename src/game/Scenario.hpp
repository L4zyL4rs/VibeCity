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

[[nodiscard]] StartingVillageIds create_starting_village(GameSession& game);

}
