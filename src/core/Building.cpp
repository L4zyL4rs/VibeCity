#include "core/Building.hpp"

#include <initializer_list>

namespace vibecity {
namespace {

constexpr Tick labor_day = 12 * 60;

ResourceArray amounts(std::initializer_list<ResourceAmount> entries)
{
    auto result = empty_resources();
    for (const auto& entry : entries) {
        result[resource_index(entry.resource)] = entry.quantity;
    }
    return result;
}

std::array<BuildingDefinition, static_cast<std::size_t>(BuildingKind::Count)> make_definitions()
{
    auto definitions = std::array<BuildingDefinition, static_cast<std::size_t>(BuildingKind::Count)>{};

    definitions[static_cast<std::size_t>(BuildingKind::House)] = BuildingDefinition{
        .kind = BuildingKind::House,
        .name = "House",
        .footprint = Footprint{1, 1},
        .worker_slots = 0,
        .resident_capacity = 5,
        .worker_supply = 3,
        .consumes_bread = true,
        .construction_materials = amounts({{ResourceId::Timber, 8}}),
        .construction_labor_minutes = 2 * labor_day,
        .storage = amounts({{ResourceId::Bread, 10}}),
        .recipe = std::nullopt
    };

    definitions[static_cast<std::size_t>(BuildingKind::Farm)] = BuildingDefinition{
        .kind = BuildingKind::Farm,
        .name = "Farm",
        .footprint = Footprint{2, 2},
        .worker_slots = 2,
        .construction_materials = amounts({{ResourceId::Timber, 6}}),
        .construction_labor_minutes = 3 * labor_day,
        .storage = amounts({{ResourceId::Grain, 40}}),
        .recipe = Recipe{
            .outputs = amounts({{ResourceId::Grain, 20}}),
            .cycle_minutes = 720
        }
    };

    definitions[static_cast<std::size_t>(BuildingKind::Bakery)] = BuildingDefinition{
        .kind = BuildingKind::Bakery,
        .name = "Bakery",
        .footprint = Footprint{2, 2},
        .worker_slots = 2,
        .construction_materials = amounts({
            {ResourceId::Timber, 12},
            {ResourceId::Tools, 1}
        }),
        .construction_labor_minutes = 6 * labor_day,
        .storage = amounts({
            {ResourceId::Grain, 18},
            {ResourceId::Firewood, 6},
            {ResourceId::Bread, 24}
        }),
        .recipe = Recipe{
            .inputs = amounts({
                {ResourceId::Grain, 6},
                {ResourceId::Firewood, 1}
            }),
            .outputs = amounts({{ResourceId::Bread, 12}}),
            .cycle_minutes = 180
        }
    };

    definitions[static_cast<std::size_t>(BuildingKind::Woodcutter)] = BuildingDefinition{
        .kind = BuildingKind::Woodcutter,
        .name = "Woodcutter",
        .footprint = Footprint{2, 2},
        .worker_slots = 2,
        .construction_materials = amounts({{ResourceId::Timber, 8}}),
        .construction_labor_minutes = 4 * labor_day,
        .storage = amounts({
            {ResourceId::Timber, 20},
            {ResourceId::Firewood, 20}
        }),
        .recipe = Recipe{
            .outputs = amounts({
                {ResourceId::Timber, 4},
                {ResourceId::Firewood, 6}
            }),
            .cycle_minutes = 240
        }
    };

    definitions[static_cast<std::size_t>(BuildingKind::Storehouse)] = BuildingDefinition{
        .kind = BuildingKind::Storehouse,
        .name = "Storehouse",
        .footprint = Footprint{2, 2},
        .construction_materials = amounts({
            {ResourceId::Timber, 20},
            {ResourceId::Tools, 2}
        }),
        .construction_labor_minutes = 8 * labor_day,
        .storage = amounts({
            {ResourceId::Grain, 100},
            {ResourceId::Bread, 100},
            {ResourceId::Timber, 100},
            {ResourceId::Firewood, 100},
            {ResourceId::Tools, 20}
        }),
        .recipe = std::nullopt
    };

    definitions[static_cast<std::size_t>(BuildingKind::ConstructionSite)] = BuildingDefinition{
        .kind = BuildingKind::ConstructionSite,
        .name = "Construction Site",
        .footprint = Footprint{1, 1},
        .storage = empty_resources(),
        .recipe = std::nullopt
    };

    return definitions;
}

const auto definitions = make_definitions();

}

const BuildingDefinition& building_definition(BuildingKind kind)
{
    return definitions[static_cast<std::size_t>(kind)];
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
    }
    return "unknown";
}

BuildingInstance make_building(BuildingId id, BuildingKind kind)
{
    const auto& definition = building_definition(kind);
    return BuildingInstance{
        .id = id,
        .kind = kind,
        .position = std::nullopt,
        .inventory = Inventory(definition.storage),
        .construction_target = std::nullopt
    };
}

BuildingInstance make_construction_site(BuildingId id, BuildingKind target_kind)
{
    const auto& target = building_definition(target_kind);
    return BuildingInstance{
        .id = id,
        .kind = BuildingKind::ConstructionSite,
        .position = std::nullopt,
        .inventory = Inventory(target.construction_materials),
        .construction_target = target_kind,
        .construction_labor_required = target.construction_labor_minutes
    };
}

}
