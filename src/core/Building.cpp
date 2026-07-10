#include "core/Building.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>

namespace vibecity {
namespace {

void add_material(Quantity& target, Quantity amount)
{
    if (amount < 0 || target > std::numeric_limits<Quantity>::max() - amount) {
        throw std::overflow_error("construction material cost overflow");
    }
    target += amount;
}

}

BuildingCatalog::BuildingCatalog()
{
    indices_by_kind_.fill(-1);
}

std::optional<ResourceId> resource_id_from_string(std::string_view id)
{
    const auto found = std::find(resource_names.begin(), resource_names.end(), id);
    if (found == resource_names.end()) {
        return std::nullopt;
    }
    return static_cast<ResourceId>(std::distance(resource_names.begin(), found));
}

const BuildingDefinition& BuildingCatalog::definition(std::string_view stable_id) const
{
    const auto kind = find_kind(stable_id);
    if (!kind.has_value()) {
        throw std::out_of_range("building stable ID is not present in catalog");
    }
    return definition(*kind);
}

std::optional<BuildingKind> BuildingCatalog::find_kind(std::string_view stable_id) const
{
    const auto found = kinds_by_stable_id_.find(std::string{stable_id});
    if (found == kinds_by_stable_id_.end()) {
        return std::nullopt;
    }
    return found->second;
}

std::string_view BuildingCatalog::stable_id(BuildingKind kind) const
{
    return definition(kind).stable_id;
}

const std::vector<BuildingDefinition>& BuildingCatalog::definitions() const
{
    return definitions_;
}

std::uint64_t BuildingCatalog::fingerprint() const
{
    return fingerprint_;
}

std::shared_ptr<const BuildingCatalog> default_building_catalog()
{
    static const auto catalog = std::make_shared<const BuildingCatalog>(
        BuildingCatalog::load_directory(VIBECITY_BUILDING_DATA_DIR));
    return catalog;
}

const BuildingDefinition& building_definition(BuildingKind kind)
{
    return default_building_catalog()->definition(kind);
}

std::string_view building_kind_name(BuildingKind kind)
{
    return building_definition(kind).name;
}

std::string_view blocking_reason_text(BlockingReason reason)
{
    switch (reason) {
    case BlockingReason::None:
        return "none";
    case BlockingReason::NotEnoughWorkers:
        return "not enough workers";
    case BlockingReason::MissingInput:
        return "missing input";
    case BlockingReason::OutputStorageFull:
        return "output storage full";
    case BlockingReason::MissingBread:
        return "missing bread";
    case BlockingReason::WaitingForHauler:
        return "waiting for hauler";
    case BlockingReason::NoReachableSource:
        return "no reachable source";
    case BlockingReason::MissingConstructionMaterial:
        return "missing construction material";
    case BlockingReason::WaitingForBuilderLabor:
        return "waiting for builder labor";
    case BlockingReason::NoNearbyMapResource:
        return "no nearby map resource";
    }
    return "unknown";
}

ResourceArray construction_materials_for_footprint(
    const BuildingDefinition& definition,
    const TileMap& map,
    GridPosition position)
{
    auto materials = definition.construction_materials;
    if (definition.footprint.width <= 0 || definition.footprint.height <= 0) {
        return materials;
    }

    for (int y = position.y; y < position.y + definition.footprint.height; ++y) {
        for (int x = position.x; x < position.x + definition.footprint.width; ++x) {
            const auto tile = GridPosition{x, y};
            if (!map.in_bounds(tile)) {
                continue;
            }

            const auto terrain = map.terrain_at(tile);
            if (terrain == TerrainId::Count) {
                continue;
            }

            const auto& terrain_materials =
                definition.terrain_construction_materials[terrain_index(terrain)];
            for (std::size_t index = 0; index < resource_count; ++index) {
                add_material(materials[index], terrain_materials[index]);
            }
        }
    }

    return materials;
}

BuildingInstance make_building(BuildingId id, const BuildingDefinition& definition)
{
    return BuildingInstance{
        .id = id,
        .kind = definition.kind,
        .position = std::nullopt,
        .inventory = Inventory(definition.storage),
        .construction_target = std::nullopt,
        .source_mask = definition.source_mask
    };
}

BuildingInstance make_construction_site(
    BuildingId id,
    const BuildingDefinition& construction_site,
    const BuildingDefinition& target,
    const ResourceArray& construction_materials)
{
    if (!construction_site.internal_construction_site) {
        throw std::invalid_argument("catalog construction site definition is not internal");
    }
    return BuildingInstance{
        .id = id,
        .kind = construction_site.kind,
        .position = std::nullopt,
        .inventory = Inventory(construction_materials),
        .construction_target = target.kind,
        .construction_labor_required = target.construction_labor_minutes,
        .source_mask = construction_site.source_mask
    };
}

}
