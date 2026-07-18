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
inline constexpr Tick rain_transport_multiplier = 2;
inline constexpr Quantity prototype_hauler_capacity = 5;
inline constexpr Quantity paved_path_brick_cost = 1;
inline constexpr int prototype_builders_per_site = 3;
inline constexpr int prototype_immigrants_per_day = 1;

enum class WeatherId : std::uint8_t {
    Clear,
    Rain,
    Count
};

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

struct DiscoveryProjectDefinition {
    DiscoveryProjectId project = DiscoveryProjectId::PotteryExperiment;
    std::string_view name;
    CapabilityId grants_capability = CapabilityId::Pottery;
    std::optional<CapabilityId> required_capability;
    std::string_view required_host_stable_id = "storehouse";
    ResourceArray inputs{};
    MapResourceId map_resource = MapResourceId::Clay;
    Quantity map_resource_quantity = 0;
    int map_radius = 0;
    Tick labor_minutes = 0;
    int worker_slots = 0;
};

struct DiscoveryProject {
    DiscoveryProjectId project = DiscoveryProjectId::PotteryExperiment;
    BuildingId host = 0;
    bool materials_consumed = false;
    Tick labor_completed = 0;
    int assigned_workers = 0;
};

struct DiscoveryProjectSummary {
    int active_projects = 0;
    int active_workers = 0;
    std::optional<DiscoveryProjectId> next_project;
    std::optional<BuildingId> next_host;
    Tick next_labor_remaining = 0;
};

enum class DiscoveryProjectStartBlocker : std::uint8_t {
    None,
    CapabilityAlreadyDiscovered,
    AlreadyActive,
    MissingCapability,
    InvalidHost,
    WrongHost,
    MissingPathAccess,
    MissingInputs,
    MissingMapResource
};

struct DiscoveryProjectStartStatus {
    DiscoveryProjectStartBlocker blocker = DiscoveryProjectStartBlocker::None;
    ResourceArray missing_inputs{};
    Quantity map_resource_available = 0;
};

enum class PathPavingBlocker : std::uint8_t {
    None,
    MissingPath,
    AlreadyPaved,
    MissingCapability,
    MissingBricks
};

struct PathPavingStatus {
    PathPavingBlocker blocker = PathPavingBlocker::None;
    Quantity connected_bricks = 0;
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

struct SimulationState {
    int map_width = 128;
    int map_height = 128;
    std::vector<GridPosition> paths;
    std::vector<GridPosition> paved_paths;
    std::vector<TerrainTile> terrain;
    std::vector<MapResourceDeposit> map_resources;
    std::vector<BuildingInstance> buildings;
    std::vector<TransportJob> transport_jobs;
    std::vector<DiscoveryProject> discovery_projects;
    BuildingId next_building_id = 1;
    TransportJobId next_transport_job_id = 1;
    int next_auto_building_x = 1;
    Tick current_tick = 0;
    CapabilityMask capabilities = 0;
    bool worker_assignment_dirty = true;
    int idle_workers = 0;
    ResourceStats stats{};
};

class Simulation {
public:
    explicit Simulation(std::shared_ptr<const BuildingCatalog> catalog = default_building_catalog());
    explicit Simulation(
        WorldGenerationSettings world_generation,
        std::shared_ptr<const BuildingCatalog> catalog = default_building_catalog());

    BuildingId add_building(BuildingKind kind);
    BuildingId add_building_at(BuildingKind kind, GridPosition position);
    BuildingId place_construction(BuildingKind target_kind);
    BuildingId place_construction_at(BuildingKind target_kind, GridPosition position);

    [[nodiscard]] BuildingInstance& building(BuildingId id);
    [[nodiscard]] const BuildingInstance& building(BuildingId id) const;
    [[nodiscard]] const std::vector<BuildingInstance>& buildings() const;
    [[nodiscard]] const std::vector<TransportJob>& transport_jobs() const;
    [[nodiscard]] const std::vector<DiscoveryProject>& discovery_projects() const;
    [[nodiscard]] const TileMap& map() const;
    [[nodiscard]] const BuildingCatalog& building_catalog() const;
    [[nodiscard]] std::shared_ptr<const BuildingCatalog> building_catalog_ptr() const;
    [[nodiscard]] const BuildingDefinition& definition(BuildingKind kind) const
    {
        return catalog_->definition(kind);
    }

    bool add_path(GridPosition position);
    bool pave_path(GridPosition position);
    [[nodiscard]] PathPavingStatus path_paving_status(GridPosition position) const;
    [[nodiscard]] bool can_pave_path(GridPosition position) const;
    bool remove_path(GridPosition position);
    bool demolish_building(BuildingId id);
    void set_building_work_enabled(BuildingId id, bool enabled);
    void grant_capability(CapabilityId capability);
    void start_discovery_project(DiscoveryProjectId project, BuildingId host);
    [[nodiscard]] DiscoveryProjectStartStatus discovery_project_start_status(
        DiscoveryProjectId project,
        BuildingId host) const;
    [[nodiscard]] bool can_start_discovery_project(
        DiscoveryProjectId project,
        BuildingId host) const;
    [[nodiscard]] std::optional<DiscoveryProjectId> discovery_project_for_host(
        BuildingId host) const;
    [[nodiscard]] bool has_capability(CapabilityId capability) const;
    [[nodiscard]] CapabilityMask capability_mask() const;
    [[nodiscard]] std::optional<CapabilityId> missing_capability(BuildingKind kind) const;
    [[nodiscard]] bool building_unlocked(BuildingKind kind) const;
    [[nodiscard]] bool can_place_building_at(BuildingKind kind, GridPosition position) const;
    bool set_map_resource(GridPosition position, MapResourceId resource, Quantity quantity);
    void set_residents(BuildingId id, int residents);
    void mark_worker_assignment_dirty();
    void assign_workers();

