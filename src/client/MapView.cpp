#include "client/MapView.hpp"

#include "client/Palette.hpp"
#include "client/Text.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace vibecity::client {
namespace {

struct ScreenPoint {
    float x = 0.0F;
    float y = 0.0F;
};

struct TileBounds {
    int first_x = 0;
    int first_y = 0;
    int end_x = 0;
    int end_y = 0;
};

constexpr int transport_arrival_frames = 36;
constexpr float transport_progress_per_frame = 0.08F;

int floor_divide(int numerator, int denominator)
{
    auto quotient = numerator / denominator;
    const auto remainder = numerator % denominator;
    if (remainder < 0) {
        --quotient;
    }
    return quotient;
}

int ceil_divide(int numerator, int denominator)
{
    return -floor_divide(-numerator, denominator);
}

TileBounds visible_tile_bounds(const TileMap& map, Camera camera, int viewport_width, int viewport_height)
{
    return TileBounds{
        .first_x = std::clamp(
            floor_divide(-camera.offset_x, camera.tile_size),
            0,
            map.width()),
        .first_y = std::clamp(floor_divide(-camera.offset_y, camera.tile_size), 0, map.height()),
        .end_x = std::clamp(
            ceil_divide(viewport_width - camera.offset_x, camera.tile_size),
            0,
            map.width()),
        .end_y = std::clamp(
            ceil_divide(viewport_height - camera.offset_y, camera.tile_size),
            0,
            map.height())
    };
}

bool contains(GridPosition position, Footprint footprint, GridPosition point)
{
    return point.x >= position.x && point.y >= position.y
        && point.x < position.x + footprint.width
        && point.y < position.y + footprint.height;
}

ScreenPoint building_center_screen(GridPosition position, Footprint footprint, Camera camera)
{
    return ScreenPoint{
        .x = static_cast<float>(camera.offset_x)
            + (static_cast<float>(position.x) + static_cast<float>(footprint.width) * 0.5F)
                * static_cast<float>(camera.tile_size),
        .y = static_cast<float>(camera.offset_y)
            + (static_cast<float>(position.y) + static_cast<float>(footprint.height) * 0.5F)
                * static_cast<float>(camera.tile_size)
    };
}

ScreenPoint tile_center_screen(GridPosition position, Camera camera)
{
    return ScreenPoint{
        .x = static_cast<float>(camera.offset_x)
            + (static_cast<float>(position.x) + 0.5F) * static_cast<float>(camera.tile_size),
        .y = static_cast<float>(camera.offset_y)
            + (static_cast<float>(position.y) + 0.5F) * static_cast<float>(camera.tile_size)
    };
}

void draw_thick_line(SDL_Renderer* renderer, ScreenPoint start, ScreenPoint end)
{
    const auto start_x = static_cast<int>(std::lround(start.x));
    const auto start_y = static_cast<int>(std::lround(start.y));
    const auto end_x = static_cast<int>(std::lround(end.x));
    const auto end_y = static_cast<int>(std::lround(end.y));

    SDL_RenderDrawLine(renderer, start_x, start_y, end_x, end_y);
    if (std::abs(end_x - start_x) >= std::abs(end_y - start_y)) {
        SDL_RenderDrawLine(renderer, start_x, start_y - 1, end_x, end_y - 1);
        SDL_RenderDrawLine(renderer, start_x, start_y + 1, end_x, end_y + 1);
    } else {
        SDL_RenderDrawLine(renderer, start_x - 1, start_y, end_x - 1, end_y);
        SDL_RenderDrawLine(renderer, start_x + 1, start_y, end_x + 1, end_y);
    }
}

float distance(ScreenPoint start, ScreenPoint end)
{
    return std::hypot(end.x - start.x, end.y - start.y);
}

ScreenPoint interpolate(ScreenPoint start, ScreenPoint end, float progress)
{
    return ScreenPoint{
        .x = start.x + (end.x - start.x) * progress,
        .y = start.y + (end.y - start.y) * progress
    };
}

void draw_map_resources(
    SDL_Renderer* renderer,
    const TileMap& map,
    Camera camera,
    TileBounds visible)
{
    for (int y = visible.first_y; y < visible.end_y; ++y) {
        for (int x = visible.first_x; x < visible.end_x; ++x) {
            const auto position = GridPosition{x, y};
            if (map.map_resource_at(position) != MapResourceId::Forest) {
                continue;
            }

            const auto quantity = map.map_resource_quantity(position);
            const auto density = static_cast<int>(
                40 * quantity / std::max<Quantity>(1, forest_tile_capacity));
            auto rect = tile_rect(position, Footprint{1, 1}, camera);
            set_color(renderer, Color{
                static_cast<std::uint8_t>(42 + density / 4),
                static_cast<std::uint8_t>(72 + density),
                static_cast<std::uint8_t>(48 + density / 3),
                255
            });
            SDL_RenderFillRect(renderer, &rect);

            const auto maximum_crown = std::max(2, camera.tile_size - 4);
            const auto crown_size = std::max(
                2,
                static_cast<int>(
                    maximum_crown * quantity / std::max<Quantity>(1, forest_tile_capacity)));
            auto crown = SDL_Rect{
                .x = rect.x + (rect.w - crown_size) / 2,
                .y = rect.y + (rect.h - crown_size) / 2,
                .w = crown_size,
                .h = crown_size
            };
            set_color(renderer, Color{70, 122, 64, 255});
            SDL_RenderFillRect(renderer, &crown);
        }
    }
}

void draw_gathering_overlay(
    SDL_Renderer* renderer,
    const Simulation& simulation,
    Camera camera,
    std::optional<BuildingId> selected)
{
    if (!selected.has_value()) {
        return;
    }

    const auto& building = simulation.building(*selected);
    const auto& instance_definition = simulation.definition(building.kind);
    const auto* operating_definition = &instance_definition;
    if (instance_definition.internal_construction_site
        && building.construction_target.has_value()) {
        operating_definition = &simulation.definition(*building.construction_target);
    }
    if (!building.position.has_value() || !operating_definition->gathering.has_value()) {
        return;
    }

    const auto& gathering = *operating_definition->gathering;
    for (const auto position : simulation.map().tiles_within_radius(
             *building.position,
             operating_definition->footprint,
             gathering.radius)) {
        auto rect = tile_rect(position, Footprint{1, 1}, camera);
        set_color(renderer, Color{118, 180, 116, 28});
        SDL_RenderFillRect(renderer, &rect);

        if (simulation.map().map_resource_at(position) == gathering.resource) {
            rect.x += 1;
            rect.y += 1;
            rect.w = std::max(1, rect.w - 2);
            rect.h = std::max(1, rect.h - 2);
            set_color(renderer, Color{170, 224, 132, 210});
            SDL_RenderDrawRect(renderer, &rect);
        }
    }
}

void draw_construction_progress(SDL_Renderer* renderer, const BuildingInstance& building, const SDL_Rect& rect)
{
    if (!building.construction_target.has_value() || building.construction_labor_required <= 0) {
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
        .x = floor_divide(screen_x - camera.offset_x, camera.tile_size),
        .y = floor_divide(screen_y - camera.offset_y, camera.tile_size)
    };
}

SDL_Rect tile_rect(GridPosition position, Footprint footprint, Camera camera)
{
    return SDL_Rect{
        .x = camera.offset_x + position.x * camera.tile_size,
        .y = camera.offset_y + position.y * camera.tile_size,
        .w = footprint.width * camera.tile_size,
        .h = footprint.height * camera.tile_size
    };
}

void zoom_camera_at(Camera& camera, int screen_x, int screen_y, int steps)
{
    if (steps == 0) {
        return;
    }
    const auto old_size = camera.tile_size;
    const auto new_size = std::clamp(
        old_size + steps * 2,
        minimum_tile_size,
        maximum_tile_size);
    if (new_size == old_size) {
        return;
    }

    const auto map_x =
        static_cast<double>(screen_x - camera.offset_x) / static_cast<double>(old_size);
    const auto map_y =
        static_cast<double>(screen_y - camera.offset_y) / static_cast<double>(old_size);
    camera.tile_size = new_size;
    camera.offset_x = static_cast<int>(std::lround(screen_x - map_x * new_size));
    camera.offset_y = static_cast<int>(std::lround(screen_y - map_y * new_size));
}

Footprint footprint_for(const Simulation& simulation, const BuildingInstance& building)
{
    if (simulation.definition(building.kind).internal_construction_site
        && building.construction_target.has_value()) {
        return simulation.definition(*building.construction_target).footprint;
    }
    return simulation.definition(building.kind).footprint;
}

std::optional<BuildingId> building_at(const Simulation& simulation, GridPosition tile)
{
    for (const auto& building : simulation.buildings()) {
        if (!building.position.has_value()) {
            continue;
        }

        if (contains(*building.position, footprint_for(simulation, building), tile)) {
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
    return simulation.map().can_place_building(tile, simulation.definition(target).footprint);
}

void draw_world(SDL_Renderer* renderer,
    const Simulation& simulation,
    Camera camera,
    std::optional<BuildingId> selected)
{
    const auto& map = simulation.map();
    auto viewport_width = map.width() * camera.tile_size;
    auto viewport_height = map.height() * camera.tile_size;
    SDL_GetRendererOutputSize(renderer, &viewport_width, &viewport_height);
    const auto visible = visible_tile_bounds(map, camera, viewport_width, viewport_height);
    const auto viewport = SDL_Rect{.x = 0, .y = 0, .w = viewport_width, .h = viewport_height};

    auto map_rect = tile_rect(
        GridPosition{0, 0},
        Footprint{map.width(), map.height()},
        camera);
    set_color(renderer, Color{30, 35, 33, 255});
    SDL_RenderFillRect(renderer, &map_rect);
    set_color(renderer, Color{72, 78, 72, 255});
    SDL_RenderDrawRect(renderer, &map_rect);

    if (camera.tile_size >= 14) {
        set_color(renderer, Color{35, 41, 38, 255});
        for (int x = visible.first_x; x <= visible.end_x; ++x) {
            const auto screen_x = camera.offset_x + x * camera.tile_size;
            SDL_RenderDrawLine(
                renderer,
                screen_x,
                camera.offset_y + visible.first_y * camera.tile_size,
                screen_x,
                camera.offset_y + visible.end_y * camera.tile_size);
        }
        for (int y = visible.first_y; y <= visible.end_y; ++y) {
            const auto screen_y = camera.offset_y + y * camera.tile_size;
            SDL_RenderDrawLine(
                renderer,
                camera.offset_x + visible.first_x * camera.tile_size,
                screen_y,
                camera.offset_x + visible.end_x * camera.tile_size,
                screen_y);
        }
    }

    draw_map_resources(renderer, map, camera, visible);

    set_color(renderer, Color{48, 52, 52, 255});
    for (int y = visible.first_y; y < visible.end_y; ++y) {
        for (int x = visible.first_x; x < visible.end_x; ++x) {
            const auto position = GridPosition{x, y};
            if (!map.has_path(position)) {
                continue;
            }

            auto rect = tile_rect(position, Footprint{1, 1}, camera);
            SDL_RenderFillRect(renderer, &rect);
        }
    }

    draw_gathering_overlay(renderer, simulation, camera, selected);

    for (const auto& building : simulation.buildings()) {
        if (!building.position.has_value()) {
            continue;
        }

        const auto footprint = footprint_for(simulation, building);
        auto rect = tile_rect(*building.position, footprint, camera);
        if (SDL_HasIntersection(&rect, &viewport) == SDL_FALSE) {
            continue;
        }

        set_color(renderer, building_color(simulation, building));
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

void TransportOverlay::update(const Simulation& simulation)
{
    for (auto& visual : visuals_) {
        visual.active = false;
    }

    for (const auto& job : simulation.transport_jobs()) {
        const auto& source = simulation.building(job.source);
        const auto& destination = simulation.building(job.destination);
        if (!source.position.has_value() || !destination.position.has_value()) {
            continue;
        }

        const auto source_footprint = footprint_for(simulation, source);
        const auto destination_footprint = footprint_for(simulation, destination);
        auto visual = std::find_if(visuals_.begin(), visuals_.end(), [&job](const Visual& candidate) {
            return candidate.id == job.id;
        });
        const auto make_visual = [&]() {
            return Visual{
                .id = job.id,
                .resource = job.resource,
                .quantity = job.quantity,
                .source = job.source,
                .destination = job.destination,
                .source_position = *source.position,
                .source_footprint = source_footprint,
                .destination_position = *destination.position,
                .destination_footprint = destination_footprint,
                .route = simulation.map().path_between_buildings(
                    *source.position,
                    source_footprint,
                    *destination.position,
                    destination_footprint),
                .arrival_frames_remaining = transport_arrival_frames
            };
        };
        if (visual == visuals_.end()) {
            visuals_.push_back(make_visual());
            visual = std::prev(visuals_.end());
        } else if (visual->source != job.source
            || visual->destination != job.destination
            || visual->resource != job.resource
            || visual->source_position != *source.position
            || visual->destination_position != *destination.position) {
            *visual = make_visual();
        }

        visual->active = true;
        visual->quantity = job.quantity;
        if (job.state == TransportJobState::CarryingGoods) {
            visual->carrying_goods = true;
            const auto total_ticks = std::max<Tick>(1, job.leg_ticks_total);
            const auto remaining = std::clamp(job.ticks_remaining, Tick{0}, total_ticks);
            const auto progress = 1.0F - static_cast<float>(remaining) / static_cast<float>(total_ticks);
            visual->target_progress = std::max(visual->target_progress, progress);
        }
    }

    for (auto& visual : visuals_) {
        if (!visual.active) {
            visual.carrying_goods = true;
            visual.completing = true;
            visual.target_progress = 1.0F;
        }

        if (visual.carrying_goods) {
            visual.displayed_progress = std::min(
                visual.target_progress,
                visual.displayed_progress + transport_progress_per_frame);
        }

        if (visual.completing && visual.displayed_progress >= 1.0F) {
            --visual.arrival_frames_remaining;
        }
    }

    std::erase_if(visuals_, [](const Visual& visual) {
        return visual.completing
            && visual.displayed_progress >= 1.0F
            && visual.arrival_frames_remaining <= 0;
    });
}

void TransportOverlay::draw(SDL_Renderer* renderer, Camera camera) const
{
    for (const auto& visual : visuals_) {
        const auto source = building_center_screen(
            visual.source_position,
            visual.source_footprint,
            camera);
        const auto destination = building_center_screen(
            visual.destination_position,
            visual.destination_footprint,
            camera);

        const auto fade = visual.completing && visual.displayed_progress >= 1.0F
            ? std::clamp(
                static_cast<float>(visual.arrival_frames_remaining)
                    / static_cast<float>(transport_arrival_frames),
                0.0F,
                1.0F)
            : 1.0F;
        const auto color = resource_color(visual.resource);
        set_color(renderer, Color{
            color.r,
            color.g,
            color.b,
            static_cast<std::uint8_t>(96.0F * fade)
        });

        auto previous = source;
        for (const auto route_tile : visual.route) {
            const auto current = tile_center_screen(route_tile, camera);
            draw_thick_line(renderer, previous, current);
            previous = current;
        }
        draw_thick_line(renderer, previous, destination);

        auto total_distance = 0.0F;
        previous = source;
        for (const auto route_tile : visual.route) {
            const auto current = tile_center_screen(route_tile, camera);
            total_distance += distance(previous, current);
            previous = current;
        }
        total_distance += distance(previous, destination);

        const auto target_distance = total_distance * visual.displayed_progress;
        auto traversed = 0.0F;
        auto marker = source;
        previous = source;
        auto consider_segment = [&](ScreenPoint current) {
            const auto segment_distance = distance(previous, current);
            if (traversed + segment_distance >= target_distance && segment_distance > 0.0F) {
                marker = interpolate(
                    previous,
                    current,
                    (target_distance - traversed) / segment_distance);
                return true;
            }
            traversed += segment_distance;
            marker = current;
            previous = current;
            return false;
        };

        auto marker_found = false;
        for (const auto route_tile : visual.route) {
            if (consider_segment(tile_center_screen(route_tile, camera))) {
                marker_found = true;
                break;
            }
        }
        if (!marker_found) {
            consider_segment(destination);
        }

        const auto marker_size = visual.carrying_goods ? 8 : 5;
        auto marker_rect = SDL_Rect{
            .x = static_cast<int>(std::lround(marker.x)) - marker_size / 2,
            .y = static_cast<int>(std::lround(marker.y)) - marker_size / 2,
            .w = marker_size,
            .h = marker_size
        };

        set_color(renderer, Color{
            color.r,
            color.g,
            color.b,
            static_cast<std::uint8_t>(255.0F * fade)
        });
        SDL_RenderFillRect(renderer, &marker_rect);
        set_color(renderer, Color{18, 20, 20, static_cast<std::uint8_t>(255.0F * fade)});
        SDL_RenderDrawRect(renderer, &marker_rect);
    }
}

std::size_t TransportOverlay::visual_count() const
{
    return visuals_.size();
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
