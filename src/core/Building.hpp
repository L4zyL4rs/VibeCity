#pragma once

#include "core/Inventory.hpp"
#include "core/Map.hpp"

#include <array>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace vibecity {

enum class BuildingKind : std::uint8_t {
    House,
    Farm,
    Bakery,
    Woodcutter,
    Storehouse,
    ConstructionSite,
    Count
};

inline constexpr std::uint8_t first_custom_building_kind = 64;

enum class BlockingReason : std::uint8_t {
    None,
    NotEnoughWorkers,
    MissingInput,
    OutputStorageFull,
    MissingBread,
    WaitingForHauler,
    NoReachableSource,
    MissingConstructionMaterial,
    WaitingForBuilderLabor,
    NoNearbyMapResource
};

enum class ResourceSourcePolicy : std::uint8_t {
    None,
    RecipeOutputs,
    AllStored
};

using ResourceSourceMask = std::uint64_t;

constexpr ResourceSourceMask resource_source_bit(ResourceId resource)
{
    static_assert(resource_count < std::numeric_limits<ResourceSourceMask>::digits);
    return ResourceSourceMask{1} << resource_index(resource);
}

constexpr ResourceSourceMask all_resource_source_mask()
{
    static_assert(resource_count < std::numeric_limits<ResourceSourceMask>::digits);
    return (ResourceSourceMask{1} << resource_count) - 1;
}

using TerrainConstructionMaterials = std::array<ResourceArray, terrain_count>;

struct Recipe {
    ResourceArray inputs{};
    ResourceArray outputs{};
    Tick cycle_minutes = 0;
};

struct GatheringRule {
    MapResourceId resource = MapResourceId::Forest;
    int radius = 0;
    Quantity units_per_cycle = 0;
};

struct BuildingDefinition {
    BuildingKind kind;
    std::string stable_id;
    std::string name;
    Footprint footprint{1, 1};
    int worker_slots = 0;
    int resident_capacity = 0;
    int worker_supply = 0;
    bool consumes_bread = false;
    ResourceArray construction_materials{};
    TerrainConstructionMaterials terrain_construction_materials{};
    Tick construction_labor_minutes = 0;
    ResourceArray storage{};
    std::optional<Recipe> recipe;
    std::optional<GatheringRule> gathering;
    std::optional<TerrainId> required_terrain;
    ResourceSourcePolicy source_policy = ResourceSourcePolicy::None;
    bool requests_storage_inputs = false;
    bool internal_construction_site = false;
    std::array<std::uint8_t, 3> map_color{128, 128, 128};
    ResourceSourceMask source_mask = 0;
};

class BuildingCatalog {
public:
    BuildingCatalog();

    [[nodiscard]] static BuildingCatalog load_directory(const std::filesystem::path& directory);

    [[nodiscard]] const BuildingDefinition& definition(BuildingKind kind) const
    {
        const auto index = indices_by_kind_[static_cast<std::uint8_t>(kind)];
        if (index < 0) {
            throw std::out_of_range("building kind is not present in catalog");
        }
        return definitions_[static_cast<std::size_t>(index)];
    }
    [[nodiscard]] const BuildingDefinition& definition(std::string_view stable_id) const;
    [[nodiscard]] std::optional<BuildingKind> find_kind(std::string_view stable_id) const;
    [[nodiscard]] std::string_view stable_id(BuildingKind kind) const;
    [[nodiscard]] const std::vector<BuildingDefinition>& definitions() const;
    [[nodiscard]] std::uint64_t fingerprint() const;

private:
    std::vector<BuildingDefinition> definitions_;
    std::unordered_map<std::string, BuildingKind> kinds_by_stable_id_;
    std::array<int, std::numeric_limits<std::uint8_t>::max() + 1> indices_by_kind_;
    std::uint64_t fingerprint_ = 0;
};

using BuildingId = std::uint32_t;

struct BuildingInstance {
    BuildingId id = 0;
    bool active = true;
    BuildingKind kind = BuildingKind::House;
    std::optional<GridPosition> position;
    Inventory inventory;
    int residents = 0;
    int assigned_workers = 0;
    int assigned_builders = 0;
    Tick recipe_progress = 0;
    int hunger_days = 0;
    std::optional<BuildingKind> construction_target;
    Tick construction_labor_required = 0;
    Tick construction_labor_completed = 0;
    BlockingReason blocking_reason = BlockingReason::None;
    ResourceSourceMask source_mask = 0;
};

[[nodiscard]] const BuildingDefinition& building_definition(BuildingKind kind);
[[nodiscard]] std::string_view building_kind_name(BuildingKind kind);
[[nodiscard]] std::shared_ptr<const BuildingCatalog> default_building_catalog();
[[nodiscard]] std::string_view blocking_reason_text(BlockingReason reason);
[[nodiscard]] ResourceArray construction_materials_for_footprint(
    const BuildingDefinition& definition,
    const TileMap& map,
    GridPosition position);
[[nodiscard]] BuildingInstance make_building(BuildingId id, const BuildingDefinition& definition);
[[nodiscard]] BuildingInstance make_construction_site(
    BuildingId id,
    const BuildingDefinition& construction_site,
    const BuildingDefinition& target,
    const ResourceArray& construction_materials);

}
