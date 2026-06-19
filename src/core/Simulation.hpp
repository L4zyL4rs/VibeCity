#pragma once

#include "core/Building.hpp"

#include <vector>

namespace vibecity {

inline constexpr Tick ticks_per_hour = 60;
inline constexpr Tick ticks_per_day = 24 * ticks_per_hour;
inline constexpr Tick dawn_tick = 6 * ticks_per_hour;

struct ResourceStats {
    ResourceArray produced{};
    ResourceArray consumed{};
};

class Simulation {
public:
    BuildingId add_building(BuildingKind kind);

    [[nodiscard]] BuildingInstance& building(BuildingId id);
    [[nodiscard]] const BuildingInstance& building(BuildingId id) const;
    [[nodiscard]] const std::vector<BuildingInstance>& buildings() const;

    void set_residents(BuildingId id, int residents);
    void mark_worker_assignment_dirty();
    void assign_workers();

    void tick();
    void run_for(Tick ticks);

    [[nodiscard]] Tick current_tick() const;
    [[nodiscard]] int current_day() const;
    [[nodiscard]] Tick minute_of_day() const;
    [[nodiscard]] ResourceArray total_inventory() const;
    [[nodiscard]] const ResourceStats& stats() const;

private:
    [[nodiscard]] BuildingInstance* find_building(BuildingId id);
    [[nodiscard]] const BuildingInstance* find_building(BuildingId id) const;

    void run_production();
    void consume_daily_bread();
    bool start_recipe(BuildingInstance& building, const Recipe& recipe);
    void finish_recipe(BuildingInstance& building, const Recipe& recipe);
    bool has_recipe_inputs(const BuildingInstance& building, const Recipe& recipe) const;
    bool has_recipe_output_capacity(const BuildingInstance& building, const Recipe& recipe) const;

    std::vector<BuildingInstance> buildings_;
    BuildingId next_building_id_ = 1;
    Tick current_tick_ = 0;
    bool worker_assignment_dirty_ = true;
    ResourceStats stats_{};
};

}
