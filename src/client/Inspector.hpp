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

struct DiscoveryProjectAction {
    BuildingId host = 0;
    DiscoveryProjectId project = DiscoveryProjectId::PotteryExperiment;
    bool can_start = false;
    bool active = false;
    std::string label;
    std::string status;
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
[[nodiscard]] std::optional<DiscoveryProjectAction> discovery_project_action(
    const Simulation& simulation,
    std::optional<BuildingId> selected);
[[nodiscard]] std::optional<SDL_Rect> inspector_discovery_project_action_rect(
    int window_width,
    int window_height);
[[nodiscard]] std::optional<DiscoveryProjectAction> discovery_project_action_at(
    const Simulation& simulation,
    std::optional<BuildingId> selected,
    int window_width,
    int window_height,
    int screen_x,
    int screen_y);

[[nodiscard]] InspectorScrollMetrics draw_inspector(SDL_Renderer* renderer,
    const Simulation& simulation,
    const VillageObjectiveTracker& objectives,
    std::optional<BuildingId> selected,
    int requested_scroll);
void draw_discovery_project_popup(SDL_Renderer* renderer,
    const Simulation& simulation,
    std::optional<BuildingId> selected);

}
