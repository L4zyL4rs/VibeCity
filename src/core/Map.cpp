#include "core/Map.hpp"

#include <algorithm>
#include <array>
#include <queue>
#include <stdexcept>
#include <tuple>

namespace vibecity {
namespace {

constexpr std::array<GridPosition, 4> neighbor_offsets{{
    GridPosition{1, 0},
    GridPosition{-1, 0},
    GridPosition{0, 1},
    GridPosition{0, -1}
}};

std::uint32_t tile_hash(GridPosition position, std::uint32_t seed = 0)
{
    auto value = static_cast<std::uint32_t>(position.x) * 0x9e3779b9U;
    value ^= static_cast<std::uint32_t>(position.y) * 0x85ebca6bU;
    value ^= seed * 0x27d4eb2dU;
    value ^= value >> 16U;
    value *= 0x7feb352dU;
    value ^= value >> 15U;
    return value;
}

bool skipped_by_hash(GridPosition position, std::uint32_t seed, std::uint32_t skip_mod)
{
    return skip_mod != 0 && tile_hash(position, seed) % skip_mod == 0U;
}

bool patch_settings_are_valid(const PatchGenerationSettings& settings)
{
    return settings.spacing_x > 0 && settings.spacing_y > 0 && settings.radius >= 0;
}

}

std::optional<TerrainId> terrain_id_from_string(std::string_view id)
{
    if (id == terrain_name(TerrainId::Grass)) {
        return TerrainId::Grass;
    }
    if (id == terrain_name(TerrainId::Fertile)) {
        return TerrainId::Fertile;
    }
    if (id == terrain_name(TerrainId::Rocky)) {
        return TerrainId::Rocky;
    }
    if (id == terrain_name(TerrainId::ShallowWater)) {
        return TerrainId::ShallowWater;
    }
    return std::nullopt;
}

std::optional<MapResourceId> map_resource_id_from_string(std::string_view id)
{
    if (id == map_resource_name(MapResourceId::Forest)) {
        return MapResourceId::Forest;
    }
    if (id == map_resource_name(MapResourceId::Stone)) {
        return MapResourceId::Stone;
    }
    if (id == map_resource_name(MapResourceId::Clay)) {
        return MapResourceId::Clay;
    }
    return std::nullopt;
}

std::optional<int> PathDistanceField::distance_to_building(GridPosition position, Footprint footprint) const
{
    if (footprint.width <= 0 || footprint.height <= 0) {
        return std::nullopt;
    }

    auto best = std::optional<int>{};
    const auto consider = [this, &best](GridPosition access) {
        const auto distance = distance_at(access);
        if (distance.has_value() && (!best.has_value() || *distance < *best)) {
            best = distance;
        }
    };

    for (int x = position.x; x < position.x + footprint.width; ++x) {
        consider(GridPosition{x, position.y - 1});
        consider(GridPosition{x, position.y + footprint.height});
    }

    for (int y = position.y; y < position.y + footprint.height; ++y) {
        consider(GridPosition{position.x - 1, y});
        consider(GridPosition{position.x + footprint.width, y});
    }

    return best;
}

bool PathDistanceField::in_bounds(GridPosition position) const
{
    return position.x >= 0 && position.y >= 0 && position.x < width_ && position.y < height_;
}

int PathDistanceField::index(GridPosition position) const
{
    return position.y * width_ + position.x;
}

std::optional<int> PathDistanceField::distance_at(GridPosition position) const
{
    if (!in_bounds(position)) {
        return std::nullopt;
    }

    const auto distance = distances_[static_cast<std::size_t>(index(position))];
    return distance >= 0 ? std::optional<int>{distance} : std::nullopt;
}

void PathDistanceField::reset(int width, int height, std::size_t tile_count)
{
    if (width_ != width || height_ != height || distances_.size() != tile_count) {
        width_ = width;
        height_ = height;
        distances_.assign(tile_count, -1);
        visited_indices_.clear();
        frontier_.clear();
        return;
    }

    for (const auto visited_index : visited_indices_) {
        distances_[static_cast<std::size_t>(visited_index)] = -1;
    }
    visited_indices_.clear();
    frontier_.clear();
}

void PathDistanceField::visit(GridPosition position, int distance)
{
    const auto tile_index = index(position);
    if (distances_[static_cast<std::size_t>(tile_index)] != -1) {
        return;
    }

    distances_[static_cast<std::size_t>(tile_index)] = distance;
    visited_indices_.push_back(tile_index);
    frontier_.push_back(position);
}

std::vector<int> PathConnectivityMap::components_touching_building(
    GridPosition position,
    Footprint footprint) const
{
    auto result = std::vector<int>{};
    if (footprint.width <= 0 || footprint.height <= 0) {
        return result;
    }

    const auto consider = [this, &result](GridPosition access) {
        const auto component = component_at(access);
        if (component >= 0 && std::find(result.begin(), result.end(), component) == result.end()) {
            result.push_back(component);
        }
    };

    for (int x = position.x; x < position.x + footprint.width; ++x) {
        consider(GridPosition{x, position.y - 1});
        consider(GridPosition{x, position.y + footprint.height});
    }

    for (int y = position.y; y < position.y + footprint.height; ++y) {
        consider(GridPosition{position.x - 1, y});
        consider(GridPosition{position.x + footprint.width, y});
    }

    std::sort(result.begin(), result.end());
    return result;
}

bool PathConnectivityMap::in_bounds(GridPosition position) const
{
    return position.x >= 0 && position.y >= 0 && position.x < width_ && position.y < height_;
}

int PathConnectivityMap::index(GridPosition position) const
{
    return position.y * width_ + position.x;
}

int PathConnectivityMap::component_at(GridPosition position) const
{
    if (!in_bounds(position)) {
        return -1;
    }
    return components_[static_cast<std::size_t>(index(position))];
}

TileMap::TileMap(int width, int height)
{
    if (width <= 0 || height <= 0) {
        throw std::invalid_argument("map dimensions must be positive");
    }

    width_ = width;
    height_ = height;
    tiles_.resize(static_cast<std::size_t>(width_ * height_));
    terrain_tiles_.assign(tiles_.size(), TerrainId::Grass);
    map_resource_tiles_.resize(tiles_.size());
}

int TileMap::width() const
{
    return width_;
}

int TileMap::height() const
{
    return height_;
}

bool TileMap::in_bounds(GridPosition position) const
{
    return position.x >= 0 && position.y >= 0 && position.x < width_ && position.y < height_;
}

bool TileMap::has_path(GridPosition position) const
{
    return in_bounds(position) && tile(position).path;
}

TerrainId TileMap::terrain_at(GridPosition position) const
{
    if (!in_bounds(position)) {
        return TerrainId::Count;
    }
    return terrain_tile(position);
}

std::vector<TerrainTile> TileMap::terrain_tiles() const
{
    auto result = std::vector<TerrainTile>{};
    for (int y = 0; y < height_; ++y) {
        for (int x = 0; x < width_; ++x) {
            const auto position = GridPosition{x, y};
            const auto terrain = terrain_at(position);
            if (terrain != TerrainId::Grass) {
                result.push_back(TerrainTile{
                    .position = position,
                    .terrain = terrain
                });
            }
        }
    }
    return result;
}

std::vector<GridPosition> TileMap::path_positions() const
{
    auto result = std::vector<GridPosition>{};
    for (int y = 0; y < height_; ++y) {
        for (int x = 0; x < width_; ++x) {
            const auto position = GridPosition{x, y};
            if (has_path(position)) {
                result.push_back(position);
            }
        }
    }
    return result;
}

std::optional<MapResourceId> TileMap::map_resource_at(GridPosition position) const
{
    if (!in_bounds(position)) {
        return std::nullopt;
    }
    const auto resource = map_resource_tile(position).resource;
    return resource == MapResourceId::Count
        ? std::nullopt
        : std::optional<MapResourceId>{resource};
}

Quantity TileMap::map_resource_quantity(GridPosition position) const
{
    if (!in_bounds(position)) {
        return 0;
    }
    return map_resource_tile(position).resource_quantity;
}

std::vector<MapResourceDeposit> TileMap::map_resource_deposits() const
{
    auto result = std::vector<MapResourceDeposit>{};
    for (int y = 0; y < height_; ++y) {
        for (int x = 0; x < width_; ++x) {
            const auto position = GridPosition{x, y};
            const auto& current = map_resource_tile(position);
            if (current.resource != MapResourceId::Count && current.resource_quantity > 0) {
                result.push_back(MapResourceDeposit{
                    .position = position,
                    .resource = current.resource,
                    .quantity = current.resource_quantity
                });
            }
        }
    }
    return result;
}

std::vector<GridPosition> TileMap::tiles_within_radius(
    GridPosition position,
    Footprint footprint,
    int radius) const
{
    auto result = std::vector<GridPosition>{};
    if (footprint.width <= 0 || footprint.height <= 0 || radius < 0) {
        return result;
    }

    const auto first_x = std::clamp(position.x - radius, 0, width_);
    const auto first_y = std::clamp(position.y - radius, 0, height_);
    const auto end_x = std::clamp(position.x + footprint.width + radius, 0, width_);
    const auto end_y = std::clamp(position.y + footprint.height + radius, 0, height_);
    if (first_x >= end_x || first_y >= end_y) {
        return result;
    }
    result.reserve(static_cast<std::size_t>((end_x - first_x) * (end_y - first_y)));
    for (int y = first_y; y < end_y; ++y) {
        for (int x = first_x; x < end_x; ++x) {
            const auto current = GridPosition{x, y};
            if (distance_to_footprint(current, position, footprint) <= radius) {
                result.push_back(current);
            }
        }
    }
    return result;
}

Quantity TileMap::map_resource_quantity_within_radius(
    GridPosition position,
    Footprint footprint,
    MapResourceId resource,
    int radius) const
{
    auto quantity = Quantity{0};
    for (const auto current : tiles_within_radius(position, footprint, radius)) {
        const auto& current_tile = map_resource_tile(current);
        if (current_tile.resource == resource) {
            quantity += current_tile.resource_quantity;
        }
    }
    return quantity;
}

bool TileMap::footprint_has_map_resource(GridPosition position, Footprint footprint) const
{
    for (int y = position.y; y < position.y + footprint.height; ++y) {
        for (int x = position.x; x < position.x + footprint.width; ++x) {
            const auto current = GridPosition{x, y};
            if (in_bounds(current) && map_resource_tile(current).resource_quantity > 0) {
                return true;
            }
        }
    }
    return false;
}

bool TileMap::footprint_matches_terrain(
    GridPosition position,
    Footprint footprint,
    TerrainId terrain) const
{
    if (footprint.width <= 0 || footprint.height <= 0 || terrain == TerrainId::Count) {
        return false;
    }

    for (int y = position.y; y < position.y + footprint.height; ++y) {
        for (int x = position.x; x < position.x + footprint.width; ++x) {
            const auto current = GridPosition{x, y};
            if (!in_bounds(current) || terrain_at(current) != terrain) {
                return false;
            }
        }
    }
    return true;
}

bool TileMap::can_place_building(GridPosition position, Footprint footprint) const
{
    if (footprint.width <= 0 || footprint.height <= 0) {
        return false;
    }

    for (int y = position.y; y < position.y + footprint.height; ++y) {
        for (int x = position.x; x < position.x + footprint.width; ++x) {
            const auto current = GridPosition{x, y};
            if (!in_bounds(current)) {
                return false;
            }

            const auto& current_tile = tile(current);
            if (terrain_blocks_construction(terrain_at(current))
                || current_tile.path
                || current_tile.occupant.has_value()) {
                return false;
            }
        }
    }

    return true;
}

bool TileMap::has_path_access(GridPosition position, Footprint footprint) const
{
    return !path_access_tiles(position, footprint).empty();
}

bool TileMap::buildings_connected(GridPosition source_position,
    Footprint source_footprint,
    GridPosition destination_position,
    Footprint destination_footprint) const
{
    return path_distance_between_buildings(source_position, source_footprint, destination_position, destination_footprint).has_value();
}

PathDistanceField TileMap::path_distances_from_building(GridPosition position, Footprint footprint) const
{
    auto field = PathDistanceField{};
    populate_path_distances_from_building(field, position, footprint);
    return field;
}

void TileMap::populate_path_distances_from_building(
    PathDistanceField& field,
    GridPosition position,
    Footprint footprint) const
{
    field.reset(width_, height_, tiles_.size());
    for (const auto start : path_access_tiles(position, footprint)) {
        field.visit(start, 0);
    }

    for (std::size_t frontier_index = 0; frontier_index < field.frontier_.size(); ++frontier_index) {
        const auto current = field.frontier_[frontier_index];
        const auto current_distance = field.distances_[static_cast<std::size_t>(index(current))];
        for (const auto offset : neighbor_offsets) {
            const auto neighbor = GridPosition{current.x + offset.x, current.y + offset.y};
            if (!has_path(neighbor)) {
                continue;
            }

            field.visit(neighbor, current_distance + 1);
        }
    }
}

PathConnectivityMap TileMap::path_connectivity() const
{
    auto connectivity = PathConnectivityMap{};
    connectivity.width_ = width_;
    connectivity.height_ = height_;
    connectivity.components_.assign(tiles_.size(), -1);

    auto frontier = std::vector<GridPosition>{};
    auto component = 0;
    for (int y = 0; y < height_; ++y) {
        for (int x = 0; x < width_; ++x) {
            const auto start = GridPosition{x, y};
            const auto start_index = index(start);
            if (!has_path(start)
                || connectivity.components_[static_cast<std::size_t>(start_index)] >= 0) {
                continue;
            }

            frontier.clear();
            frontier.push_back(start);
            connectivity.components_[static_cast<std::size_t>(start_index)] = component;

            for (std::size_t frontier_index = 0; frontier_index < frontier.size(); ++frontier_index) {
                const auto current = frontier[frontier_index];
                for (const auto offset : neighbor_offsets) {
                    const auto neighbor = GridPosition{current.x + offset.x, current.y + offset.y};
                    if (!has_path(neighbor)) {
                        continue;
                    }

                    const auto neighbor_index = index(neighbor);
                    auto& neighbor_component = connectivity.components_[static_cast<std::size_t>(neighbor_index)];
                    if (neighbor_component >= 0) {
                        continue;
                    }

                    neighbor_component = component;
                    frontier.push_back(neighbor);
                }
            }

            ++component;
        }
    }

    return connectivity;
}

std::optional<int> TileMap::path_distance_between_buildings(GridPosition source_position,
    Footprint source_footprint,
    GridPosition destination_position,
    Footprint destination_footprint) const
{
    const auto starts = path_access_tiles(source_position, source_footprint);
    const auto goals = path_access_tiles(destination_position, destination_footprint);
    if (starts.empty() || goals.empty()) {
        return std::nullopt;
    }

    auto goal_tiles = std::vector<bool>(tiles_.size(), false);
    for (const auto goal : goals) {
        goal_tiles[static_cast<std::size_t>(index(goal))] = true;
    }

    auto distances = std::vector<int>(tiles_.size(), -1);
    auto queue = std::queue<GridPosition>{};
    for (const auto start : starts) {
        const auto start_index = index(start);
        if (distances[static_cast<std::size_t>(start_index)] != -1) {
            continue;
        }

        distances[static_cast<std::size_t>(start_index)] = 0;
        queue.push(start);
    }

    while (!queue.empty()) {
        const auto current = queue.front();
        queue.pop();

        const auto current_index = index(current);
        const auto current_distance = distances[static_cast<std::size_t>(current_index)];
        if (goal_tiles[static_cast<std::size_t>(current_index)]) {
            return current_distance;
        }

        for (const auto offset : neighbor_offsets) {
            const auto neighbor = GridPosition{current.x + offset.x, current.y + offset.y};
            if (!has_path(neighbor)) {
                continue;
            }

            const auto neighbor_index = index(neighbor);
            if (distances[static_cast<std::size_t>(neighbor_index)] != -1) {
                continue;
            }

            distances[static_cast<std::size_t>(neighbor_index)] = current_distance + 1;
            queue.push(neighbor);
        }
    }

    return std::nullopt;
}

std::vector<GridPosition> TileMap::path_between_buildings(
    GridPosition source_position,
    Footprint source_footprint,
    GridPosition destination_position,
    Footprint destination_footprint) const
{
    const auto starts = path_access_tiles(source_position, source_footprint);
    const auto goals = path_access_tiles(destination_position, destination_footprint);
    if (starts.empty() || goals.empty()) {
        return {};
    }

    auto goal_tiles = std::vector<bool>(tiles_.size(), false);
    for (const auto goal : goals) {
        goal_tiles[static_cast<std::size_t>(index(goal))] = true;
    }

    auto predecessors = std::vector<int>(tiles_.size(), -1);
    auto queue = std::queue<GridPosition>{};
    for (const auto start : starts) {
        const auto start_index = index(start);
        if (predecessors[static_cast<std::size_t>(start_index)] != -1) {
            continue;
        }

        predecessors[static_cast<std::size_t>(start_index)] = start_index;
        queue.push(start);
    }

    while (!queue.empty()) {
        const auto current = queue.front();
        queue.pop();

        const auto current_index = index(current);
        if (goal_tiles[static_cast<std::size_t>(current_index)]) {
            auto route = std::vector<GridPosition>{};
            auto route_index = current_index;
            while (true) {
                route.push_back(GridPosition{
                    .x = route_index % width_,
                    .y = route_index / width_
                });
                const auto predecessor = predecessors[static_cast<std::size_t>(route_index)];
                if (predecessor == route_index) {
                    break;
                }
                route_index = predecessor;
            }
            std::reverse(route.begin(), route.end());
            return route;
        }

        for (const auto offset : neighbor_offsets) {
            const auto neighbor = GridPosition{current.x + offset.x, current.y + offset.y};
            if (!has_path(neighbor)) {
                continue;
            }

            const auto neighbor_index = index(neighbor);
            if (predecessors[static_cast<std::size_t>(neighbor_index)] != -1) {
                continue;
            }

            predecessors[static_cast<std::size_t>(neighbor_index)] = current_index;
            queue.push(neighbor);
        }
    }

    return {};
}

void TileMap::generate_world(const WorldGenerationSettings& settings)
{
    for (auto& resource_tile : map_resource_tiles_) {
        resource_tile.resource = MapResourceId::Count;
        resource_tile.resource_quantity = 0;
    }
    generate_terrain(settings);
    generate_map_resources(settings);
}

void TileMap::generate_terrain(const WorldGenerationSettings& settings)
{
    std::fill(terrain_tiles_.begin(), terrain_tiles_.end(), TerrainId::Grass);
    for (int y = 0; y < height_; ++y) {
        for (int x = 0; x < width_; ++x) {
            auto& resource_tile = map_resource_tile(GridPosition{x, y});
            if (resource_tile.resource != MapResourceId::Count
                && !terrain_supports_map_resource(TerrainId::Grass, resource_tile.resource)) {
                resource_tile.resource = MapResourceId::Count;
                resource_tile.resource_quantity = 0;
            }
        }
    }

    if (settings.fertile.enabled && patch_settings_are_valid(settings.fertile)) {
        const auto fertile_radius_squared = settings.fertile.radius * settings.fertile.radius;
        for (int center_y = settings.fertile.start_y; center_y < height_; center_y += settings.fertile.spacing_y) {
            for (int center_x = settings.fertile.start_x; center_x < width_; center_x += settings.fertile.spacing_x) {
                for (int y = center_y - settings.fertile.radius; y <= center_y + settings.fertile.radius; ++y) {
                    for (int x = center_x - settings.fertile.radius; x <= center_x + settings.fertile.radius; ++x) {
                        const auto position = GridPosition{x, y};
                        const auto dx = x - center_x;
                        const auto dy = y - center_y;
                        if (!in_bounds(position)
                            || dx * dx + dy * dy > fertile_radius_squared
                            || skipped_by_hash(position, settings.seed, settings.fertile.skip_mod)) {
                            continue;
                        }
                        set_terrain(position, TerrainId::Fertile);
                    }
                }
            }
        }
    }

    if (settings.starter_fertile_strip) {
        for (int y = 1; y <= 3 && y < height_; ++y) {
            for (int x = 1; x < std::min(width_, 40); ++x) {
                set_terrain(GridPosition{x, y}, TerrainId::Fertile);
            }
        }
    }

    if (settings.rocky.enabled && patch_settings_are_valid(settings.rocky)) {
        const auto rocky_radius_squared = settings.rocky.radius * settings.rocky.radius;
        for (int center_y = settings.rocky.start_y; center_y < height_; center_y += settings.rocky.spacing_y) {
            for (int center_x = settings.rocky.start_x; center_x < width_; center_x += settings.rocky.spacing_x) {
                for (int y = center_y - settings.rocky.radius; y <= center_y + settings.rocky.radius; ++y) {
                    for (int x = center_x - settings.rocky.radius; x <= center_x + settings.rocky.radius; ++x) {
                        const auto position = GridPosition{x, y};
                        const auto dx = x - center_x;
                        const auto dy = y - center_y;
                        if (!in_bounds(position)
                            || dx * dx + dy * dy > rocky_radius_squared
                            || skipped_by_hash(position, settings.seed, settings.rocky.skip_mod)) {
                            continue;
                        }
                        set_terrain(position, TerrainId::Rocky);
                    }
                }
            }
        }
    }
}

void TileMap::generate_default_terrain()
{
    generate_terrain(WorldGenerationSettings{});
}

bool TileMap::set_terrain(GridPosition position, TerrainId terrain)
{
    if (!in_bounds(position) || terrain == TerrainId::Count) {
        return false;
    }

    const auto& map_tile = tile(position);
    if (map_tile.path || map_tile.occupant.has_value()) {
        return false;
    }

    terrain_tile(position) = terrain;
    auto& resource_tile = map_resource_tile(position);
    if (resource_tile.resource != MapResourceId::Count
        && !terrain_supports_map_resource(terrain, resource_tile.resource)) {
        resource_tile.resource = MapResourceId::Count;
        resource_tile.resource_quantity = 0;
    }
    return true;
}

void TileMap::generate_map_resources(const WorldGenerationSettings& settings)
{
    for (auto& resource_tile : map_resource_tiles_) {
        resource_tile.resource = MapResourceId::Count;
        resource_tile.resource_quantity = 0;
    }

    if (settings.forest.enabled && patch_settings_are_valid(settings.forest)) {
        const auto grove_radius_squared = settings.forest.radius * settings.forest.radius;
        for (int center_y = settings.forest.start_y; center_y < height_; center_y += settings.forest.spacing_y) {
            for (int center_x = settings.forest.start_x; center_x < width_; center_x += settings.forest.spacing_x) {
                for (int y = center_y - settings.forest.radius; y <= center_y + settings.forest.radius; ++y) {
                    for (int x = center_x - settings.forest.radius; x <= center_x + settings.forest.radius; ++x) {
                        const auto position = GridPosition{x, y};
                        const auto dx = x - center_x;
                        const auto dy = y - center_y;
                        if (!in_bounds(position)
                            || dx * dx + dy * dy > grove_radius_squared
                            || skipped_by_hash(position, settings.seed, settings.forest.skip_mod)
                            || !terrain_supports_map_resource(
                                terrain_at(position),
                                MapResourceId::Forest)) {
                            continue;
                        }
                        set_map_resource(
                            position,
                            MapResourceId::Forest,
                            forest_tile_capacity);
                    }
                }
            }
        }
    }

    if (settings.clay.enabled && patch_settings_are_valid(settings.clay)) {
        const auto clay_radius_squared = settings.clay.radius * settings.clay.radius;
        for (int center_y = settings.clay.start_y; center_y < height_; center_y += settings.clay.spacing_y) {
            for (int center_x = settings.clay.start_x; center_x < width_; center_x += settings.clay.spacing_x) {
                for (int y = center_y - settings.clay.radius; y <= center_y + settings.clay.radius; ++y) {
                    for (int x = center_x - settings.clay.radius; x <= center_x + settings.clay.radius; ++x) {
                        const auto position = GridPosition{x, y};
                        const auto dx = x - center_x;
                        const auto dy = y - center_y;
                        if (!in_bounds(position)
                            || dx * dx + dy * dy > clay_radius_squared
                            || skipped_by_hash(position, settings.seed, settings.clay.skip_mod)
                            || !terrain_supports_map_resource(
                                terrain_at(position),
                                MapResourceId::Clay)
                            || map_resource_quantity(position) > 0) {
                            continue;
                        }
                        set_map_resource(
                            position,
                            MapResourceId::Clay,
                            clay_tile_capacity);
                    }
                }
            }
        }
    }

    if (settings.stone_deposits) {
        for (int y = 0; y < height_; ++y) {
            for (int x = 0; x < width_; ++x) {
                const auto position = GridPosition{x, y};
                if (terrain_at(position) != TerrainId::Rocky
                    || skipped_by_hash(position, settings.seed, settings.stone_skip_mod)) {
                    continue;
                }
                set_map_resource(
                    position,
                    MapResourceId::Stone,
                    stone_tile_capacity);
            }
        }
    }
}

void TileMap::generate_default_map_resources()
{
    generate_map_resources(WorldGenerationSettings{});
}

bool TileMap::set_map_resource(
    GridPosition position,
    MapResourceId resource,
    Quantity quantity)
{
    if (!in_bounds(position)
        || resource == MapResourceId::Count
        || quantity < 0
        || quantity > map_resource_capacity(resource)) {
        return false;
    }

    const auto& map_tile = tile(position);
    if (map_tile.path || map_tile.occupant.has_value()) {
        return false;
    }
    auto& current = map_resource_tile(position);
    if (quantity == 0) {
        current.resource = MapResourceId::Count;
        current.resource_quantity = 0;
        return true;
    }
    if (!terrain_supports_map_resource(terrain_at(position), resource)) {
        return false;
    }

    current.resource = resource;
    current.resource_quantity = static_cast<std::uint8_t>(quantity);
    return true;
}

bool TileMap::harvest_map_resource_within_radius(
    GridPosition position,
    Footprint footprint,
    MapResourceId resource,
    int radius,
    Quantity quantity)
{
    if (quantity <= 0
        || map_resource_quantity_within_radius(position, footprint, resource, radius) < quantity) {
        return false;
    }

    auto candidates = tiles_within_radius(position, footprint, radius);
    std::sort(candidates.begin(), candidates.end(), [&](GridPosition left, GridPosition right) {
        return std::tuple{
            distance_to_footprint(left, position, footprint),
            left.y,
            left.x
        } < std::tuple{
            distance_to_footprint(right, position, footprint),
            right.y,
            right.x
        };
    });

    auto remaining = quantity;
    for (const auto candidate : candidates) {
        auto& current = map_resource_tile(candidate);
        if (current.resource != resource || current.resource_quantity <= 0) {
            continue;
        }

        const auto harvested = std::min(
            remaining,
            static_cast<Quantity>(current.resource_quantity));
        current.resource_quantity -= static_cast<std::uint8_t>(harvested);
        remaining -= harvested;
        if (current.resource_quantity == 0) {
            current.resource = MapResourceId::Count;
        }
        if (remaining == 0) {
            return true;
        }
    }
    return false;
}

bool TileMap::add_path(GridPosition position)
{
    if (!in_bounds(position) || terrain_blocks_construction(terrain_at(position))) {
        return false;
    }

    auto& path_tile = tile(position);
    if (path_tile.occupant.has_value()) {
        return false;
    }

    path_tile.path = true;
    auto& resource_tile = map_resource_tile(position);
    resource_tile.resource = MapResourceId::Count;
    resource_tile.resource_quantity = 0;
    return true;
}

bool TileMap::remove_path(GridPosition position)
{
    if (!in_bounds(position)) {
        return false;
    }

    auto& path_tile = tile(position);
    if (!path_tile.path) {
        return false;
    }

    path_tile.path = false;
    return true;
}

bool TileMap::place_building(MapOccupantId id, GridPosition position, Footprint footprint)
{
    if (!can_place_building(position, footprint)) {
        return false;
    }

    for (int y = position.y; y < position.y + footprint.height; ++y) {
        for (int x = position.x; x < position.x + footprint.width; ++x) {
            auto& current = tile(GridPosition{x, y});
            current.occupant = id;
            auto& resource_tile = map_resource_tile(GridPosition{x, y});
            resource_tile.resource = MapResourceId::Count;
            resource_tile.resource_quantity = 0;
        }
    }

    return true;
}

bool TileMap::remove_building(MapOccupantId id, GridPosition position, Footprint footprint)
{
    if (id == 0 || footprint.width <= 0 || footprint.height <= 0) {
        return false;
    }

    for (int y = position.y; y < position.y + footprint.height; ++y) {
        for (int x = position.x; x < position.x + footprint.width; ++x) {
            const auto current = GridPosition{x, y};
            if (!in_bounds(current) || tile(current).occupant != id) {
                return false;
            }
        }
    }

    for (int y = position.y; y < position.y + footprint.height; ++y) {
        for (int x = position.x; x < position.x + footprint.width; ++x) {
            tile(GridPosition{x, y}).occupant = std::nullopt;
        }
    }

    return true;
}

int TileMap::index(GridPosition position) const
{
    return position.y * width_ + position.x;
}

const TileMap::Tile& TileMap::tile(GridPosition position) const
{
    return tiles_[static_cast<std::size_t>(index(position))];
}

TileMap::Tile& TileMap::tile(GridPosition position)
{
    return tiles_[static_cast<std::size_t>(index(position))];
}

TerrainId& TileMap::terrain_tile(GridPosition position)
{
    return terrain_tiles_[static_cast<std::size_t>(index(position))];
}

TerrainId TileMap::terrain_tile(GridPosition position) const
{
    return terrain_tiles_[static_cast<std::size_t>(index(position))];
}

const TileMap::MapResourceTile& TileMap::map_resource_tile(GridPosition position) const
{
    return map_resource_tiles_[static_cast<std::size_t>(index(position))];
}

TileMap::MapResourceTile& TileMap::map_resource_tile(GridPosition position)
{
    return map_resource_tiles_[static_cast<std::size_t>(index(position))];
}

int TileMap::distance_to_footprint(
    GridPosition tile_position,
    GridPosition position,
    Footprint footprint)
{
    const auto end_x = position.x + footprint.width - 1;
    const auto end_y = position.y + footprint.height - 1;
    const auto dx = tile_position.x < position.x
        ? position.x - tile_position.x
        : std::max(0, tile_position.x - end_x);
    const auto dy = tile_position.y < position.y
        ? position.y - tile_position.y
        : std::max(0, tile_position.y - end_y);
    return dx + dy;
}

std::vector<GridPosition> TileMap::path_access_tiles(GridPosition position, Footprint footprint) const
{
    auto result = std::vector<GridPosition>{};

    for (int x = position.x; x < position.x + footprint.width; ++x) {
        result.push_back(GridPosition{x, position.y - 1});
        result.push_back(GridPosition{x, position.y + footprint.height});
    }

    for (int y = position.y; y < position.y + footprint.height; ++y) {
        result.push_back(GridPosition{position.x - 1, y});
        result.push_back(GridPosition{position.x + footprint.width, y});
    }

    std::erase_if(result, [this](GridPosition access) {
        return !has_path(access);
    });

    std::sort(result.begin(), result.end(), [](GridPosition left, GridPosition right) {
        return std::tie(left.y, left.x) < std::tie(right.y, right.x);
    });
    result.erase(std::unique(result.begin(), result.end()), result.end());
    return result;
}

}
