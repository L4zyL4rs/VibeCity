#include "core/Building.hpp"

#include <initializer_list>

namespace vibecity {
namespace {

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
        .worker_slots = 0,
        .resident_capacity = 5,
        .worker_supply = 3,
        .consumes_bread = true,
        .storage = amounts({{ResourceId::Bread, 10}}),
        .recipe = std::nullopt
    };

    definitions[static_cast<std::size_t>(BuildingKind::Farm)] = BuildingDefinition{
        .kind = BuildingKind::Farm,
        .name = "Farm",
        .worker_slots = 2,
        .storage = amounts({{ResourceId::Grain, 40}}),
        .recipe = Recipe{
            .outputs = amounts({{ResourceId::Grain, 20}}),
            .cycle_minutes = 720
        }
    };

    definitions[static_cast<std::size_t>(BuildingKind::Bakery)] = BuildingDefinition{
        .kind = BuildingKind::Bakery,
        .name = "Bakery",
        .worker_slots = 2,
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
        .worker_slots = 2,
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
        .storage = amounts({
            {ResourceId::Grain, 100},
            {ResourceId::Bread, 100},
            {ResourceId::Timber, 100},
            {ResourceId::Firewood, 100},
            {ResourceId::Tools, 20}
        }),
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
    }
    return "unknown";
}

BuildingInstance make_building(BuildingId id, BuildingKind kind)
{
    const auto& definition = building_definition(kind);
    return BuildingInstance{
        .id = id,
        .kind = kind,
        .inventory = Inventory(definition.storage)
    };
}

}
