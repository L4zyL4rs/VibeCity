#pragma once

#include "game/GameSession.hpp"

#include <vector>

namespace vibecity {

struct StartingVillageIds {
    std::vector<BuildingId> houses;
    BuildingId farm = 0;
    BuildingId woodcutter = 0;
    BuildingId bakery = 0;
    BuildingId storehouse = 0;
    BuildingId farm_site = 0;
};

[[nodiscard]] StartingVillageIds create_starting_village(GameSession& game);

}
