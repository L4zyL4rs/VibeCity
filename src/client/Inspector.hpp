#pragma once

#include "core/Simulation.hpp"
#include "game/Objectives.hpp"

#include <SDL.h>

#include <optional>
#include <string>
#include <vector>

namespace vibecity::client {

inline constexpr int inspector_width = 340;

struct InspectorScrollMetrics {
    int offset = 0;
    int max_offset = 0;
};

[[nodiscard]] std::string selected_summary(const Simulation& simulation, std::optional<BuildingId> selected);
[[nodiscard]] std::vector<std::string> selected_logistics_lines(
    const Simulation& simulation,
    BuildingId selected);
[[nodiscard]] std::vector<std::string> construction_site_detail_lines(
    const Simulation& simulation,
    BuildingId selected);
[[nodiscard]] std::vector<std::string> discovery_project_detail_lines(
    const Simulation& simulation,
    std::optional<BuildingId> selected);

[[nodiscard]] InspectorScrollMetrics draw_inspector(SDL_Renderer* renderer,
    const Simulation& simulation,
    const VillageObjectiveTracker& objectives,
    std::optional<BuildingId> selected,
    int requested_scroll);
void draw_discovery_project_popup(SDL_Renderer* renderer,
    const Simulation& simulation,
    std::optional<BuildingId> selected);

}
