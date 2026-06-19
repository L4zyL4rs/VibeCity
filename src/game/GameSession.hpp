#pragma once

#include "core/Simulation.hpp"

#include <optional>
#include <string>
#include <variant>

namespace vibecity {

struct PlacePathCommand {
    GridPosition position;
};

struct PlaceBuildingCommand {
    BuildingKind kind = BuildingKind::House;
    GridPosition position;
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

struct AdvanceTimeCommand {
    Tick ticks = 0;
};

using GameCommand = std::variant<
    PlacePathCommand,
    PlaceBuildingCommand,
    PlaceConstructionCommand,
    SetResidentsCommand,
    AddInventoryCommand,
    AdvanceTimeCommand>;

struct CommandResult {
    bool success = false;
    std::string message;
    std::optional<BuildingId> building;
};

class GameSession {
public:
    [[nodiscard]] Simulation& simulation();
    [[nodiscard]] const Simulation& simulation() const;

    [[nodiscard]] CommandResult execute(const GameCommand& command);

private:
    Simulation simulation_;
};

}
