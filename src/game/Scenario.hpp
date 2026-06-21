#pragma once

#include "game/GameSession.hpp"

#include <optional>
#include <vector>

namespace vibecity {

struct StartingVillageIds {
    std::vector<BuildingId> houses;
    std::optional<BuildingId> farm;
    std::optional<BuildingId> woodcutter;
    std::optional<BuildingId> bakery;
    BuildingId storehouse = 0;
};

[[nodiscard]] StartingVillageIds create_starting_village(GameSession& game);

}
