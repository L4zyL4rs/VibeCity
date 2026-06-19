#include "core/Simulation.hpp"

#include <algorithm>
#include <stdexcept>

namespace vibecity {
namespace {

bool has_any(const ResourceArray& amounts)
{
    return std::any_of(amounts.begin(), amounts.end(), [](Quantity quantity) {
        return quantity != 0;
    });
}

}

BuildingId Simulation::add_building(BuildingKind kind)
{
    const auto id = next_building_id_++;
    buildings_.push_back(make_building(id, kind));
    worker_assignment_dirty_ = true;
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

    worker_assignment_dirty_ = false;
}

void Simulation::tick()
{
    if (worker_assignment_dirty_ || minute_of_day() == dawn_tick) {
        assign_workers();
    }

    run_production();

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

}
