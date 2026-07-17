#include "core/Simulation.hpp"

#include <algorithm>
#include <stdexcept>
#include <string>
#include <tuple>

namespace vibecity {
namespace {

constexpr int house_food_priority = 100;
constexpr int construction_material_priority = 90;
constexpr int discovery_material_priority = 85;
constexpr int recipe_input_priority = 80;
constexpr int storehouse_pull_priority = 10;
constexpr Quantity house_bread_target = 10;
constexpr int storage_pull_divisor = 4;
constexpr int distance_field_candidate_threshold = 4;
constexpr std::size_t worker_connectivity_pair_threshold = 64;
constexpr Quantity pottery_experiment_firewood = 6;
constexpr Quantity pottery_experiment_clay = 2;
constexpr int pottery_experiment_radius = 6;
constexpr Tick pottery_experiment_labor_minutes = ticks_per_day;
constexpr int discovery_project_workers_per_project = 2;

bool has_any(const ResourceArray& amounts)
{
    return std::any_of(amounts.begin(), amounts.end(), [](Quantity quantity) {
        return quantity != 0;
    });
}

bool request_order(const ResourceRequest& left, const ResourceRequest& right)
{
    return std::tuple{-left.priority, left.destination, resource_index(left.resource)}
        < std::tuple{-right.priority, right.destination, resource_index(right.resource)};
}

bool shares_component(const std::vector<int>& left, const std::vector<int>& right)
{
    return std::any_of(left.begin(), left.end(), [&right](int component) {
        return std::binary_search(right.begin(), right.end(), component);
    });
}

BuildingInstance make_inactive_building(BuildingId id)
{
    auto building = BuildingInstance{};
    building.id = id;
    building.active = false;
    building.work_enabled = false;
    building.source_mask = 0;
    return building;
}

ResourceArray pottery_experiment_inputs()
{
    auto inputs = empty_resources();
    inputs[resource_index(ResourceId::Firewood)] = pottery_experiment_firewood;
    return inputs;
}

}

const DiscoveryProjectDefinition& discovery_project_definition(DiscoveryProjectId project)
{
    static const auto pottery_experiment = DiscoveryProjectDefinition{
        .project = DiscoveryProjectId::PotteryExperiment,
        .name = "Pottery Experiment",
        .grants_capability = CapabilityId::Pottery,
        .required_host = BuildingKind::House,
        .inputs = pottery_experiment_inputs(),
        .map_resource = MapResourceId::Clay,
        .map_resource_quantity = pottery_experiment_clay,
        .map_radius = pottery_experiment_radius,
        .labor_minutes = pottery_experiment_labor_minutes,
        .worker_slots = discovery_project_workers_per_project
    };

    switch (project) {
    case DiscoveryProjectId::PotteryExperiment:
        return pottery_experiment;
    case DiscoveryProjectId::Count:
        break;
    }
    throw std::invalid_argument("invalid discovery project");
}

std::string_view weather_name(WeatherId weather)
{
    switch (weather) {
    case WeatherId::Clear:
        return "clear";
    case WeatherId::Rain:
        return "rain";
    case WeatherId::Count:
        return "unknown";
    }
    return "unknown";
}

WeatherId weather_for_day(int day)
{
    if (day <= 0) {
        return WeatherId::Clear;
    }
    return day % 5 == 2 ? WeatherId::Rain : WeatherId::Clear;
}

std::string_view discovery_project_start_blocker_text(DiscoveryProjectStartBlocker blocker)
{
    switch (blocker) {
    case DiscoveryProjectStartBlocker::None:
        return "ready";
    case DiscoveryProjectStartBlocker::CapabilityAlreadyDiscovered:
        return "capability already discovered";
    case DiscoveryProjectStartBlocker::AlreadyActive:
        return "discovery project is already active";
    case DiscoveryProjectStartBlocker::InvalidHost:
        return "discovery project host is invalid";
    case DiscoveryProjectStartBlocker::WrongHost:
        return "discovery project requires a different host";
    case DiscoveryProjectStartBlocker::MissingPathAccess:
        return "discovery project host needs path access";
    case DiscoveryProjectStartBlocker::MissingInputs:
        return "discovery project is missing hosted inputs";
    case DiscoveryProjectStartBlocker::MissingMapResource:
        return "discovery project is missing nearby map resource";
    }
    return "unknown discovery project blocker";
}

Simulation::Simulation(std::shared_ptr<const BuildingCatalog> catalog)
    : Simulation(WorldGenerationSettings{}, std::move(catalog))
{
}

Simulation::Simulation(
    WorldGenerationSettings world_generation,
    std::shared_ptr<const BuildingCatalog> catalog)
    : catalog_(std::move(catalog))
{
    if (catalog_ == nullptr) {
        throw std::invalid_argument("simulation requires a building catalog");
    }
    map_.generate_world(world_generation);
}

BuildingId Simulation::add_building(BuildingKind kind)
{
    const auto& building = definition(kind);
    if (building.internal_construction_site) {
        throw std::invalid_argument("construction site cannot be placed as a completed building");
    }
    if (!building_unlocked(kind)) {
        throw std::invalid_argument("building is locked by missing capability");
    }
    const auto id = next_building_id_++;
    auto instance = make_building(id, building);
    instance.position = auto_place_building(id, building);
    buildings_.push_back(instance);
    worker_assignment_dirty_ = true;
    return id;
}

BuildingId Simulation::add_building_at(BuildingKind kind, GridPosition position)
{
    const auto& building = definition(kind);
    if (building.internal_construction_site) {
        throw std::invalid_argument("construction site cannot be placed as a completed building");
    }
    if (!building_unlocked(kind)) {
        throw std::invalid_argument("building is locked by missing capability");
    }
    if (!can_place_definition_at(building, position)) {
        throw std::invalid_argument("building cannot be placed at requested position");
    }

    const auto id = next_building_id_++;
    map_.place_building(id, position, building.footprint);
    auto instance = make_building(id, building);
    instance.position = position;
    buildings_.push_back(instance);
    worker_assignment_dirty_ = true;
    return id;
}

BuildingId Simulation::place_construction(BuildingKind target_kind)
{
    const auto& target = definition(target_kind);
    if (target.internal_construction_site) {
        throw std::invalid_argument("construction site cannot target another construction site");
    }
    if (!building_unlocked(target_kind)) {
        throw std::invalid_argument("construction target is locked by missing capability");
    }

    const auto id = next_building_id_++;
    const auto position = auto_place_building(id, target);
    auto site = make_construction_site(
        id,
        definition(BuildingKind::ConstructionSite),
        target,
        construction_materials_for_footprint(target, map_, position));
    site.position = position;
    buildings_.push_back(site);
    return id;
}

BuildingId Simulation::place_construction_at(BuildingKind target_kind, GridPosition position)
{
    const auto& target = definition(target_kind);
    if (target.internal_construction_site) {
        throw std::invalid_argument("construction site cannot target another construction site");
    }
    if (!building_unlocked(target_kind)) {
        throw std::invalid_argument("construction target is locked by missing capability");
    }

    if (!can_place_definition_at(target, position)) {
        throw std::invalid_argument("construction site cannot be placed at requested position");
    }

    const auto id = next_building_id_++;
    map_.place_building(id, position, target.footprint);
    auto site = make_construction_site(
        id,
        definition(BuildingKind::ConstructionSite),
        target,
        construction_materials_for_footprint(target, map_, position));
    site.position = position;
    buildings_.push_back(site);
    return id;
}

BuildingInstance& Simulation::building(BuildingId id)
{
    auto* result = find_building(id);
    if (result == nullptr) {
        throw std::out_of_range("building id does not exist");
    }
    return *result;
}

const BuildingInstance& Simulation::building(BuildingId id) const
{
    const auto* result = find_building(id);
    if (result == nullptr) {
        throw std::out_of_range("building id does not exist");
    }
    return *result;
}

const std::vector<BuildingInstance>& Simulation::buildings() const
{
    return buildings_;
}

const std::vector<TransportJob>& Simulation::transport_jobs() const
{
    return transport_jobs_;
}

const std::vector<DiscoveryProject>& Simulation::discovery_projects() const
{
    return discovery_projects_;
}

const TileMap& Simulation::map() const
{
    return map_;
}

const BuildingCatalog& Simulation::building_catalog() const
{
    return *catalog_;
}

std::shared_ptr<const BuildingCatalog> Simulation::building_catalog_ptr() const
{
    return catalog_;
}

bool Simulation::add_path(GridPosition position)
{
    const auto added = map_.add_path(position);
    if (added) {
        worker_assignment_dirty_ = true;
    }
    return added;
}

bool Simulation::remove_path(GridPosition position)
{
    const auto removed = map_.remove_path(position);
    if (removed) {
        worker_assignment_dirty_ = true;
    }
    return removed;
}

bool Simulation::demolish_building(BuildingId id)
{
    auto* instance = find_building(id);
    if (instance == nullptr || !instance->position.has_value()) {
        return false;
    }

    const auto position = *instance->position;
    const auto footprint = footprint_for(*instance);
    if (!map_.remove_building(id, position, footprint)) {
        return false;
    }

    cancel_transport_jobs_for_building(id);
    std::erase_if(discovery_projects_, [id](const DiscoveryProject& project) {
        return project.host == id;
    });
    *instance = make_inactive_building(id);
    worker_assignment_dirty_ = true;
    return true;
}

void Simulation::set_building_work_enabled(BuildingId id, bool enabled)
{
    auto& instance = building(id);
    if (instance.work_enabled == enabled) {
        return;
    }

    instance.work_enabled = enabled;
    if (!enabled) {
        instance.assigned_workers = 0;
        instance.assigned_builders = 0;
        if (definition(instance.kind).recipe.has_value()
            || instance.construction_target.has_value()) {
            instance.blocking_reason = BlockingReason::WorkDisabled;
        }
    } else if (instance.blocking_reason == BlockingReason::WorkDisabled) {
        instance.blocking_reason = BlockingReason::None;
    }
    worker_assignment_dirty_ = true;
}

void Simulation::grant_capability(CapabilityId capability)
{
    if (capability == CapabilityId::Count) {
        throw std::invalid_argument("invalid capability");
    }
    capabilities_ |= capability_bit(capability);
}

void Simulation::start_discovery_project(DiscoveryProjectId project, BuildingId host)
{
    const auto start_status = discovery_project_start_status(project, host);
    if (start_status.blocker != DiscoveryProjectStartBlocker::None
        && start_status.blocker != DiscoveryProjectStartBlocker::MissingInputs) {
        throw std::invalid_argument(std::string{
            discovery_project_start_blocker_text(start_status.blocker)});
    }

    discovery_projects_.push_back(DiscoveryProject{
        .project = project,
        .host = host,
        .materials_consumed = false,
        .labor_completed = 0,
        .assigned_workers = 0
    });
}

DiscoveryProjectStartStatus Simulation::discovery_project_start_status(
    DiscoveryProjectId project,
    BuildingId host) const
{
    const auto& project_definition = discovery_project_definition(project);
    auto status = DiscoveryProjectStartStatus{};

    if (has_capability(project_definition.grants_capability)) {
        status.blocker = DiscoveryProjectStartBlocker::CapabilityAlreadyDiscovered;
        return status;
    }
    if (std::any_of(
            discovery_projects_.begin(),
            discovery_projects_.end(),
            [project](const DiscoveryProject& active) {
                return active.project == project;
            })) {
        status.blocker = DiscoveryProjectStartBlocker::AlreadyActive;
        return status;
    }

    const auto* host_building = find_building(host);
    if (host_building == nullptr || !host_building->position.has_value()) {
        status.blocker = DiscoveryProjectStartBlocker::InvalidHost;
        return status;
    }
    if (host_building->kind != project_definition.required_host) {
        status.blocker = DiscoveryProjectStartBlocker::WrongHost;
        return status;
    }

    const auto& host_definition = definition(host_building->kind);
    if (!map_.has_path_access(*host_building->position, host_definition.footprint)) {
        status.blocker = DiscoveryProjectStartBlocker::MissingPathAccess;
        return status;
    }

    status.map_resource_available = map_.map_resource_quantity_within_radius(
        *host_building->position,
        host_definition.footprint,
        project_definition.map_resource,
        project_definition.map_radius);

    auto missing_inputs = false;
    for (std::size_t index = 0; index < resource_count; ++index) {
        const auto required = project_definition.inputs[index];
        if (required <= 0) {
            continue;
        }
        const auto resource = static_cast<ResourceId>(index);
        const auto available = host_building->inventory.available(resource);
        if (available < required) {
            status.missing_inputs[index] = required - available;
            missing_inputs = true;
        }
    }
    if (missing_inputs) {
        status.blocker = DiscoveryProjectStartBlocker::MissingInputs;
        return status;
    }

    if (status.map_resource_available < project_definition.map_resource_quantity) {
        status.blocker = DiscoveryProjectStartBlocker::MissingMapResource;
        return status;
    }

    return status;
}

bool Simulation::can_start_discovery_project(
    DiscoveryProjectId project,
    BuildingId host) const
{
    const auto blocker = discovery_project_start_status(project, host).blocker;
    return blocker == DiscoveryProjectStartBlocker::None
        || blocker == DiscoveryProjectStartBlocker::MissingInputs;
}

bool Simulation::has_capability(CapabilityId capability) const
{
    if (capability == CapabilityId::Count) {
        return false;
    }
    return (capabilities_ & capability_bit(capability)) != 0;
}

CapabilityMask Simulation::capability_mask() const
{
    return capabilities_;
}

std::optional<CapabilityId> Simulation::missing_capability(BuildingKind kind) const
{
    const auto& building = definition(kind);
    if (building.required_capability.has_value()
        && !has_capability(*building.required_capability)) {
        return building.required_capability;
    }
    return std::nullopt;
}

bool Simulation::building_unlocked(BuildingKind kind) const
{
    return !missing_capability(kind).has_value();
}

bool Simulation::can_place_building_at(BuildingKind kind, GridPosition position) const
{
    const auto& building = definition(kind);
    if (building.internal_construction_site) {
        return false;
    }
    if (!building_unlocked(kind)) {
        return false;
    }
    return can_place_definition_at(building, position);
}

bool Simulation::set_map_resource(
    GridPosition position,
    MapResourceId resource,
    Quantity quantity)
{
    return map_.set_map_resource(position, resource, quantity);
}

void Simulation::set_residents(BuildingId id, int residents)
{
    auto& instance = building(id);
    const auto& building = definition(instance.kind);
    instance.residents = std::clamp(residents, 0, building.resident_capacity);
    worker_assignment_dirty_ = true;
}

void Simulation::mark_worker_assignment_dirty()
{
    worker_assignment_dirty_ = true;
}

void Simulation::assign_workers()
{
    for (auto& instance : buildings_) {
        instance.assigned_workers = 0;
    }

    struct WorkerPool {
        const BuildingInstance* house = nullptr;
        int available = 0;
        std::vector<int> path_components;
    };

    auto worker_pools = std::vector<WorkerPool>{};
    auto workplace_count = std::size_t{0};
    for (const auto& instance : buildings_) {
        if (!instance.active) {
            continue;
        }
        const auto& building = definition(instance.kind);
        const auto available_workers = std::min(building.worker_supply, instance.residents);
        if (available_workers > 0) {
            worker_pools.push_back(WorkerPool{
                .house = &instance,
                .available = available_workers,
                .path_components = {}
            });
        }
        if (building.worker_slots > 0 && instance.work_enabled) {
            ++workplace_count;
        }
    }

    const auto use_path_connectivity =
        worker_pools.size() * workplace_count >= worker_connectivity_pair_threshold;
    auto path_connectivity = std::optional<PathConnectivityMap>{};
    if (use_path_connectivity) {
        path_connectivity = map_.path_connectivity();
        for (auto& pool : worker_pools) {
            if (pool.house != nullptr && pool.house->position.has_value()) {
                pool.path_components = path_connectivity->components_touching_building(
                    *pool.house->position,
                    footprint_for(*pool.house));
            }
        }
    }

    for (auto& instance : buildings_) {
        if (!instance.active) {
            continue;
        }
        const auto& building = definition(instance.kind);
        if (building.worker_slots <= 0 || !instance.work_enabled) {
            continue;
        }

        auto remaining_slots = building.worker_slots;
        const auto workplace_components = use_path_connectivity && instance.position.has_value()
            ? path_connectivity->components_touching_building(
                *instance.position,
                footprint_for(instance))
            : std::vector<int>{};
        for (auto& pool : worker_pools) {
            if (remaining_slots <= 0) {
                break;
            }

            if (pool.available <= 0 || pool.house == nullptr) {
                continue;
            }

            const auto reachable = use_path_connectivity
                ? (!pool.house->position.has_value()
                    || !instance.position.has_value()
                    || shares_component(pool.path_components, workplace_components))
                : transport_minutes_if_connected(*pool.house, instance).has_value();
            if (!reachable) {
                continue;
            }

            const auto assigned = std::min(remaining_slots, pool.available);
            instance.assigned_workers += assigned;
            pool.available -= assigned;
            remaining_slots -= assigned;
        }
    }

    idle_workers_ = 0;
    for (const auto& pool : worker_pools) {
        idle_workers_ += pool.available;
    }
    worker_assignment_dirty_ = false;
}

void Simulation::tick()
{
    if (worker_assignment_dirty_ || minute_of_day() == dawn_tick) {
        assign_workers();
    }

    if (current_tick_ % logistics_dispatch_interval == 0) {
        dispatch_logistics();
    }

    advance_transport_jobs();
    run_production();
    run_construction();
    run_discovery_projects();

    ++current_tick_;

    if (current_tick_ % ticks_per_day == 0) {
        consume_daily_bread();
        grow_population();
    }
}

void Simulation::run_for(Tick ticks)
{
    for (Tick tick_index = 0; tick_index < ticks; ++tick_index) {
        tick();
    }
}

Tick Simulation::current_tick() const
{
    return current_tick_;
}

int Simulation::current_day() const
{
    return static_cast<int>(current_tick_ / ticks_per_day);
}

Tick Simulation::minute_of_day() const
{
    return current_tick_ % ticks_per_day;
}

WeatherId Simulation::current_weather() const
{
    return weather_for_day(current_day());
}

int Simulation::idle_workers() const
{
    return idle_workers_;
}

int Simulation::available_haulers() const
{
    return std::max(0, idle_workers_ - static_cast<int>(transport_jobs_.size()));
}

int Simulation::total_residents() const
{
    auto residents = 0;
    for (const auto& instance : buildings_) {
        if (!instance.active) {
            continue;
        }
        residents += instance.residents;
    }
    return residents;
}

int Simulation::total_housing_capacity() const
{
    auto capacity = 0;
    for (const auto& instance : buildings_) {
        if (!instance.active) {
            continue;
        }
        capacity += definition(instance.kind).resident_capacity;
    }
    return capacity;
}

int Simulation::free_housing_capacity() const
{
    return std::max(0, total_housing_capacity() - total_residents());
}

Quantity Simulation::daily_bread_need() const
{
    auto need = Quantity{0};
    for (const auto& instance : buildings_) {
        if (!instance.active) {
            continue;
        }
        const auto& building = definition(instance.kind);
        if (building.consumes_bread) {
            need += static_cast<Quantity>(instance.residents);
        }
    }
    return need;
}

Quantity Simulation::bread_required_for_population_growth() const
{
    return daily_bread_need() + prototype_immigrants_per_day;
}

Quantity Simulation::stored_bread() const
{
    auto bread = Quantity{0};
    for (const auto& instance : buildings_) {
        if (!instance.active) {
            continue;
        }
        bread += instance.inventory.quantity(ResourceId::Bread);
    }
    return bread;
}

Quantity Simulation::bread_days_remaining() const
{
    const auto need = daily_bread_need();
    if (need <= 0) {
        return 0;
    }
    return stored_bread() / need;
}

PopulationGrowthBlocker Simulation::population_growth_blocker() const
{
    if (free_housing_capacity() <= 0) {
        return PopulationGrowthBlocker::NoHousing;
    }

    for (const auto& instance : buildings_) {
        if (!instance.active) {
            continue;
        }
        if (instance.residents > 0 && instance.blocking_reason == BlockingReason::MissingBread) {
            return PopulationGrowthBlocker::HungryHouse;
        }
    }

    if (stored_bread() < bread_required_for_population_growth()) {
        return PopulationGrowthBlocker::NotEnoughBread;
    }

    return PopulationGrowthBlocker::None;
}

ConstructionSummary Simulation::construction_summary() const
{
    auto summary = ConstructionSummary{};
    for (const auto& instance : buildings_) {
        if (!instance.active) {
            continue;
        }
        if (!definition(instance.kind).internal_construction_site) {
            continue;
        }

        ++summary.sites;
        summary.active_builders += instance.assigned_builders;
        if (!summary.next_site.has_value()) {
            summary.next_site = instance.id;
            summary.next_target = instance.construction_target;
            summary.next_labor_remaining = std::max<Tick>(
                0,
                instance.construction_labor_required - instance.construction_labor_completed);
            summary.next_blocker = instance.blocking_reason;
        }

        switch (instance.blocking_reason) {
        case BlockingReason::MissingConstructionMaterial:
        case BlockingReason::NoReachableSource:
            ++summary.waiting_materials;
            break;
        case BlockingReason::WaitingForHauler:
            ++summary.waiting_logistics;
            break;
        case BlockingReason::WaitingForBuilderLabor:
            ++summary.waiting_builders;
            break;
        case BlockingReason::None:
        case BlockingReason::NotEnoughWorkers:
        case BlockingReason::MissingInput:
        case BlockingReason::OutputStorageFull:
        case BlockingReason::MissingBread:
        case BlockingReason::NoNearbyMapResource:
        case BlockingReason::WorkDisabled:
            break;
        }
    }
    return summary;
}

LogisticsSummary Simulation::logistics_summary() const
{
    auto summary = LogisticsSummary{};
    summary.active_jobs = static_cast<int>(transport_jobs_.size());

    for (const auto& job : transport_jobs_) {
        switch (job.state) {
        case TransportJobState::GoingToPickup:
            ++summary.going_to_pickup;
            break;
        case TransportJobState::CarryingGoods:
            ++summary.carrying_goods;
            summary.in_transit[resource_index(job.resource)] += job.quantity;
            break;
        case TransportJobState::Complete:
        case TransportJobState::Failed:
            break;
        }
    }

    for (const auto& instance : buildings_) {
        if (!instance.active) {
            continue;
        }
        for (std::size_t index = 0; index < resource_count; ++index) {
            const auto resource = static_cast<ResourceId>(index);
            summary.reserved_incoming[index] += instance.inventory.reserved_incoming(resource);
            summary.reserved_outgoing[index] += instance.inventory.reserved_outgoing(resource);
        }
    }

    return summary;
}

DiscoveryProjectSummary Simulation::discovery_project_summary() const
{
    auto summary = DiscoveryProjectSummary{};
    summary.active_projects = static_cast<int>(discovery_projects_.size());
    for (const auto& project : discovery_projects_) {
        const auto& project_definition = discovery_project_definition(project.project);
        summary.active_workers += project.assigned_workers;
        if (!summary.next_project.has_value()) {
            summary.next_project = project.project;
            summary.next_host = project.host;
            summary.next_labor_remaining = std::max<Tick>(
                0,
                project_definition.labor_minutes - project.labor_completed);
        }
    }
    return summary;
}

ResourceArray Simulation::total_inventory() const
{
    auto totals = empty_resources();
    for (const auto& instance : buildings_) {
        if (!instance.active) {
            continue;
        }
        const auto& quantities = instance.inventory.quantities();
        for (std::size_t index = 0; index < resource_count; ++index) {
            totals[index] += quantities[index];
        }
    }
    return totals;
}

const ResourceStats& Simulation::stats() const
{
    return stats_;
}

BuildingInstance* Simulation::find_building(BuildingId id)
{
    if (id == 0) {
        return nullptr;
    }

    const auto index = static_cast<std::size_t>(id - 1);
    if (index >= buildings_.size() || buildings_[index].id != id || !buildings_[index].active) {
        return nullptr;
    }

    return &buildings_[index];
}

const BuildingInstance* Simulation::find_building(BuildingId id) const
{
    if (id == 0) {
        return nullptr;
    }

    const auto index = static_cast<std::size_t>(id - 1);
    if (index >= buildings_.size() || buildings_[index].id != id || !buildings_[index].active) {
        return nullptr;
    }

    return &buildings_[index];
}

bool Simulation::can_place_definition_at(
    const BuildingDefinition& definition,
    GridPosition position) const
{
    if (!map_.can_place_building(position, definition.footprint)) {
        return false;
    }
    if (definition.required_terrain.has_value()
        && !map_.footprint_matches_terrain(
            position,
            definition.footprint,
            *definition.required_terrain)) {
        return false;
    }
    return true;
}

GridPosition Simulation::auto_place_building(
    BuildingId id,
    const BuildingDefinition& definition)
{
    for (int x = 0; x < map_.width(); ++x) {
        map_.add_path(GridPosition{x, 0});
    }

    for (int y = 1; y < map_.height(); ++y) {
        const auto first_x = y == 1 ? next_auto_building_x_ : 1;
        for (int x = first_x; x < map_.width(); ++x) {
            const auto position = GridPosition{x, y};
            if (!can_place_definition_at(definition, position)) {
                continue;
            }
            if (!map_.place_building(id, position, definition.footprint)) {
                throw std::runtime_error("automatic building placement failed");
            }
            next_auto_building_x_ = position.x + definition.footprint.width + 1;
            return position;
        }
    }

    throw std::runtime_error("automatic building placement failed");
}

Footprint Simulation::footprint_for(const BuildingInstance& building) const
{
    if (definition(building.kind).internal_construction_site && building.construction_target.has_value()) {
        return definition(*building.construction_target).footprint;
    }

    return definition(building.kind).footprint;
}

std::optional<Tick> Simulation::transport_minutes_if_connected(const BuildingInstance& source,
    const BuildingInstance& destination) const
{
    if (!source.position.has_value() || !destination.position.has_value()) {
        return weather_adjusted_transport_minutes(prototype_transport_leg_minutes);
    }

    const auto distance = map_.path_distance_between_buildings(
        *source.position,
        footprint_for(source),
        *destination.position,
        footprint_for(destination));

    if (!distance.has_value()) {
        return std::nullopt;
    }

    return weather_adjusted_transport_minutes(std::max<Tick>(1, *distance));
}

Tick Simulation::weather_adjusted_transport_minutes(Tick clear_minutes) const
{
    if (current_weather() == WeatherId::Rain) {
        return clear_minutes * rain_transport_multiplier;
    }
    return clear_minutes;
}

void Simulation::run_production()
{
    for (auto& instance : buildings_) {
        if (!instance.active) {
            continue;
        }
        const auto& building = definition(instance.kind);
        if (!building.recipe.has_value()) {
            continue;
        }
        if (!instance.work_enabled) {
            instance.blocking_reason = BlockingReason::WorkDisabled;
            continue;
        }

        const auto& recipe = *building.recipe;
        if (building.worker_slots > 0 && instance.assigned_workers < building.worker_slots) {
            instance.blocking_reason = BlockingReason::NotEnoughWorkers;
            continue;
        }

        if (instance.recipe_progress == 0 && !start_recipe(instance, recipe)) {
            continue;
        }

        ++instance.recipe_progress;

        if (instance.recipe_progress >= recipe.cycle_minutes) {
            finish_recipe(instance, recipe);
            instance.recipe_progress = 0;
            instance.blocking_reason = BlockingReason::None;
        }
    }
}

void Simulation::dispatch_logistics()
{
    auto requests = collect_resource_requests();
    std::sort(requests.begin(), requests.end(), request_order);

    auto cached_destination = std::optional<BuildingId>{};

    for (const auto& request : requests) {
        if (available_haulers() <= 0) {
            if (auto* destination = find_building(request.destination)) {
                destination->blocking_reason = BlockingReason::WaitingForHauler;
            }
            return;
        }

        auto* destination = find_building(request.destination);
        if (destination == nullptr) {
            continue;
        }

        const auto source_count = viable_source_count(request);
        const auto* distance_field = static_cast<const PathDistanceField*>(nullptr);
        if (cached_destination == destination->id) {
            distance_field = &logistics_distance_field_;
        } else if (source_count >= distance_field_candidate_threshold && destination->position.has_value()) {
            map_.populate_path_distances_from_building(
                logistics_distance_field_,
                *destination->position,
                footprint_for(*destination));
            cached_destination = destination->id;
            distance_field = &logistics_distance_field_;
        }

        const auto selection = source_count > 0
            ? find_source_for_request(request, distance_field)
            : std::nullopt;
        if (!selection.has_value()) {
            if (request.priority == storehouse_pull_priority) {
                continue;
            }
            if (destination->blocking_reason == BlockingReason::MissingInput
                || destination->blocking_reason == BlockingReason::MissingBread
                || destination->blocking_reason == BlockingReason::None) {
                destination->blocking_reason = BlockingReason::NoReachableSource;
            }
            continue;
        }

        auto& source = *selection->building;
        const auto quantity = std::min({
            request.quantity,
            source.inventory.available(request.resource),
            destination->inventory.free_capacity(request.resource),
            prototype_hauler_capacity
        });

        if (quantity <= 0) {
            continue;
        }

        if (!create_transport_job(
                source,
                *destination,
                request.resource,
                quantity,
                selection->distance)) {
            destination->blocking_reason = BlockingReason::WaitingForHauler;
        }
    }
}

void Simulation::advance_transport_jobs()
{
    for (auto& job : transport_jobs_) {
        if (job.state == TransportJobState::Complete || job.state == TransportJobState::Failed) {
            continue;
        }

        if (job.ticks_remaining > 0) {
            --job.ticks_remaining;
        }

        if (job.ticks_remaining > 0) {
            continue;
        }

        if (job.state == TransportJobState::GoingToPickup) {
            auto* source = find_building(job.source);
            auto* destination = find_building(job.destination);
            if (source == nullptr || destination == nullptr
                || !source->inventory.consume_reserved_outgoing(job.resource, job.quantity)) {
                if (destination != nullptr) {
                    destination->inventory.release_incoming(job.resource, job.quantity);
                }
                job.state = TransportJobState::Failed;
                continue;
            }

            job.state = TransportJobState::CarryingGoods;
            job.ticks_remaining = job.delivery_ticks;
            job.leg_ticks_total = job.ticks_remaining;
            continue;
        }

        if (job.state == TransportJobState::CarryingGoods) {
            auto* destination = find_building(job.destination);
            if (destination == nullptr || !destination->inventory.commit_reserved_incoming(job.resource, job.quantity)) {
                job.state = TransportJobState::Failed;
                continue;
            }

            destination->blocking_reason = BlockingReason::None;
            stats_.transported[resource_index(job.resource)] += job.quantity;
            job.state = TransportJobState::Complete;
        }
    }

    std::erase_if(transport_jobs_, [](const TransportJob& job) {
        return job.state == TransportJobState::Complete || job.state == TransportJobState::Failed;
    });
}

void Simulation::run_construction()
{
    auto available_builders = std::max(0, idle_workers_ - static_cast<int>(transport_jobs_.size()));

    for (auto& site : buildings_) {
        if (!site.active) {
            continue;
        }
        site.assigned_builders = 0;
        if (!definition(site.kind).internal_construction_site) {
            continue;
        }
        if (!site.work_enabled) {
            site.blocking_reason = BlockingReason::WorkDisabled;
            continue;
        }

        if (!construction_materials_delivered(site)) {
            if (site.blocking_reason != BlockingReason::NoReachableSource
                && site.blocking_reason != BlockingReason::WaitingForHauler) {
                site.blocking_reason = BlockingReason::MissingConstructionMaterial;
            }
            continue;
        }

        if (site.construction_labor_completed >= site.construction_labor_required) {
            complete_construction(site);
            continue;
        }

        if (available_builders <= 0) {
            site.blocking_reason = BlockingReason::WaitingForBuilderLabor;
            continue;
        }

        const auto builders = std::min(prototype_builders_per_site, available_builders);
        site.assigned_builders = builders;
        available_builders -= builders;
        site.construction_labor_completed += builders;
        site.blocking_reason = BlockingReason::None;

        if (site.construction_labor_completed >= site.construction_labor_required) {
            complete_construction(site);
        }
    }
}

void Simulation::run_discovery_projects()
{
    auto available_workers = std::max(0, idle_workers_ - static_cast<int>(transport_jobs_.size()));
    for (const auto& building : buildings_) {
        available_workers -= building.assigned_builders;
    }
    available_workers = std::max(0, available_workers);

    for (auto& project : discovery_projects_) {
        project.assigned_workers = 0;
        auto* host = find_building(project.host);
        if (host == nullptr) {
            continue;
        }
        if (!host->work_enabled) {
            host->blocking_reason = BlockingReason::WorkDisabled;
            continue;
        }

        const auto& project_definition = discovery_project_definition(project.project);
        const auto& host_definition = definition(host->kind);
        if (!project.materials_consumed) {
            auto has_inputs = true;
            for (std::size_t index = 0; index < resource_count; ++index) {
                const auto quantity = project_definition.inputs[index];
                if (quantity <= 0) {
                    continue;
                }
                if (host->inventory.available(static_cast<ResourceId>(index)) < quantity) {
                    has_inputs = false;
                    break;
                }
            }
            if (!has_inputs) {
                continue;
            }

            if (!host->position.has_value()
                || map_.map_resource_quantity_within_radius(
                    *host->position,
                    host_definition.footprint,
                    project_definition.map_resource,
                    project_definition.map_radius)
                    < project_definition.map_resource_quantity) {
                continue;
            }

            for (std::size_t index = 0; index < resource_count; ++index) {
                const auto quantity = project_definition.inputs[index];
                if (quantity > 0) {
                    host->inventory.remove(static_cast<ResourceId>(index), quantity);
                    stats_.consumed[index] += quantity;
                }
            }
            if (!map_.harvest_map_resource_within_radius(
                    *host->position,
                    host_definition.footprint,
                    project_definition.map_resource,
                    project_definition.map_radius,
                    project_definition.map_resource_quantity)) {
                throw std::runtime_error("discovery project map resource consumption failed");
            }
            project.materials_consumed = true;
        }

        const auto remaining_labor = project_definition.labor_minutes - project.labor_completed;
        if (remaining_labor <= 0) {
            continue;
        }
        if (available_workers <= 0) {
            continue;
        }

        const auto workers = std::min({
            project_definition.worker_slots,
            available_workers,
            static_cast<int>(std::min<Tick>(remaining_labor, project_definition.worker_slots))
        });
        if (workers <= 0) {
            continue;
        }

        project.assigned_workers = workers;
        project.labor_completed += workers;
        available_workers -= workers;
    }

    for (const auto& project : discovery_projects_) {
        const auto& project_definition = discovery_project_definition(project.project);
        if (project.labor_completed >= project_definition.labor_minutes) {
            grant_capability(project_definition.grants_capability);
        }
    }
    std::erase_if(discovery_projects_, [this](const DiscoveryProject& project) {
        return has_capability(discovery_project_definition(project.project).grants_capability);
    });
}

void Simulation::consume_daily_bread()
{
    for (auto& instance : buildings_) {
        if (!instance.active) {
            continue;
        }
        const auto& building = definition(instance.kind);
        if (!building.consumes_bread || instance.residents <= 0) {
            continue;
        }

        const auto needed = static_cast<Quantity>(instance.residents);
        const auto eaten = std::min(instance.inventory.available(ResourceId::Bread), needed);
        if (eaten > 0) {
            instance.inventory.remove(ResourceId::Bread, eaten);
            stats_.consumed[resource_index(ResourceId::Bread)] += eaten;
        }

        if (eaten < needed) {
            ++instance.hunger_days;
            instance.blocking_reason = BlockingReason::MissingBread;
        } else if (instance.blocking_reason == BlockingReason::MissingBread) {
            instance.blocking_reason = BlockingReason::None;
        }
    }
}

void Simulation::grow_population()
{
    if (population_growth_blocker() != PopulationGrowthBlocker::None) {
        return;
    }

    auto immigrants_remaining = prototype_immigrants_per_day;
    for (auto& instance : buildings_) {
        if (!instance.active) {
            continue;
        }
        if (immigrants_remaining <= 0) {
            break;
        }

        const auto& building = definition(instance.kind);
        if (!building.consumes_bread || instance.residents >= building.resident_capacity) {
            continue;
        }

        const auto added = std::min(immigrants_remaining, building.resident_capacity - instance.residents);
        instance.residents += added;
        immigrants_remaining -= added;
        worker_assignment_dirty_ = true;
    }
}

std::vector<ResourceRequest> Simulation::collect_resource_requests() const
{
    auto requests = std::vector<ResourceRequest>{};

    for (const auto& instance : buildings_) {
        if (!instance.active) {
            continue;
        }
        const auto& building = definition(instance.kind);

        if (building.internal_construction_site && instance.construction_target.has_value()) {
            if (!instance.work_enabled) {
                continue;
            }
            for (std::size_t index = 0; index < resource_count; ++index) {
                const auto required =
                    instance.inventory.capacity(static_cast<ResourceId>(index));
                if (required <= 0) {
                    continue;
                }

                const auto resource = static_cast<ResourceId>(index);
                const auto projected = projected_quantity(instance, resource);
                if (projected < required) {
                    requests.push_back(ResourceRequest{
                        .destination = instance.id,
                        .resource = resource,
                        .quantity = required - projected,
                        .priority = construction_material_priority
                    });
                }
            }
            continue;
        }

        if (building.consumes_bread && instance.residents > 0) {
            const auto projected_bread = projected_quantity(instance, ResourceId::Bread);
            if (projected_bread < house_bread_target) {
                requests.push_back(ResourceRequest{
                    .destination = instance.id,
                    .resource = ResourceId::Bread,
                    .quantity = house_bread_target - projected_bread,
                    .priority = house_food_priority
                });
            }
        }

        if (building.recipe.has_value()) {
            if (!instance.work_enabled) {
                continue;
            }
            const auto& recipe = *building.recipe;
            for (std::size_t index = 0; index < resource_count; ++index) {
                const auto resource = static_cast<ResourceId>(index);
                if (recipe.inputs[index] <= 0) {
                    continue;
                }

                const auto target = instance.inventory.capacity(resource);
                const auto projected = projected_quantity(instance, resource);
                if (target > 0 && projected < target) {
                    requests.push_back(ResourceRequest{
                        .destination = instance.id,
                        .resource = resource,
                        .quantity = target - projected,
                        .priority = recipe_input_priority
                    });
                }
            }
        }

        if (building.requests_storage_inputs && instance.work_enabled) {
            for (std::size_t index = 0; index < resource_count; ++index) {
                const auto resource = static_cast<ResourceId>(index);
                const auto capacity = instance.inventory.capacity(resource);
                const auto target = (capacity + storage_pull_divisor - 1)
                    / storage_pull_divisor;
                const auto projected = projected_quantity(instance, resource);
                if (target > 0 && projected < target) {
                    requests.push_back(ResourceRequest{
                        .destination = instance.id,
                        .resource = resource,
                        .quantity = target - projected,
                        .priority = storehouse_pull_priority
                    });
                }
            }
        }
    }

    for (const auto& project : discovery_projects_) {
        if (project.materials_consumed) {
            continue;
        }
        const auto* host = find_building(project.host);
        if (host == nullptr || !host->work_enabled) {
            continue;
        }
        const auto& project_definition = discovery_project_definition(project.project);
        for (std::size_t index = 0; index < resource_count; ++index) {
            const auto required = project_definition.inputs[index];
            if (required <= 0) {
                continue;
            }
            const auto resource = static_cast<ResourceId>(index);
            const auto projected = projected_quantity(*host, resource);
            if (projected < required) {
                requests.push_back(ResourceRequest{
                    .destination = host->id,
                    .resource = resource,
                    .quantity = required - projected,
                    .priority = discovery_material_priority
                });
            }
        }
    }

    return requests;
}

int Simulation::viable_source_count(const ResourceRequest& request) const
{
    return static_cast<int>(std::count_if(
        buildings_.begin(),
        buildings_.end(),
        [this, &request](const BuildingInstance& candidate) {
            return candidate.active
                && candidate.id != request.destination
                && can_source_for_request(candidate, request);
        }));
}

std::optional<Simulation::SourceSelection> Simulation::find_source_for_request(
    const ResourceRequest& request,
    const PathDistanceField* distance_field)
{
    const auto* destination = find_building(request.destination);
    if (destination == nullptr) {
        return std::nullopt;
    }

    auto* best = static_cast<BuildingInstance*>(nullptr);
    auto best_distance = Tick{0};

    for (auto& candidate : buildings_) {
        if (!candidate.active
            || candidate.id == request.destination
            || !can_source_for_request(candidate, request)) {
            continue;
        }

        auto distance = std::optional<Tick>{};
        if (distance_field != nullptr && candidate.position.has_value()) {
            const auto path_distance = distance_field->distance_to_building(
                *candidate.position,
                footprint_for(candidate));
            distance = path_distance.has_value()
                ? std::optional<Tick>{weather_adjusted_transport_minutes(
                    std::max<Tick>(1, *path_distance))}
                : std::nullopt;
        } else {
            distance = transport_minutes_if_connected(candidate, *destination);
        }
        if (!distance.has_value()) {
            continue;
        }

        if (best == nullptr || *distance < best_distance || (*distance == best_distance && candidate.id < best->id)) {
            best = &candidate;
            best_distance = *distance;
        }
    }

    if (best == nullptr) {
        return std::nullopt;
    }

    return SourceSelection{
        .building = best,
        .distance = best_distance
    };
}

bool Simulation::can_source_resource(const BuildingInstance& source, ResourceId resource) const
{
    return (source.source_mask & resource_source_bit(resource)) != 0;
}

bool Simulation::can_source_for_request(
    const BuildingInstance& source,
    const ResourceRequest& request) const
{
    if (!can_source_resource(source, request.resource)
        || source.inventory.available(request.resource) <= 0) {
        return false;
    }

    if (request.priority != storehouse_pull_priority) {
        return true;
    }

    const auto* destination = find_building(request.destination);
    if (destination == nullptr) {
        return false;
    }

    const auto& source_definition = definition(source.kind);
    const auto& destination_definition = definition(destination->kind);
    if (!source_definition.requests_storage_inputs
        || !destination_definition.requests_storage_inputs) {
        return true;
    }

    return source.inventory.available(request.resource)
        > projected_quantity(*destination, request.resource) + prototype_hauler_capacity;
}

Quantity Simulation::projected_quantity(const BuildingInstance& building, ResourceId resource) const
{
    return building.inventory.quantity(resource) + building.inventory.reserved_incoming(resource);
}

bool Simulation::construction_materials_delivered(const BuildingInstance& site) const
{
    if (!definition(site.kind).internal_construction_site || !site.construction_target.has_value()) {
        return false;
    }

    for (std::size_t index = 0; index < resource_count; ++index) {
        const auto resource = static_cast<ResourceId>(index);
        const auto required = site.inventory.capacity(resource);
        if (required > 0 && site.inventory.quantity(resource) < required) {
            return false;
        }
    }

    return true;
}

bool Simulation::create_transport_job(
    BuildingInstance& source,
    BuildingInstance& destination,
    ResourceId resource,
    Quantity quantity,
    Tick delivery_ticks)
{
    if (!source.inventory.reserve_outgoing(resource, quantity)) {
        return false;
    }

    if (!destination.inventory.reserve_incoming(resource, quantity)) {
        source.inventory.release_outgoing(resource, quantity);
        return false;
    }

    transport_jobs_.push_back(TransportJob{
        .id = next_transport_job_id_++,
        .resource = resource,
        .quantity = quantity,
        .source = source.id,
        .destination = destination.id,
        .state = TransportJobState::GoingToPickup,
        .ticks_remaining = prototype_transport_leg_minutes,
        .leg_ticks_total = prototype_transport_leg_minutes,
        .delivery_ticks = delivery_ticks
    });
    return true;
}

void Simulation::cancel_transport_jobs_for_building(BuildingId id)
{
    for (const auto& job : transport_jobs_) {
        if (job.source != id && job.destination != id) {
            continue;
        }

        auto* source = find_building(job.source);
        auto* destination = find_building(job.destination);
        if (job.state == TransportJobState::GoingToPickup && source != nullptr) {
            source->inventory.release_outgoing(job.resource, job.quantity);
        }
        if (destination != nullptr) {
            destination->inventory.release_incoming(job.resource, job.quantity);
        }
    }

    std::erase_if(transport_jobs_, [id](const TransportJob& job) {
        return job.source == id || job.destination == id;
    });
}

void Simulation::complete_construction(BuildingInstance& site)
{
    if (!definition(site.kind).internal_construction_site || !site.construction_target.has_value()) {
        return;
    }

    const auto target = *site.construction_target;
    const auto id = site.id;
    const auto position = site.position;
    site = make_building(id, definition(target));
    site.position = position;
    ++stats_.constructed_buildings;
    worker_assignment_dirty_ = true;
}

bool Simulation::start_recipe(BuildingInstance& building, const Recipe& recipe)
{
    const auto& definition = this->definition(building.kind);
    if (!has_recipe_inputs(building, recipe)) {
        building.blocking_reason = BlockingReason::MissingInput;
        return false;
    }

    if (definition.gathering.has_value()) {
        const auto& gathering = *definition.gathering;
        if (!building.position.has_value()
            || map_.map_resource_quantity_within_radius(
                   *building.position,
                   definition.footprint,
                   gathering.resource,
                   gathering.radius)
                < gathering.units_per_cycle) {
            building.blocking_reason = BlockingReason::NoNearbyMapResource;
            return false;
        }
    }

    if (!has_recipe_output_capacity(building, recipe)) {
        building.blocking_reason = BlockingReason::OutputStorageFull;
        return false;
    }

    if (definition.gathering.has_value()) {
        const auto& gathering = *definition.gathering;
        if (!map_.harvest_map_resource_within_radius(
                *building.position,
                definition.footprint,
                gathering.resource,
                gathering.radius,
                gathering.units_per_cycle)) {
            building.blocking_reason = BlockingReason::NoNearbyMapResource;
            return false;
        }
    }

    for (std::size_t index = 0; index < resource_count; ++index) {
        const auto quantity = recipe.outputs[index];
        if (quantity > 0) {
            building.inventory.reserve_incoming(static_cast<ResourceId>(index), quantity);
        }
    }

    for (std::size_t index = 0; index < resource_count; ++index) {
        const auto quantity = recipe.inputs[index];
        if (quantity > 0) {
            building.inventory.remove(static_cast<ResourceId>(index), quantity);
        }
    }

    building.blocking_reason = BlockingReason::None;
    return true;
}

void Simulation::finish_recipe(BuildingInstance& building, const Recipe& recipe)
{
    for (std::size_t index = 0; index < resource_count; ++index) {
        const auto quantity = recipe.outputs[index];
        if (quantity > 0) {
            building.inventory.commit_reserved_incoming(static_cast<ResourceId>(index), quantity);
            stats_.produced[index] += quantity;
        }
    }
}

bool Simulation::has_recipe_inputs(const BuildingInstance& building, const Recipe& recipe) const
{
    if (!has_any(recipe.inputs)) {
        return true;
    }

    for (std::size_t index = 0; index < resource_count; ++index) {
        const auto quantity = recipe.inputs[index];
        if (quantity > 0 && building.inventory.available(static_cast<ResourceId>(index)) < quantity) {
            return false;
        }
    }
    return true;
}

bool Simulation::has_recipe_output_capacity(const BuildingInstance& building, const Recipe& recipe) const
{
    for (std::size_t index = 0; index < resource_count; ++index) {
        const auto quantity = recipe.outputs[index];
        if (quantity > 0 && building.inventory.free_capacity(static_cast<ResourceId>(index)) < quantity) {
            return false;
        }
    }
    return true;
}

std::string_view transport_job_state_name(TransportJobState state)
{
    switch (state) {
    case TransportJobState::GoingToPickup:
        return "going to pickup";
    case TransportJobState::CarryingGoods:
        return "carrying goods";
    case TransportJobState::Complete:
        return "complete";
    case TransportJobState::Failed:
        return "failed";
    }
    return "unknown";
}

std::string_view population_growth_blocker_text(PopulationGrowthBlocker blocker)
{
    switch (blocker) {
    case PopulationGrowthBlocker::None:
        return "ready";
    case PopulationGrowthBlocker::NoHousing:
        return "no housing";
    case PopulationGrowthBlocker::HungryHouse:
        return "hungry house";
    case PopulationGrowthBlocker::NotEnoughBread:
        return "low bread";
    }
    return "unknown";
}

}
