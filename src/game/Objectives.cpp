#include "game/Objectives.hpp"

#include <algorithm>
#include <stdexcept>

namespace vibecity {
namespace {

std::size_t objective_index(VillageObjectiveId id)
{
    return static_cast<std::size_t>(id);
}

int building_count(const Simulation& simulation, BuildingKind kind)
{
    return static_cast<int>(std::count_if(simulation.buildings().begin(), simulation.buildings().end(), [kind](const BuildingInstance& building) {
        return building.active && building.kind == kind;
    }));
}

int total_hunger_days(const Simulation& simulation)
{
    auto hunger_days = 0;
    for (const auto& building : simulation.buildings()) {
        if (!building.active) {
            continue;
        }
        hunger_days += building.hunger_days;
    }
    return hunger_days;
}

VillageObjectiveStatus make_status(VillageObjectiveId id, int current, int target)
{
    return VillageObjectiveStatus{
        .id = id,
        .label = village_objective_label(id),
        .current = current,
        .target = target,
        .complete = current >= target
    };
}

}

VillageObjectiveTracker::VillageObjectiveTracker()
{
    statuses_[objective_index(VillageObjectiveId::HaveWoodcutter)] = make_status(VillageObjectiveId::HaveWoodcutter, 0, 1);
    statuses_[objective_index(VillageObjectiveId::HaveFarm)] = make_status(VillageObjectiveId::HaveFarm, 0, 1);
    statuses_[objective_index(VillageObjectiveId::HaveBakery)] = make_status(VillageObjectiveId::HaveBakery, 0, 1);
    statuses_[objective_index(VillageObjectiveId::Reach15Residents)] = make_status(VillageObjectiveId::Reach15Residents, 0, 15);
    statuses_[objective_index(VillageObjectiveId::Reach25Residents)] = make_status(VillageObjectiveId::Reach25Residents, 0, 25);
    statuses_[objective_index(VillageObjectiveId::Stable25Residents)] = make_status(VillageObjectiveId::Stable25Residents, 0, stable_days_target);
}

void VillageObjectiveTracker::update(const Simulation& simulation)
{
    const auto day = simulation.current_day();
    const auto hunger_days = total_hunger_days(simulation);

    if (!initialized_) {
        initialized_ = true;
        last_day_ = day;
        last_hunger_days_ = hunger_days;
    } else if (day > last_day_) {
        const auto elapsed_days = day - last_day_;
        if (simulation.total_residents() >= stable_population_target && hunger_days == last_hunger_days_) {
            stable_days_at_25_residents_ += elapsed_days;
        } else {
            stable_days_at_25_residents_ = 0;
        }
        last_day_ = day;
        last_hunger_days_ = hunger_days;
    } else {
        last_hunger_days_ = hunger_days;
    }

    statuses_[objective_index(VillageObjectiveId::HaveWoodcutter)] = make_status(
        VillageObjectiveId::HaveWoodcutter,
        building_count(simulation, BuildingKind::Woodcutter),
        1);
    statuses_[objective_index(VillageObjectiveId::HaveFarm)] = make_status(
        VillageObjectiveId::HaveFarm,
        building_count(simulation, BuildingKind::Farm),
        1);
    statuses_[objective_index(VillageObjectiveId::HaveBakery)] = make_status(
        VillageObjectiveId::HaveBakery,
        building_count(simulation, BuildingKind::Bakery),
        1);
    statuses_[objective_index(VillageObjectiveId::Reach15Residents)] = make_status(
        VillageObjectiveId::Reach15Residents,
        simulation.total_residents(),
        15);
    statuses_[objective_index(VillageObjectiveId::Reach25Residents)] = make_status(
        VillageObjectiveId::Reach25Residents,
        simulation.total_residents(),
        25);
    statuses_[objective_index(VillageObjectiveId::Stable25Residents)] = make_status(
        VillageObjectiveId::Stable25Residents,
        stable_days_at_25_residents_,
        stable_days_target);
}

const std::array<VillageObjectiveStatus, village_objective_count>& VillageObjectiveTracker::statuses() const
{
    return statuses_;
}

const VillageObjectiveStatus* VillageObjectiveTracker::active_status() const
{
    const auto found = std::find_if(statuses_.begin(), statuses_.end(), [](const VillageObjectiveStatus& status) {
        return !status.complete;
    });

    if (found == statuses_.end()) {
        return nullptr;
    }

    return &*found;
}

int VillageObjectiveTracker::completed_count() const
{
    return static_cast<int>(std::count_if(statuses_.begin(), statuses_.end(), [](const VillageObjectiveStatus& status) {
        return status.complete;
    }));
}

bool VillageObjectiveTracker::all_complete() const
{
    return active_status() == nullptr;
}

int VillageObjectiveTracker::stable_days_at_25_residents() const
{
    return stable_days_at_25_residents_;
}

VillageObjectiveTrackerState VillageObjectiveTracker::state() const
{
    return VillageObjectiveTrackerState{
        .initialized = initialized_,
        .last_day = last_day_,
        .last_hunger_days = last_hunger_days_,
        .stable_days_at_25_residents = stable_days_at_25_residents_
    };
}

void VillageObjectiveTracker::restore(
    const VillageObjectiveTrackerState& state,
    const Simulation& simulation)
{
    if (state.last_day < 0
        || state.last_hunger_days < 0
        || state.stable_days_at_25_residents < 0
        || (state.initialized && state.last_day != simulation.current_day())
        || (!state.initialized
            && (state.last_day != 0
                || state.last_hunger_days != 0
                || state.stable_days_at_25_residents != 0))) {
        throw std::invalid_argument("invalid saved objective state");
    }

    initialized_ = state.initialized;
    last_day_ = state.last_day;
    last_hunger_days_ = state.last_hunger_days;
    stable_days_at_25_residents_ = state.stable_days_at_25_residents;
    update(simulation);
}

std::string_view village_objective_label(VillageObjectiveId id)
{
    switch (id) {
    case VillageObjectiveId::HaveWoodcutter:
        return "WOODCUTTER";
    case VillageObjectiveId::HaveFarm:
        return "FARM";
    case VillageObjectiveId::HaveBakery:
        return "BAKERY";
    case VillageObjectiveId::Reach15Residents:
        return "15 RESIDENTS";
    case VillageObjectiveId::Reach25Residents:
        return "25 RESIDENTS";
    case VillageObjectiveId::Stable25Residents:
        return "FED DAYS";
    case VillageObjectiveId::Count:
        break;
    }
    return "UNKNOWN";
}

}
