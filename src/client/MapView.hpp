#pragma once

#include "core/Building.hpp"
#include "core/Map.hpp"
#include "core/Simulation.hpp"

#include <SDL.h>

#include <cstddef>
#include <optional>
#include <vector>

namespace vibecity::client {

inline constexpr int minimum_tile_size = 8;
inline constexpr int maximum_tile_size = 32;
inline constexpr int default_tile_size = 16;

struct Camera {
    int offset_x = 320;
    int offset_y = -120;
    int tile_size = default_tile_size;
};

[[nodiscard]] GridPosition screen_to_map(int screen_x, int screen_y, Camera camera);
[[nodiscard]] SDL_Rect tile_rect(GridPosition position, Footprint footprint, Camera camera);
void zoom_camera_at(Camera& camera, int screen_x, int screen_y, int steps);
[[nodiscard]] Footprint footprint_for(const Simulation& simulation, const BuildingInstance& building);
[[nodiscard]] std::optional<BuildingId> building_at(const Simulation& simulation, GridPosition tile);
[[nodiscard]] bool can_place_path_preview(const Simulation& simulation, GridPosition tile);
[[nodiscard]] bool can_place_building_preview(const Simulation& simulation, BuildingKind target, GridPosition tile);
[[nodiscard]] Quantity gathering_resource_quantity_for_placement(
    const Simulation& simulation,
    BuildingKind target,
    GridPosition tile);

void draw_world(SDL_Renderer* renderer,
    const Simulation& simulation,
    Camera camera,
    std::optional<BuildingId> selected);

class TransportOverlay {
public:
    void update(const Simulation& simulation);
    void draw(SDL_Renderer* renderer, Camera camera) const;

    [[nodiscard]] std::size_t visual_count() const;

private:
    struct Visual {
        TransportJobId id = 0;
        ResourceId resource = ResourceId::Grain;
        Quantity quantity = 0;
        BuildingId source = 0;
        BuildingId destination = 0;
        GridPosition source_position{};
        Footprint source_footprint{};
        GridPosition destination_position{};
        Footprint destination_footprint{};
        std::vector<GridPosition> route;
        float displayed_progress = 0.0F;
        float target_progress = 0.0F;
        int arrival_frames_remaining = 0;
        bool active = false;
        bool carrying_goods = false;
        bool completing = false;
    };

    std::vector<Visual> visuals_;
};

void draw_placement_preview(SDL_Renderer* renderer,
    Camera camera,
    std::optional<GridPosition> hover_tile,
    std::optional<Footprint> footprint,
    bool valid);

void draw_building_placement_preview(SDL_Renderer* renderer,
    const Simulation& simulation,
    Camera camera,
    std::optional<GridPosition> hover_tile,
    BuildingKind target,
    bool valid);

}
