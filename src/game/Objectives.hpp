#pragma once

#include "core/Simulation.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace vibecity {

enum class VillageObjectiveId : std::uint8_t {
    HaveWoodcutter,
    HaveFarm,
    HaveBakery,
    HaveQuarry,
    HaveTwoStorehouses,
    Reach15Residents,
    Reach25Residents,
    Stable25Residents,
    Count
};

inline constexpr std::size_t village_objective_count = static_cast<std::size_t>(VillageObjectiveId::Count);
inline constexpr int stable_population_target = 25;
inline constexpr int stable_days_target = 5;

struct VillageObjectiveStatus {
    VillageObjectiveId id = VillageObjectiveId::HaveWoodcutter;
    std::string_view label;
    int current = 0;
    int target = 1;
    bool complete = false;
};

struct VillageObjectiveTrackerState {
    bool initialized = false;
    int last_day = 0;
    int last_hunger_days = 0;
    int stable_days_at_25_residents = 0;
};

class VillageObjectiveTracker {
public:
    VillageObjectiveTracker();

    void update(const Simulation& simulation);

    [[nodiscard]] const std::array<VillageObjectiveStatus, village_objective_count>& statuses() const;
    [[nodiscard]] const VillageObjectiveStatus* active_status() const;
    [[nodiscard]] int completed_count() const;
    [[nodiscard]] bool all_complete() const;
    [[nodiscard]] int stable_days_at_25_residents() const;
    [[nodiscard]] VillageObjectiveTrackerState state() const;
    void restore(const VillageObjectiveTrackerState& state, const Simulation& simulation);

private:
    std::array<VillageObjectiveStatus, village_objective_count> statuses_{};
    bool initialized_ = false;
    int last_day_ = 0;
    int last_hunger_days_ = 0;
    int stable_days_at_25_residents_ = 0;
};

[[nodiscard]] std::string_view village_objective_label(VillageObjectiveId id);

}
