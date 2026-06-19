#pragma once

#include "core/Inventory.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <string_view>

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

enum class BlockingReason : std::uint8_t {
    None,
    NotEnoughWorkers,
    MissingInput,
    OutputStorageFull,
    MissingBread,
    WaitingForHauler,
    NoReachableSource,
    MissingConstructionMaterial,
    WaitingForBuilderLabor
};

struct Recipe {
    ResourceArray inputs{};
    ResourceArray outputs{};
    Tick cycle_minutes = 0;
};

struct BuildingDefinition {
    BuildingKind kind;
    std::string_view name;
    int worker_slots = 0;
    int resident_capacity = 0;
    int worker_supply = 0;
    bool consumes_bread = false;
    ResourceArray construction_materials{};
    Tick construction_labor_minutes = 0;
    ResourceArray storage{};
    std::optional<Recipe> recipe;
};

using BuildingId = std::uint32_t;

struct BuildingInstance {
    BuildingId id = 0;
    BuildingKind kind = BuildingKind::House;
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
};

[[nodiscard]] const BuildingDefinition& building_definition(BuildingKind kind);
[[nodiscard]] std::string_view building_kind_name(BuildingKind kind);
[[nodiscard]] std::string_view blocking_reason_text(BlockingReason reason);
[[nodiscard]] BuildingInstance make_building(BuildingId id, BuildingKind kind);
[[nodiscard]] BuildingInstance make_construction_site(BuildingId id, BuildingKind target_kind);

}
