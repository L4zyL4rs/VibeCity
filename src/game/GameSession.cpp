#include "game/GameSession.hpp"

#include <exception>
#include <utility>

namespace vibecity {
namespace {

template <typename... Visitors>
struct Overloaded : Visitors... {
    using Visitors::operator()...;
};

template <typename... Visitors>
Overloaded(Visitors...) -> Overloaded<Visitors...>;

CommandResult ok(std::string message, std::optional<BuildingId> building = std::nullopt)
{
    return CommandResult{
        .success = true,
        .message = std::move(message),
        .building = building
    };
}

CommandResult failed(std::string message)
{
    return CommandResult{
        .success = false,
        .message = std::move(message),
        .building = std::nullopt
    };
}

}

GameSession::GameSession(std::shared_ptr<const BuildingCatalog> catalog)
    : simulation_(std::move(catalog))
{
    objectives_.update(simulation_);
}

GameSession::GameSession(
    WorldGenerationSettings world_generation,
    std::shared_ptr<const BuildingCatalog> catalog)
    : simulation_(world_generation, std::move(catalog))
{
    objectives_.update(simulation_);
}

Simulation& GameSession::simulation()
{
    return simulation_;
}

const Simulation& GameSession::simulation() const
{
    return simulation_;
}

const VillageObjectiveTracker& GameSession::objectives() const
{
    return objectives_;
}

CommandResult GameSession::execute(const GameCommand& command)
{
    try {
        auto result = std::visit(Overloaded{
            [this](const PlacePathCommand& place_path) {
                if (!simulation_.add_path(place_path.position)) {
                    return failed("path could not be placed");
                }
                return ok("path placed");
            },
            [this](const RemovePathCommand& remove_path) {
                if (!simulation_.remove_path(remove_path.position)) {
                    return failed("path could not be removed");
                }
                return ok("path removed");
            },
            [this](const PlaceBuildingCommand& place_building) {
                const auto id = simulation_.add_building_at(place_building.kind, place_building.position);
                return ok("building placed", id);
            },
            [this](const DemolishBuildingCommand& demolish_building) {
                if (!simulation_.demolish_building(demolish_building.building)) {
                    return failed("building could not be demolished");
                }
                return ok("building demolished");
            },
            [this](const PlaceConstructionCommand& place_construction) {
                const auto id = simulation_.place_construction_at(place_construction.target_kind, place_construction.position);
                return ok("construction site placed", id);
            },
            [this](const SetResidentsCommand& set_residents) {
                simulation_.set_residents(set_residents.building, set_residents.residents);
                return ok("residents set", set_residents.building);
            },
            [this](const AddInventoryCommand& add_inventory) {
                auto& building = simulation_.building(add_inventory.building);
                if (!building.inventory.add(add_inventory.resource, add_inventory.quantity)) {
                    return failed("inventory could not be added");
                }
                return ok("inventory added", add_inventory.building);
            },
            [this](const AdvanceTimeCommand& advance_time) {
                if (advance_time.ticks < 0) {
                    return failed("cannot advance negative ticks");
                }
                for (Tick tick = 0; tick < advance_time.ticks; ++tick) {
                    const auto previous_day = simulation_.current_day();
                    simulation_.tick();
                    if (simulation_.current_day() != previous_day) {
                        objectives_.update(simulation_);
                    }
                }
                return ok("time advanced");
            }
        },
            command);

        if (result.success) {
            objectives_.update(simulation_);
        }
        return result;
    } catch (const std::exception& exception) {
        return failed(exception.what());
    }
}

}
