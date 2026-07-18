#pragma once

#include "core/Simulation.hpp"
#include "game/Objectives.hpp"

#include <filesystem>
#include <optional>
#include <string>
#include <variant>

namespace vibecity {

struct PlacePathCommand {
    GridPosition position;
};

struct RemovePathCommand {
    GridPosition position;
};

struct PavePathCommand {
    GridPosition position;
};

struct PlaceBuildingCommand {
    BuildingKind kind = BuildingKind::House;
    GridPosition position;
};

struct DemolishBuildingCommand {
    BuildingId building = 0;
};

struct PlaceConstructionCommand {
    BuildingKind target_kind = BuildingKind::House;
    GridPosition position;
};

struct SetResidentsCommand {
    BuildingId building = 0;
    int residents = 0;
};

struct AddInventoryCommand {
    BuildingId building = 0;
    ResourceId resource = ResourceId::Grain;
    Quantity quantity = 0;
};

struct StartDiscoveryProjectCommand {
    DiscoveryProjectId project = DiscoveryProjectId::PotteryExperiment;
    BuildingId host = 0;
};

struct SetBuildingWorkEnabledCommand {
    BuildingId building = 0;
    bool enabled = true;
};

struct AdvanceTimeCommand {
    Tick ticks = 0;
};

using GameCommand = std::variant<
    PlacePathCommand,
    RemovePathCommand,
    PavePathCommand,
    PlaceBuildingCommand,
    DemolishBuildingCommand,
    PlaceConstructionCommand,
    SetResidentsCommand,
    AddInventoryCommand,
    StartDiscoveryProjectCommand,
    SetBuildingWorkEnabledCommand,
    AdvanceTimeCommand>;

struct CommandResult {
    bool success = false;
    std::string message;
    std::optional<BuildingId> building;
};

struct SessionIoResult {
    bool success = false;
    std::string message;
};

class GameSession {
public:
    explicit GameSession(std::shared_ptr<const BuildingCatalog> catalog = default_building_catalog());
    explicit GameSession(
        WorldGenerationSettings world_generation,
        std::shared_ptr<const BuildingCatalog> catalog = default_building_catalog());

    [[nodiscard]] Simulation& simulation();
    [[nodiscard]] const Simulation& simulation() const;
    [[nodiscard]] const VillageObjectiveTracker& objectives() const;

    [[nodiscard]] CommandResult execute(const GameCommand& command);
    [[nodiscard]] SessionIoResult save_to_file(const std::filesystem::path& path) const;
    [[nodiscard]] SessionIoResult load_from_file(const std::filesystem::path& path);

private:
    Simulation simulation_;
    VillageObjectiveTracker objectives_;
};

}
