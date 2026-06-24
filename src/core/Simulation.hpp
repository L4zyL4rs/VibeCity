#pragma once

#include "core/Building.hpp"

#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

namespace vibecity {

inline constexpr Tick ticks_per_hour = 60;
inline constexpr Tick ticks_per_day = 24 * ticks_per_hour;
inline constexpr Tick dawn_tick = 6 * ticks_per_hour;
inline constexpr Tick logistics_dispatch_interval = 10;
inline constexpr Tick prototype_transport_leg_minutes = 5;
inline constexpr Quantity prototype_hauler_capacity = 5;
inline constexpr int prototype_builders_per_site = 3;
inline constexpr int prototype_immigrants_per_day = 1;

struct ResourceStats {
    ResourceArray produced{};
    ResourceArray consumed{};
    ResourceArray transported{};
    int constructed_buildings = 0;
};

struct ConstructionSummary {
    int sites = 0;
    int waiting_materials = 0;
    int waiting_logistics = 0;
    int waiting_builders = 0;
    int active_builders = 0;
    std::optional<BuildingId> next_site;
    std::optional<BuildingKind> next_target;
    Tick next_labor_remaining = 0;
    BlockingReason next_blocker = BlockingReason::None;
};

struct LogisticsSummary {
    int active_jobs = 0;
    int going_to_pickup = 0;
    int carrying_goods = 0;
    ResourceArray reserved_incoming{};
    ResourceArray reserved_outgoing{};
    ResourceArray in_transit{};
};

using TransportJobId = std::uint32_t;

enum class TransportJobState : std::uint8_t {
    GoingToPickup,
    CarryingGoods,
    Complete,
    Failed
};

struct TransportJob {
    TransportJobId id = 0;
    ResourceId resource = ResourceId::Grain;
    Quantity quantity = 0;
    BuildingId source = 0;
    BuildingId destination = 0;
    TransportJobState state = TransportJobState::GoingToPickup;
    Tick ticks_remaining = 0;
    Tick leg_ticks_total = 0;
    Tick delivery_ticks = 0;
};

struct ResourceRequest {
    BuildingId destination = 0;
    ResourceId resource = ResourceId::Grain;
    Quantity quantity = 0;
    int priority = 0;
};

enum class PopulationGrowthBlocker : std::uint8_t {
    None,
    NoHousing,
    HungryHouse,
    NotEnoughBread
};

class Simulation {
public:
    BuildingId add_building(BuildingKind kind);
    BuildingId add_building_at(BuildingKind kind, GridPosition position);
    BuildingId place_construction(BuildingKind target_kind);
    BuildingId place_construction_at(BuildingKind target_kind, GridPosition position);

    [[nodiscard]] BuildingInstance& building(BuildingId id);
    [[nodiscard]] const BuildingInstance& building(BuildingId id) const;
    [[nodiscard]] const std::vector<BuildingInstance>& buildings() const;
    [[nodiscard]] const std::vector<TransportJob>& transport_jobs() const;
    [[nodiscard]] const TileMap& map() const;

    bool add_path(GridPosition position);
    void set_residents(BuildingId id, int residents);
    void mark_worker_assignment_dirty();
    void assign_workers();

    void tick();
    void run_for(Tick ticks);

    [[nodiscard]] Tick current_tick() const;
    [[nodiscard]] int current_day() const;
    [[nodiscard]] Tick minute_of_day() const;
    [[nodiscard]] int idle_workers() const;
    [[nodiscard]] int available_haulers() const;
    [[nodiscard]] int total_residents() const;
    [[nodiscard]] int total_housing_capacity() const;
    [[nodiscard]] int free_housing_capacity() const;
    [[nodiscard]] Quantity daily_bread_need() const;
    [[nodiscard]] Quantity stored_bread() const;
    [[nodiscard]] Quantity bread_days_remaining() const;
    [[nodiscard]] PopulationGrowthBlocker population_growth_blocker() const;
    [[nodiscard]] ConstructionSummary construction_summary() const;
    [[nodiscard]] LogisticsSummary logistics_summary() const;
    [[nodiscard]] ResourceArray total_inventory() const;
    [[nodiscard]] const ResourceStats& stats() const;

private:
    struct SourceSelection {
        BuildingInstance* building = nullptr;
        Tick distance = 0;
    };

    [[nodiscard]] BuildingInstance* find_building(BuildingId id);
    [[nodiscard]] const BuildingInstance* find_building(BuildingId id) const;

    [[nodiscard]] GridPosition auto_place_building(BuildingId id, Footprint footprint);
    [[nodiscard]] Footprint footprint_for(const BuildingInstance& building) const;
    [[nodiscard]] std::optional<Tick> transport_minutes_if_connected(const BuildingInstance& source,
        const BuildingInstance& destination) const;
    void run_production();
    void dispatch_logistics();
    void advance_transport_jobs();
    void run_construction();
    void consume_daily_bread();
    void grow_population();
    [[nodiscard]] std::vector<ResourceRequest> collect_resource_requests() const;
    [[nodiscard]] int viable_source_count(const ResourceRequest& request) const;
    [[nodiscard]] std::optional<SourceSelection> find_source_for_request(
        const ResourceRequest& request,
        const PathDistanceField* distance_field);
    [[nodiscard]] bool can_source_resource(const BuildingInstance& source, ResourceId resource) const;
    [[nodiscard]] Quantity projected_quantity(const BuildingInstance& building, ResourceId resource) const;
    [[nodiscard]] bool construction_materials_delivered(const BuildingInstance& site) const;
    [[nodiscard]] bool create_transport_job(
        BuildingInstance& source,
        BuildingInstance& destination,
        ResourceId resource,
        Quantity quantity,
        Tick delivery_ticks);
    void complete_construction(BuildingInstance& site);
    bool start_recipe(BuildingInstance& building, const Recipe& recipe);
    void finish_recipe(BuildingInstance& building, const Recipe& recipe);
    bool has_recipe_inputs(const BuildingInstance& building, const Recipe& recipe) const;
    bool has_recipe_output_capacity(const BuildingInstance& building, const Recipe& recipe) const;

    // Buildings are append-only so stable ID N remains at index N - 1.
    std::vector<BuildingInstance> buildings_;
    std::vector<TransportJob> transport_jobs_;
    TileMap map_{128, 128};
    PathDistanceField logistics_distance_field_;
    BuildingId next_building_id_ = 1;
    TransportJobId next_transport_job_id_ = 1;
    int next_auto_building_x_ = 1;
    Tick current_tick_ = 0;
    bool worker_assignment_dirty_ = true;
    int idle_workers_ = 0;
    ResourceStats stats_{};
};

[[nodiscard]] std::string_view transport_job_state_name(TransportJobState state);
[[nodiscard]] std::string_view population_growth_blocker_text(PopulationGrowthBlocker blocker);

}
