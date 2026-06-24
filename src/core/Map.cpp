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

TileMap::TileMap(int width, int height)
{
    if (width <= 0 || height <= 0) {
        throw std::invalid_argument("map dimensions must be positive");
    }

    width_ = width;
    height_ = height;
    tiles_.resize(static_cast<std::size_t>(width_ * height_));
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
            if (current_tile.path || current_tile.occupant.has_value()) {
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
    field.width_ = width_;
    field.height_ = height_;
    field.distances_.assign(tiles_.size(), -1);

    auto queue = std::queue<GridPosition>{};
    for (const auto start : path_access_tiles(position, footprint)) {
        const auto start_index = index(start);
        if (field.distances_[static_cast<std::size_t>(start_index)] != -1) {
            continue;
        }

        field.distances_[static_cast<std::size_t>(start_index)] = 0;
        queue.push(start);
    }

    while (!queue.empty()) {
        const auto current = queue.front();
        queue.pop();

        const auto current_distance = field.distances_[static_cast<std::size_t>(index(current))];
        for (const auto offset : neighbor_offsets) {
            const auto neighbor = GridPosition{current.x + offset.x, current.y + offset.y};
            if (!has_path(neighbor)) {
                continue;
            }

            const auto neighbor_index = index(neighbor);
            if (field.distances_[static_cast<std::size_t>(neighbor_index)] != -1) {
                continue;
            }

            field.distances_[static_cast<std::size_t>(neighbor_index)] = current_distance + 1;
            queue.push(neighbor);
        }
    }

    return field;
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

bool TileMap::add_path(GridPosition position)
{
    if (!in_bounds(position)) {
        return false;
    }

    auto& path_tile = tile(position);
    if (path_tile.occupant.has_value()) {
        return false;
    }

    path_tile.path = true;
    return true;
}

bool TileMap::place_building(MapOccupantId id, GridPosition position, Footprint footprint)
{
    if (!can_place_building(position, footprint)) {
        return false;
    }

    for (int y = position.y; y < position.y + footprint.height; ++y) {
        for (int x = position.x; x < position.x + footprint.width; ++x) {
            tile(GridPosition{x, y}).occupant = id;
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
