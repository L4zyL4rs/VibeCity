#pragma once

#include "core/Resource.hpp"

#include <cstdint>
#include <optional>
#include <string_view>
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

enum class TerrainId : std::uint8_t {
    Grass,
    Fertile,
    Rocky,
    ShallowWater,
    Count
};

struct TerrainTile {
    GridPosition position;
    TerrainId terrain = TerrainId::Grass;

    friend constexpr bool operator==(
        const TerrainTile& left,
        const TerrainTile& right) = default;
};

[[nodiscard]] constexpr std::string_view terrain_name(TerrainId terrain)
{
    switch (terrain) {
    case TerrainId::Grass:
        return "grass";
    case TerrainId::Fertile:
        return "fertile";
    case TerrainId::Rocky:
        return "rocky";
    case TerrainId::ShallowWater:
        return "shallow_water";
    case TerrainId::Count:
        return "unknown";
    }
    return "unknown";
}

[[nodiscard]] std::optional<TerrainId> terrain_id_from_string(std::string_view id);
[[nodiscard]] constexpr bool terrain_blocks_construction(TerrainId terrain)
{
    switch (terrain) {
    case TerrainId::Grass:
    case TerrainId::Fertile:
    case TerrainId::Rocky:
        return false;
    case TerrainId::ShallowWater:
    case TerrainId::Count:
        return true;
    }
    return true;
}

enum class MapResourceId : std::uint8_t {
    Forest,
    Stone,
    Count
};

inline constexpr Quantity forest_tile_capacity = 6;
inline constexpr Quantity stone_tile_capacity = 8;
static_assert(forest_tile_capacity <= 255);
static_assert(stone_tile_capacity <= 255);

struct MapResourceDeposit {
    GridPosition position;
    MapResourceId resource = MapResourceId::Forest;
    Quantity quantity = 0;

    friend constexpr bool operator==(
        const MapResourceDeposit& left,
        const MapResourceDeposit& right) = default;
};

[[nodiscard]] constexpr std::string_view map_resource_name(MapResourceId resource)
{
    switch (resource) {
    case MapResourceId::Forest:
        return "forest";
    case MapResourceId::Stone:
        return "stone";
    case MapResourceId::Count:
        return "unknown";
    }
    return "unknown";
}

[[nodiscard]] std::optional<MapResourceId> map_resource_id_from_string(std::string_view id);
[[nodiscard]] constexpr Quantity map_resource_capacity(MapResourceId resource)
{
    switch (resource) {
    case MapResourceId::Forest:
        return forest_tile_capacity;
    case MapResourceId::Stone:
        return stone_tile_capacity;
    case MapResourceId::Count:
        return 0;
    }
    return 0;
}

[[nodiscard]] constexpr bool terrain_supports_map_resource(
    TerrainId terrain,
    MapResourceId resource)
{
    switch (resource) {
    case MapResourceId::Forest:
        return terrain == TerrainId::Grass || terrain == TerrainId::Fertile;
    case MapResourceId::Stone:
        return terrain == TerrainId::Rocky;
    case MapResourceId::Count:
        return false;
    }
    return false;
}

class PathDistanceField {
public:
    [[nodiscard]] std::optional<int> distance_to_building(GridPosition position, Footprint footprint) const;

private:
    friend class TileMap;

    [[nodiscard]] bool in_bounds(GridPosition position) const;
    [[nodiscard]] int index(GridPosition position) const;
    [[nodiscard]] std::optional<int> distance_at(GridPosition position) const;
    void reset(int width, int height, std::size_t tile_count);
    void visit(GridPosition position, int distance);

    int width_ = 0;
    int height_ = 0;
    std::vector<int> distances_;
    std::vector<int> visited_indices_;
    std::vector<GridPosition> frontier_;
};

class PathConnectivityMap {
public:
    [[nodiscard]] std::vector<int> components_touching_building(
        GridPosition position,
        Footprint footprint) const;

private:
    friend class TileMap;

    [[nodiscard]] bool in_bounds(GridPosition position) const;
    [[nodiscard]] int index(GridPosition position) const;
    [[nodiscard]] int component_at(GridPosition position) const;

    int width_ = 0;
    int height_ = 0;
    std::vector<int> components_;
};

