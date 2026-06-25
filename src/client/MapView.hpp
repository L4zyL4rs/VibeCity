#pragma once

#include "core/Building.hpp"
#include "core/Map.hpp"
#include "core/Simulation.hpp"

#include <SDL.h>

#include <optional>

namespace vibecity::client {

inline constexpr int tile_size = 10;

struct Camera {
    int offset_x = 80;
    int offset_y = 76;
};

[[nodiscard]] GridPosition screen_to_map(int screen_x, int screen_y, Camera camera);
[[nodiscard]] SDL_Rect tile_rect(GridPosition position, Footprint footprint, Camera camera);
[[nodiscard]] Footprint footprint_for(const Simulation& simulation, const BuildingInstance& building);
[[nodiscard]] std::optional<BuildingId> building_at(const Simulation& simulation, GridPosition tile);
[[nodiscard]] bool can_place_path_preview(const Simulation& simulation, GridPosition tile);
[[nodiscard]] bool can_place_building_preview(const Simulation& simulation, BuildingKind target, GridPosition tile);

void draw_world(SDL_Renderer* renderer,
    const Simulation& simulation,
    Camera camera,
    std::optional<BuildingId> selected);

void draw_transport_jobs(SDL_Renderer* renderer, const Simulation& simulation, Camera camera);

void draw_placement_preview(SDL_Renderer* renderer,
    Camera camera,
    std::optional<GridPosition> hover_tile,
    std::optional<Footprint> footprint,
    bool valid);

}
