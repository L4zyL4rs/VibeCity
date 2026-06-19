#include "core/Simulation.hpp"

#include <algorithm>
#include <stdexcept>
#include <tuple>

namespace vibecity {
namespace {

constexpr int house_food_priority = 100;
constexpr int construction_material_priority = 90;
constexpr int recipe_input_priority = 80;
constexpr int storehouse_pull_priority = 10;
constexpr Quantity house_bread_target = 10;

bool has_any(const ResourceArray& amounts)
{
    return std::any_of(amounts.begin(), amounts.end(), [](Quantity quantity) {
        return quantity != 0;
    });
}

bool recipe_outputs_resource(const BuildingDefinition& definition, ResourceId resource)
{
    return definition.recipe.has_value() && definition.recipe->outputs[resource_index(resource)] > 0;
}

bool request_order(const ResourceRequest& left, const ResourceRequest& right)
{
    return std::tuple{-left.priority, left.destination, resource_index(left.resource)}
        < std::tuple{-right.priority, right.destination, resource_index(right.resource)};
}

}

BuildingId Simulation::add_building(BuildingKind kind)
{
    const auto id = next_building_id_++;
    buildings_.push_back(make_building(id, kind));
    worker_assignment_dirty_ = true;
    return id;
}

BuildingId Simulation::place_construction(BuildingKind target_kind)
{
    if (target_kind == BuildingKind::ConstructionSite) {
        throw std::invalid_argument("construction site cannot target another construction site");
    }

    const auto id = next_building_id_++;
    buildings_.push_back(make_construction_site(id, target_kind));
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

void Simulation::set_residents(BuildingId id, int residents)
{
    auto& instance = building(id);
    const auto& definition = building_definition(instance.kind);
    instance.residents = std::clamp(residents, 0, definition.resident_capacity);
    worker_assignment_dirty_ = true;
}

void Simulation::mark_worker_assignment_dirty()
{
    worker_assignment_dirty_ = true;
}

void Simulation::assign_workers()
{
    int available_workers = 0;

    for (auto& instance : buildings_) {
        instance.assigned_workers = 0;
    }

    for (const auto& instance : buildings_) {
        const auto& definition = building_definition(instance.kind);
        if (definition.worker_supply > 0) {
            available_workers += std::min(definition.worker_supply, instance.residents);
        }
    }

    for (auto& instance : buildings_) {
        const auto& definition = building_definition(instance.kind);
        if (definition.worker_slots <= 0) {
            continue;
        }

        const auto assigned = std::min(definition.worker_slots, available_workers);
        instance.assigned_workers = assigned;
        available_workers -= assigned;
    }

    idle_workers_ = available_workers;
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

    ++current_tick_;

    if (current_tick_ % ticks_per_day == 0) {
        consume_daily_bread();
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

int Simulation::idle_workers() const
{
    return idle_workers_;
}

int Simulation::available_haulers() const
{
    return std::max(0, idle_workers_ - static_cast<int>(transport_jobs_.size()));
}

ResourceArray Simulation::total_inventory() const
{
    auto totals = empty_resources();
    for (const auto& instance : buildings_) {
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
    const auto found = std::find_if(buildings_.begin(), buildings_.end(), [id](const BuildingInstance& instance) {
        return instance.id == id;
    });
    return found == buildings_.end() ? nullptr : &*found;
}

const BuildingInstance* Simulation::find_building(BuildingId id) const
{
    const auto found = std::find_if(buildings_.begin(), buildings_.end(), [id](const BuildingInstance& instance) {
        return instance.id == id;
    });
    return found == buildings_.end() ? nullptr : &*found;
}

void Simulation::run_production()
{
    for (auto& instance : buildings_) {
        const auto& definition = building_definition(instance.kind);
        if (!definition.recipe.has_value()) {
            continue;
        }

        const auto& recipe = *definition.recipe;
        if (definition.worker_slots > 0 && instance.assigned_workers < definition.worker_slots) {
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

    for (const auto& request : requests) {
        if (available_haulers() <= 0) {
            if (auto* destination = find_building(request.destination)) {
                destination->blocking_reason = BlockingReason::WaitingForHauler;
            }
            return;
        }

        auto* destination = find_building(request.destination);
        auto* source = find_source_for_request(request);
        if (destination == nullptr) {
            continue;
        }

        if (source == nullptr) {
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

        const auto quantity = std::min({
            request.quantity,
            source->inventory.available(request.resource),
            destination->inventory.free_capacity(request.resource),
            prototype_hauler_capacity
        });

        if (quantity <= 0) {
            continue;
        }

        if (!create_transport_job(*source, *destination, request.resource, quantity)) {
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
            job.ticks_remaining = prototype_transport_leg_minutes;
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
        site.assigned_builders = 0;
        if (site.kind != BuildingKind::ConstructionSite) {
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

void Simulation::consume_daily_bread()
{
    for (auto& instance : buildings_) {
        const auto& definition = building_definition(instance.kind);
        if (!definition.consumes_bread || instance.residents <= 0) {
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

std::vector<ResourceRequest> Simulation::collect_resource_requests() const
{
    auto requests = std::vector<ResourceRequest>{};

    for (const auto& instance : buildings_) {
        const auto& definition = building_definition(instance.kind);

        if (instance.kind == BuildingKind::ConstructionSite && instance.construction_target.has_value()) {
            const auto& target = building_definition(*instance.construction_target);
            for (std::size_t index = 0; index < resource_count; ++index) {
                const auto required = target.construction_materials[index];
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

        if (definition.consumes_bread && instance.residents > 0) {
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

        if (definition.recipe.has_value()) {
            const auto& recipe = *definition.recipe;
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

        if (instance.kind == BuildingKind::Storehouse) {
            for (std::size_t index = 0; index < resource_count; ++index) {
                const auto resource = static_cast<ResourceId>(index);
                const auto target = instance.inventory.capacity(resource);
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

    return requests;
}

BuildingInstance* Simulation::find_source_for_request(const ResourceRequest& request)
{
    auto* best = static_cast<BuildingInstance*>(nullptr);

    for (auto& candidate : buildings_) {
        if (candidate.id == request.destination || !can_source_resource(candidate, request.resource)) {
            continue;
        }

        if (candidate.inventory.available(request.resource) <= 0) {
            continue;
        }

        if (best == nullptr || candidate.id < best->id) {
            best = &candidate;
        }
    }

    return best;
}

const BuildingInstance* Simulation::find_source_for_request(const ResourceRequest& request) const
{
    auto* best = static_cast<const BuildingInstance*>(nullptr);

    for (const auto& candidate : buildings_) {
        if (candidate.id == request.destination || !can_source_resource(candidate, request.resource)) {
            continue;
        }

        if (candidate.inventory.available(request.resource) <= 0) {
            continue;
        }

        if (best == nullptr || candidate.id < best->id) {
            best = &candidate;
        }
    }

    return best;
}

bool Simulation::can_source_resource(const BuildingInstance& source, ResourceId resource) const
{
    if (source.kind == BuildingKind::House) {
        return false;
    }

    if (source.kind == BuildingKind::ConstructionSite) {
        return false;
    }

    if (source.kind == BuildingKind::Storehouse) {
        return true;
    }

    return recipe_outputs_resource(building_definition(source.kind), resource);
}

Quantity Simulation::projected_quantity(const BuildingInstance& building, ResourceId resource) const
{
    return building.inventory.quantity(resource) + building.inventory.reserved_incoming(resource);
}

bool Simulation::construction_materials_delivered(const BuildingInstance& site) const
{
    if (site.kind != BuildingKind::ConstructionSite || !site.construction_target.has_value()) {
        return false;
    }

    const auto& target = building_definition(*site.construction_target);
    for (std::size_t index = 0; index < resource_count; ++index) {
        const auto required = target.construction_materials[index];
        if (required > 0 && site.inventory.quantity(static_cast<ResourceId>(index)) < required) {
            return false;
        }
    }

    return true;
}

bool Simulation::create_transport_job(BuildingInstance& source, BuildingInstance& destination, ResourceId resource, Quantity quantity)
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
        .ticks_remaining = prototype_transport_leg_minutes
    });
    return true;
}

void Simulation::complete_construction(BuildingInstance& site)
{
    if (site.kind != BuildingKind::ConstructionSite || !site.construction_target.has_value()) {
        return;
    }

    const auto target = *site.construction_target;
    const auto id = site.id;
    site = make_building(id, target);
    ++stats_.constructed_buildings;
    worker_assignment_dirty_ = true;
}

bool Simulation::start_recipe(BuildingInstance& building, const Recipe& recipe)
{
    if (!has_recipe_inputs(building, recipe)) {
        building.blocking_reason = BlockingReason::MissingInput;
        return false;
    }

    if (!has_recipe_output_capacity(building, recipe)) {
        building.blocking_reason = BlockingReason::OutputStorageFull;
        return false;
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

}