class TileMap {
public:
    TileMap(int width = 128, int height = 128);

    [[nodiscard]] int width() const;
    [[nodiscard]] int height() const;
    [[nodiscard]] bool in_bounds(GridPosition position) const;
    [[nodiscard]] TerrainId terrain_at(GridPosition position) const;
    [[nodiscard]] std::vector<TerrainTile> terrain_tiles() const;
    [[nodiscard]] bool has_path(GridPosition position) const;
    [[nodiscard]] std::vector<GridPosition> path_positions() const;
    [[nodiscard]] std::optional<MapResourceId> map_resource_at(GridPosition position) const;
    [[nodiscard]] Quantity map_resource_quantity(GridPosition position) const;
    [[nodiscard]] std::vector<MapResourceDeposit> map_resource_deposits() const;
    [[nodiscard]] std::vector<GridPosition> tiles_within_radius(
        GridPosition position,
        Footprint footprint,
        int radius) const;
    [[nodiscard]] Quantity map_resource_quantity_within_radius(
        GridPosition position,
        Footprint footprint,
        MapResourceId resource,
        int radius) const;
    [[nodiscard]] bool footprint_has_map_resource(
        GridPosition position,
        Footprint footprint) const;
    [[nodiscard]] bool can_place_building(GridPosition position, Footprint footprint) const;
    [[nodiscard]] bool has_path_access(GridPosition position, Footprint footprint) const;
    [[nodiscard]] bool buildings_connected(GridPosition source_position,
        Footprint source_footprint,
        GridPosition destination_position,
        Footprint destination_footprint) const;
    [[nodiscard]] PathDistanceField path_distances_from_building(
        GridPosition position,
        Footprint footprint) const;
    void populate_path_distances_from_building(
        PathDistanceField& field,
        GridPosition position,
        Footprint footprint) const;
    [[nodiscard]] PathConnectivityMap path_connectivity() const;
    [[nodiscard]] std::optional<int> path_distance_between_buildings(GridPosition source_position,
        Footprint source_footprint,
        GridPosition destination_position,
        Footprint destination_footprint) const;
    [[nodiscard]] std::vector<GridPosition> path_between_buildings(
        GridPosition source_position,
        Footprint source_footprint,
        GridPosition destination_position,
        Footprint destination_footprint) const;

    void generate_default_terrain();
    bool set_terrain(GridPosition position, TerrainId terrain);
    void generate_default_map_resources();
    bool set_map_resource(GridPosition position, MapResourceId resource, Quantity quantity);
    bool harvest_map_resource_within_radius(
        GridPosition position,
        Footprint footprint,
        MapResourceId resource,
        int radius,
        Quantity quantity);
    bool add_path(GridPosition position);
    bool remove_path(GridPosition position);
    bool place_building(MapOccupantId id, GridPosition position, Footprint footprint);
    bool remove_building(MapOccupantId id, GridPosition position, Footprint footprint);

private:
    struct Tile {
        bool path = false;
        std::optional<MapOccupantId> occupant;
    };

    struct MapResourceTile {
        std::uint8_t resource_quantity = 0;
        MapResourceId resource = MapResourceId::Count;
    };

    [[nodiscard]] int index(GridPosition position) const;
    [[nodiscard]] const Tile& tile(GridPosition position) const;
    [[nodiscard]] Tile& tile(GridPosition position);
    [[nodiscard]] TerrainId& terrain_tile(GridPosition position);
    [[nodiscard]] TerrainId terrain_tile(GridPosition position) const;
    [[nodiscard]] const MapResourceTile& map_resource_tile(GridPosition position) const;
    [[nodiscard]] MapResourceTile& map_resource_tile(GridPosition position);
    [[nodiscard]] static int distance_to_footprint(
        GridPosition tile,
        GridPosition position,
        Footprint footprint);
    [[nodiscard]] std::vector<GridPosition> path_access_tiles(GridPosition position, Footprint footprint) const;

    int width_ = 0;
    int height_ = 0;
    std::vector<Tile> tiles_;
    std::vector<TerrainId> terrain_tiles_;
    std::vector<MapResourceTile> map_resource_tiles_;
};

}
