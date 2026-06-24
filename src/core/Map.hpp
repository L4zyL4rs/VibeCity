#pragma once

#include <cstdint>
#include <optional>
#include <vector>

namespace vibecity {

using MapOccupantId = std::uint32_t;

struct GridPosition {
    int x = 0;
    int y = 0;

    friend constexpr bool operator==(GridPosition left, GridPosition right) = default;
};

struct Footprint {
    int width = 1;
    int height = 1;
};

class PathDistanceField {
public:
    [[nodiscard]] std::optional<int> distance_to_building(GridPosition position, Footprint footprint) const;

private:
    friend class TileMap;

    [[nodiscard]] bool in_bounds(GridPosition position) const;
    [[nodiscard]] int index(GridPosition position) const;
    [[nodiscard]] std::optional<int> distance_at(GridPosition position) const;

    int width_ = 0;
    int height_ = 0;
    std::vector<int> distances_;
};

class TileMap {
public:
    TileMap(int width = 128, int height = 128);

    [[nodiscard]] int width() const;
    [[nodiscard]] int height() const;
    [[nodiscard]] bool in_bounds(GridPosition position) const;
    [[nodiscard]] bool has_path(GridPosition position) const;
    [[nodiscard]] bool can_place_building(GridPosition position, Footprint footprint) const;
    [[nodiscard]] bool has_path_access(GridPosition position, Footprint footprint) const;
    [[nodiscard]] bool buildings_connected(GridPosition source_position,
        Footprint source_footprint,
        GridPosition destination_position,
        Footprint destination_footprint) const;
    [[nodiscard]] PathDistanceField path_distances_from_building(
        GridPosition position,
        Footprint footprint) const;
    [[nodiscard]] std::optional<int> path_distance_between_buildings(GridPosition source_position,
        Footprint source_footprint,
        GridPosition destination_position,
        Footprint destination_footprint) const;

    bool add_path(GridPosition position);
    bool place_building(MapOccupantId id, GridPosition position, Footprint footprint);

private:
    struct Tile {
        bool path = false;
        std::optional<MapOccupantId> occupant;
    };

    [[nodiscard]] int index(GridPosition position) const;
    [[nodiscard]] const Tile& tile(GridPosition position) const;
    [[nodiscard]] Tile& tile(GridPosition position);
    [[nodiscard]] std::vector<GridPosition> path_access_tiles(GridPosition position, Footprint footprint) const;

    int width_ = 0;
    int height_ = 0;
    std::vector<Tile> tiles_;
};

}