    void tick();
    void run_for(Tick ticks);

    [[nodiscard]] Tick current_tick() const;
    [[nodiscard]] int current_day() const;
    [[nodiscard]] Tick minute_of_day() const;
    [[nodiscard]] WeatherId current_weather() const;
    [[nodiscard]] int idle_workers() const;
    [[nodiscard]] int available_haulers() const;
    [[nodiscard]] int total_residents() const;
    [[nodiscard]] int total_housing_capacity() const;
    [[nodiscard]] int free_housing_capacity() const;
    [[nodiscard]] Quantity daily_bread_need() const;
    [[nodiscard]] Quantity bread_required_for_population_growth() const;
    [[nodiscard]] Quantity stored_bread() const;
    [[nodiscard]] Quantity bread_days_remaining() const;
    [[nodiscard]] PopulationGrowthBlocker population_growth_blocker() const;
    [[nodiscard]] ConstructionSummary construction_summary() const;
    [[nodiscard]] LogisticsSummary logistics_summary() const;
    [[nodiscard]] DiscoveryProjectSummary discovery_project_summary() const;
    [[nodiscard]] ResourceArray total_inventory() const;
    [[nodiscard]] const ResourceStats& stats() const;
    [[nodiscard]] SimulationState state() const;
    [[nodiscard]] static Simulation from_state(
        SimulationState state,
        std::shared_ptr<const BuildingCatalog> catalog = default_building_catalog());

private:
    struct SourceSelection {
        BuildingInstance* building = nullptr;
        Tick distance = 0;
    };

    [[nodiscard]] BuildingInstance* find_building(BuildingId id);
    [[nodiscard]] const BuildingInstance* find_building(BuildingId id) const;

    [[nodiscard]] bool can_place_definition_at(
        const BuildingDefinition& definition,
        GridPosition position) const;
    [[nodiscard]] GridPosition auto_place_building(
        BuildingId id,
        const BuildingDefinition& definition);
    [[nodiscard]] Footprint footprint_for(const BuildingInstance& building) const;
    [[nodiscard]] std::optional<Tick> transport_minutes_if_connected(const BuildingInstance& source,
        const BuildingInstance& destination) const;
    [[nodiscard]] std::optional<Tick> transport_minutes_for_route(
        const std::vector<GridPosition>& route) const;
    [[nodiscard]] Tick weather_adjusted_transport_minutes(Tick clear_minutes) const;
    void run_production();
    void dispatch_logistics();
    void advance_transport_jobs();
    void run_construction();
    void run_discovery_projects();
    void consume_daily_bread();
    void grow_population();
    [[nodiscard]] std::vector<ResourceRequest> collect_resource_requests() const;
    [[nodiscard]] int viable_source_count(const ResourceRequest& request) const;
    [[nodiscard]] std::optional<SourceSelection> find_source_for_request(
        const ResourceRequest& request,
        const PathDistanceField* distance_field);
    [[nodiscard]] bool can_source_resource(const BuildingInstance& source, ResourceId resource) const;
    [[nodiscard]] bool can_source_for_request(
        const BuildingInstance& source,
        const ResourceRequest& request) const;
    [[nodiscard]] Quantity connected_resource_available_for_path(
        GridPosition position,
        ResourceId resource) const;
    [[nodiscard]] BuildingInstance* find_source_for_path_resource(
        GridPosition position,
        ResourceId resource,
        Quantity quantity);
    [[nodiscard]] Quantity projected_quantity(const BuildingInstance& building, ResourceId resource) const;
    [[nodiscard]] bool construction_materials_delivered(const BuildingInstance& site) const;
    [[nodiscard]] bool create_transport_job(
        BuildingInstance& source,
        BuildingInstance& destination,
        ResourceId resource,
        Quantity quantity);
    void complete_construction(BuildingInstance& site);
    bool start_recipe(BuildingInstance& building, const Recipe& recipe);
    void finish_recipe(BuildingInstance& building, const Recipe& recipe);
    bool has_recipe_inputs(const BuildingInstance& building, const Recipe& recipe) const;
    bool has_recipe_output_capacity(const BuildingInstance& building, const Recipe& recipe) const;
    void cancel_transport_jobs_for_building(BuildingId id);

    // Buildings are append-only so stable ID N remains at index N - 1.
    std::vector<BuildingInstance> buildings_;
    std::vector<TransportJob> transport_jobs_;
    std::vector<DiscoveryProject> discovery_projects_;
    TileMap map_{128, 128};
    PathDistanceField logistics_distance_field_;
    std::shared_ptr<const BuildingCatalog> catalog_;
    BuildingId next_building_id_ = 1;
    TransportJobId next_transport_job_id_ = 1;
    int next_auto_building_x_ = 1;
    Tick current_tick_ = 0;
    CapabilityMask capabilities_ = 0;
    bool worker_assignment_dirty_ = true;
    int idle_workers_ = 0;
    ResourceStats stats_{};
};

[[nodiscard]] std::string_view transport_job_state_name(TransportJobState state);
[[nodiscard]] std::string_view weather_name(WeatherId weather);
[[nodiscard]] WeatherId weather_for_day(int day);
[[nodiscard]] std::string_view population_growth_blocker_text(PopulationGrowthBlocker blocker);
[[nodiscard]] const DiscoveryProjectDefinition& discovery_project_definition(DiscoveryProjectId project);
[[nodiscard]] std::string_view discovery_project_start_blocker_text(
    DiscoveryProjectStartBlocker blocker);
[[nodiscard]] std::string_view path_paving_blocker_text(PathPavingBlocker blocker);

}
