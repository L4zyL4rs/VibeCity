#pragma once

#include "core/Simulation.hpp"
#include "game/Objectives.hpp"

#include <SDL.h>

#include <optional>
#include <string>

namespace vibecity::client {

inline constexpr int inspector_width = 340;

[[nodiscard]] std::string selected_summary(const Simulation& simulation, std::optional<BuildingId> selected);

void draw_inspector(SDL_Renderer* renderer,
    const Simulation& simulation,
    const VillageObjectiveTracker& objectives,
    std::optional<BuildingId> selected);

}
