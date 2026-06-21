#include "client/MapView.hpp"

#include "client/Palette.hpp"
#include "client/Text.hpp"

#include <algorithm>

namespace vibecity::client {
namespace {

struct ScreenPoint {
    float x = 0.0F;
    float y = 0.0F;
};

bool contains(GridPosition position, Footprint footprint, GridPosition point)
{
    return point.x >= position.x && point.y >= position.y
        && point.x < position.x + footprint.width
        && point.y < position.y + footprint.height;
}

std::optional<ScreenPoint> building_center_screen(const Simulation& simulation, BuildingId building_id, Camera camera)
{
    for (const auto& building : simulation.buildings()) {
        if (building.id != building_id || !building.position.has_value()) {
            continue;
        }

        const auto footprint = footprint_for(building);
        return ScreenPoint{
            .x = static_cast<float>(camera.offset_x)
                + (static_cast<float>(building.position->x) + static_cast<float>(footprint.width) * 0.5F)
                    * static_cast<float>(tile_size),
            .y = static_cast<float>(camera.offset_y)
                + (static_cast<float>(building.position->y) + static_cast<float>(footprint.height) * 0.5F)
                    * static_cast<float>(tile_size)
        };
    }

    return std::nullopt;
}

void draw_construction_progress(SDL_Renderer* renderer, const BuildingInstance& building, const SDL_Rect& rect)
{
    if (building.kind != BuildingKind::ConstructionSite || building.construction_labor_required <= 0) {
        return;
    }

    const auto completed = std::clamp(
        building.construction_labor_completed,
        Tick{0},
        building.construction_labor_required);
    const auto progress = static_cast<float>(completed) / static_cast<float>(building.construction_labor_required);
    const auto track = SDL_Rect{
        .x = rect.x + 1,
        .y = rect.y + rect.h - 4,
        .w = std::max(0, rect.w - 2),
        .h = 3
    };
    const auto fill = SDL_Rect{
        .x = track.x,
        .y = track.y,
        .w = static_cast<int>(static_cast<float>(track.w) * progress),
        .h = track.h
    };

    set_color(renderer, Color{58, 47, 30, 255});
    SDL_RenderFillRect(renderer, &track);
    if (fill.w > 0) {
        set_color(renderer, Color{226, 186, 84, 255});
        SDL_RenderFillRect(renderer, &fill);
    }
}

}

GridPosition screen_to_map(int screen_x, int screen_y, Camera camera)
{
    return GridPosition{
        .x = (screen_x - camera.offset_x) / tile_size,
        .y = (screen_y - camera.offset_y) / tile_size
    };
}

SDL_Rect tile_rect(GridPosition position, Footprint footprint, Camera camera)
{
    return SDL_Rect{
        .x = camera.offset_x + position.x * tile_size,
        .y = camera.offset_y + position.y * tile_size,
        .w = footprint.width * tile_size,
        .h = footprint.height * tile_size
    };
}

Footprint footprint_for(const BuildingInstance& building)
{
    if (building.kind == BuildingKind::ConstructionSite && building.construction_target.has_value()) {
        return building_definition(*building.construction_target).footprint;
    }
    return building_definition(building.kind).footprint;
}

std::optional<BuildingId> building_at(const Simulation& simulation, GridPosition tile)
{
    for (const auto& building : simulation.buildings()) {
        if (!building.position.has_value()) {
            continue;
        }

        if (contains(*building.position, footprint_for(building), tile)) {
            return building.id;
        }
    }
    return std::nullopt;
}

bool can_place_path_preview(const Simulation& simulation, GridPosition tile)
{
    return simulation.map().in_bounds(tile)
        && !simulation.map().has_path(tile)
        && !building_at(simulation, tile).has_value();
}

bool can_place_building_preview(const Simulation& simulation, BuildingKind target, GridPosition tile)
{
    return simulation.map().can_place_building(tile, building_definition(target).footprint);
}

void draw_world(SDL_Renderer* renderer,
    const Simulation& simulation,
    Camera camera,
    std::optional<BuildingId> selected)
{
    const auto& map = simulation.map();
    set_color(renderer, Color{48, 52, 52, 255});
    for (int y = 0; y < map.height(); ++y) {
        for (int x = 0; x < map.width(); ++x) {
            const auto position = GridPosition{x, y};
            if (!map.has_path(position)) {
                continue;
            }

            auto rect = tile_rect(position, Footprint{1, 1}, camera);
            SDL_RenderFillRect(renderer, &rect);
        }
    }

    for (const auto& building : simulation.buildings()) {
        if (!building.position.has_value()) {
            continue;
        }

        const auto footprint = footprint_for(building);
        auto rect = tile_rect(*building.position, footprint, camera);
        set_color(renderer, building_color(building));
        SDL_RenderFillRect(renderer, &rect);

        if (building.blocking_reason != BlockingReason::None) {
            set_color(renderer, Color{202, 76, 66, 255});
        } else {
            set_color(renderer, Color{28, 31, 31, 255});
        }
        SDL_RenderDrawRect(renderer, &rect);

        if (building.blocking_reason != BlockingReason::None) {
            auto blocker = SDL_Rect{
                .x = rect.x + rect.w - 7,
                .y = rect.y + 1,
                .w = 6,
                .h = 6
            };
            set_color(renderer, Color{230, 74, 68, 255});
            SDL_RenderFillRect(renderer, &blocker);
            set_color(renderer, Color{18, 20, 20, 255});
            SDL_RenderDrawRect(renderer, &blocker);
        }

        draw_construction_progress(renderer, building, rect);

        if (selected.has_value() && *selected == building.id) {
            auto selected_rect = rect;
            selected_rect.x -= 2;
            selected_rect.y -= 2;
            selected_rect.w += 4;
            selected_rect.h += 4;
            set_color(renderer, Color{230, 230, 218, 255});
            SDL_RenderDrawRect(renderer, &selected_rect);
        }
    }
}

void draw_transport_jobs(SDL_Renderer* renderer, const Simulation& simulation, Camera camera)
{
    for (const auto& job : simulation.transport_jobs()) {
        const auto source = building_center_screen(simulation, job.source, camera);
        const auto destination = building_center_screen(simulation, job.destination, camera);
        if (!source.has_value() || !destination.has_value()) {
            continue;
        }

        auto color = resource_color(job.resource);
        set_color(renderer, Color{color.r, color.g, color.b, 72});
        SDL_RenderDrawLine(renderer,
            static_cast<int>(source->x),
            static_cast<int>(source->y),
            static_cast<int>(destination->x),
            static_cast<int>(destination->y));

        auto marker = *source;
        if (job.state == TransportJobState::CarryingGoods) {
            const auto total_ticks = std::max<Tick>(1, job.leg_ticks_total);
            const auto remaining = std::clamp(job.ticks_remaining, Tick{0}, total_ticks);
            const auto progress = 1.0F - static_cast<float>(remaining) / static_cast<float>(total_ticks);
            marker.x = source->x + (destination->x - source->x) * progress;
            marker.y = source->y + (destination->y - source->y) * progress;
        }

        const auto marker_size = job.state == TransportJobState::GoingToPickup ? 5 : 7;
        auto marker_rect = SDL_Rect{
            .x = static_cast<int>(marker.x) - marker_size / 2,
            .y = static_cast<int>(marker.y) - marker_size / 2,
            .w = marker_size,
            .h = marker_size
        };

        set_color(renderer, color);
        SDL_RenderFillRect(renderer, &marker_rect);
        set_color(renderer, Color{18, 20, 20, 255});
        SDL_RenderDrawRect(renderer, &marker_rect);
    }
}

void draw_placement_preview(SDL_Renderer* renderer,
    Camera camera,
    std::optional<GridPosition> hover_tile,
    std::optional<Footprint> footprint,
    bool valid)
{
    if (!hover_tile.has_value() || !footprint.has_value()) {
        return;
    }

    auto rect = tile_rect(*hover_tile, *footprint, camera);

    set_color(renderer, valid ? Color{96, 190, 116, 76} : Color{220, 76, 72, 76});
    SDL_RenderFillRect(renderer, &rect);
    set_color(renderer, valid ? Color{140, 236, 156, 230} : Color{255, 112, 96, 230});
    SDL_RenderDrawRect(renderer, &rect);
}

}
