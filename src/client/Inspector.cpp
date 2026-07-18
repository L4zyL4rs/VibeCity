#include "client/Inspector.hpp"

#include "client/BuildMenu.hpp"
#include "client/Hud.hpp"
#include "client/Text.hpp"

#include <algorithm>
#include <cstddef>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace vibecity::client {
namespace {

std::string_view resource_display_name(ResourceId resource)
{
    switch (resource) {
    case ResourceId::Grain:
        return "GRAIN";
    case ResourceId::Bread:
        return "BREAD";
    case ResourceId::Timber:
        return "TIMBER";
    case ResourceId::Firewood:
        return "FIREWOOD";
    case ResourceId::Stone:
        return "STONE";
    case ResourceId::Tools:
        return "TOOLS";
    case ResourceId::Bricks:
        return "BRICKS";
    case ResourceId::Pottery:
        return "POTTERY";
    case ResourceId::Count:
        return "UNK";
    }
    return "UNK";
}

std::string_view map_resource_display_name(MapResourceId resource)
{
    switch (resource) {
    case MapResourceId::Forest:
        return "FOREST";
    case MapResourceId::Stone:
        return "STONE";
    case MapResourceId::Clay:
        return "CLAY";
    case MapResourceId::Count:
        return "UNKNOWN";
    }
    return "UNKNOWN";
}

int total_assigned_workers(const Simulation& simulation)
{
    auto workers = 0;
    for (const auto& building : simulation.buildings()) {
        workers += building.assigned_workers;
    }
    return workers;
}

Quantity resource_total(const ResourceArray& amounts)
{
    auto total = Quantity{0};
    for (const auto amount : amounts) {
        total += amount;
    }
    return total;
}

std::string resource_line(const BuildingInstance& building, ResourceId resource)
{
    const auto quantity = building.inventory.quantity(resource);
    const auto capacity = building.inventory.capacity(resource);
    const auto incoming = building.inventory.reserved_incoming(resource);
    const auto outgoing = building.inventory.reserved_outgoing(resource);
    if (quantity <= 0 && capacity <= 0 && incoming <= 0 && outgoing <= 0) {
        return {};
    }

    auto output = std::ostringstream{};
    output << resource_display_name(resource) << ": " << quantity << "/" << capacity;
    if (incoming > 0) {
        output << " IN:" << incoming;
    }
    if (outgoing > 0) {
        output << " OUT:" << outgoing;
    }
    return output.str();
}

std::string stored_pair_line(const ResourceArray& totals, ResourceId first, ResourceId second)
{
    auto output = std::ostringstream{};
    output << resource_display_name(first) << ": " << totals[resource_index(first)]
           << "  " << resource_display_name(second) << ": " << totals[resource_index(second)];
    return output.str();
}

bool has_flow(const ResourceStats& stats, ResourceId resource)
{
    const auto index = resource_index(resource);
    return stats.produced[index] != 0 || stats.consumed[index] != 0 || stats.transported[index] != 0;
}

std::string flow_line(const ResourceStats& stats, ResourceId resource)
{
    const auto index = resource_index(resource);
    auto output = std::ostringstream{};
    output << resource_display_name(resource)
           << "  PRODUCED " << stats.produced[index]
           << "  CONSUMED " << stats.consumed[index]
           << "  TRANSPORTED " << stats.transported[index];
    return output.str();
}

std::string resource_amounts_text(const ResourceArray& amounts)
{
    auto output = std::ostringstream{};
    auto wrote_amount = false;
    for (std::size_t index = 0; index < resource_count; ++index) {
        if (amounts[index] <= 0) {
            continue;
        }
        output << (wrote_amount ? " + " : "")
               << amounts[index] << " "
               << resource_display_name(static_cast<ResourceId>(index));
        wrote_amount = true;
    }
    return wrote_amount ? output.str() : "NONE";
}

std::string resource_capacities_text(const BuildingInstance& building)
{
    auto amounts = empty_resources();
    for (std::size_t index = 0; index < resource_count; ++index) {
        const auto resource = static_cast<ResourceId>(index);
        amounts[index] = building.inventory.capacity(resource);
    }
    return resource_amounts_text(amounts);
}

ResourceArray construction_delivered_amounts(const BuildingInstance& building)
{
    auto amounts = empty_resources();
    for (std::size_t index = 0; index < resource_count; ++index) {
        const auto resource = static_cast<ResourceId>(index);
        amounts[index] = building.inventory.quantity(resource);
    }
    return amounts;
}

ResourceArray construction_incoming_amounts(const BuildingInstance& building)
{
    auto amounts = empty_resources();
    for (std::size_t index = 0; index < resource_count; ++index) {
        const auto resource = static_cast<ResourceId>(index);
        amounts[index] = building.inventory.reserved_incoming(resource);
    }
    return amounts;
}

ResourceArray construction_missing_amounts(const BuildingInstance& building)
{
    auto amounts = empty_resources();
    for (std::size_t index = 0; index < resource_count; ++index) {
        const auto resource = static_cast<ResourceId>(index);
        const auto required = building.inventory.capacity(resource);
        if (required <= 0) {
            continue;
        }
        const auto supplied = building.inventory.quantity(resource)
            + building.inventory.reserved_incoming(resource);
        amounts[index] = std::max<Quantity>(0, required - supplied);
    }
    return amounts;
}

std::string truncate_text(std::string text, std::size_t maximum)
{
    if (text.size() <= maximum) {
        return text;
    }
    if (maximum == 0) {
        return {};
    }
    text.resize(maximum);
    text.back() = '.';
    return text;
}

std::string uppercase_ascii(std::string_view text)
{
    auto output = std::string{};
    output.reserve(text.size());
    for (const auto character : text) {
        if (character >= 'a' && character <= 'z') {
            output.push_back(static_cast<char>(character - 'a' + 'A'));
        } else {
            output.push_back(character);
        }
    }
    return output;
}

const BuildingInstance* find_active_building(const Simulation& simulation, BuildingId id)
{
    for (const auto& building : simulation.buildings()) {
        if (building.active && building.id == id) {
            return &building;
        }
    }
    return nullptr;
}

std::string short_building_label(const Simulation& simulation, BuildingId id)
{
    if (const auto* building = find_active_building(simulation, id)) {
        auto output = std::ostringstream{};
        output << "#" << building->id << " " << simulation.definition(building->kind).name;
        return truncate_text(output.str(), 18);
    }

    return "#" + std::to_string(id) + " UNKNOWN";
}

void add_unique_building(std::vector<BuildingId>& ids, BuildingId id)
{
    if (std::find(ids.begin(), ids.end(), id) == ids.end()) {
        ids.push_back(id);
    }
}

std::string transport_state_label(TransportJobState state)
{
    switch (state) {
    case TransportJobState::GoingToPickup:
        return "PICKUP";
    case TransportJobState::CarryingGoods:
        return "CARRY";
    case TransportJobState::Complete:
        return "DONE";
    case TransportJobState::Failed:
        return "FAILED";
    }
    return "UNKNOWN";
}

std::string construction_status_label(const BuildingInstance& site)
{
    if (!site.work_enabled) {
        return "WORK PAUSED";
    }
    if (site.assigned_builders > 0) {
        return "BUILDING";
    }

    switch (site.blocking_reason) {
    case BlockingReason::MissingConstructionMaterial:
        return "WAITING FOR MATERIALS";
    case BlockingReason::NoReachableSource:
        return "NO REACHABLE MATERIAL SOURCE";
    case BlockingReason::WaitingForHauler:
        return "WAITING FOR HAULER";
    case BlockingReason::WaitingForBuilderLabor:
        return "WAITING FOR BUILDERS";
    case BlockingReason::WorkDisabled:
        return "WORK PAUSED";
    case BlockingReason::None:
    case BlockingReason::NotEnoughWorkers:
    case BlockingReason::MissingInput:
    case BlockingReason::OutputStorageFull:
    case BlockingReason::MissingBread:
    case BlockingReason::NoNearbyMapResource:
        break;
    }

    if (resource_total(construction_missing_amounts(site)) > 0) {
        return "WAITING FOR MATERIALS";
    }
    if (site.construction_labor_completed < site.construction_labor_required) {
        return "WAITING FOR BUILDERS";
    }
    return "READY";
}

ResourceArray reserved_amounts(const BuildingInstance& building, bool incoming)
{
    auto amounts = empty_resources();
    for (std::size_t index = 0; index < resource_count; ++index) {
        const auto resource = static_cast<ResourceId>(index);
        amounts[index] = incoming
            ? building.inventory.reserved_incoming(resource)
            : building.inventory.reserved_outgoing(resource);
    }
    return amounts;
}

ResourceArray daily_recipe_amounts(const ResourceArray& amounts, Tick cycle_minutes)
{
    auto daily = empty_resources();
    for (std::size_t index = 0; index < resource_count; ++index) {
        daily[index] = amounts[index] * ticks_per_day / cycle_minutes;
    }
    return daily;
}

std::string duration_text(Tick minutes)
{
    if (minutes % ticks_per_hour == 0) {
        return std::to_string(minutes / ticks_per_hour) + " HOURS";
    }
    return std::to_string(minutes) + " MINUTES";
}

std::string capability_summary_line(const Simulation& simulation)
{
    auto output = std::ostringstream{};
    auto wrote_capability = false;
    for (std::size_t index = 0; index < capability_count; ++index) {
        const auto capability = static_cast<CapabilityId>(index);
        if (!simulation.has_capability(capability)) {
            continue;
        }
        output << (wrote_capability ? ", " : "")
               << capability_name(capability);
        wrote_capability = true;
    }
    return wrote_capability ? output.str() : "NONE";
}

const DiscoveryProject* active_project(
    const Simulation& simulation,
    DiscoveryProjectId project_id)
{
    for (const auto& project : simulation.discovery_projects()) {
        if (project.project == project_id) {
            return &project;
        }
    }
    return nullptr;
}

std::string discovery_project_required_host_name(
    const Simulation& simulation,
    const DiscoveryProjectDefinition& project)
{
    const auto kind = simulation.building_catalog().find_kind(project.required_host_stable_id);
    if (!kind.has_value()) {
        return std::string{project.required_host_stable_id};
    }
    return simulation.definition(*kind).name;
}

std::string discovery_project_requirement_line(const DiscoveryProjectDefinition& project)
{
    auto line = std::string{"NEEDS: "} + resource_amounts_text(project.inputs);
    if (project.map_resource_quantity > 0) {
        line += " + ";
        line += std::to_string(project.map_resource_quantity);
        line += " ";
        line += map_resource_display_name(project.map_resource);
        line += " NEARBY";
    }
    return truncate_text(line, 48);
}

std::string discovery_project_status_line(
    const DiscoveryProjectDefinition& project,
    const DiscoveryProjectStartStatus& status)
{
    switch (status.blocker) {
    case DiscoveryProjectStartBlocker::None:
        return "STATUS: READY AT THIS HOST";
    case DiscoveryProjectStartBlocker::CapabilityAlreadyDiscovered:
        return "STATUS: " + uppercase_ascii(capability_name(project.grants_capability)) + " DISCOVERED";
    case DiscoveryProjectStartBlocker::AlreadyActive:
        return "STATUS: PROJECT ALREADY ACTIVE";
    case DiscoveryProjectStartBlocker::MissingCapability:
        if (project.required_capability.has_value()) {
            return "STATUS: NEEDS "
                + uppercase_ascii(capability_name(*project.required_capability));
        }
        return "STATUS: NEEDS PREREQUISITE";
    case DiscoveryProjectStartBlocker::InvalidHost:
        return "STATUS: SELECT A PLACED HOST";
    case DiscoveryProjectStartBlocker::WrongHost:
        return "STATUS: NEEDS DIFFERENT HOST";
    case DiscoveryProjectStartBlocker::MissingPathAccess:
        return "STATUS: HOST NEEDS ROAD ACCESS";
    case DiscoveryProjectStartBlocker::MissingInputs:
        return "STATUS: STARTS, NEEDS DELIVERY";
    case DiscoveryProjectStartBlocker::MissingMapResource:
        return "STATUS: NOT ENOUGH CLAY NEARBY";
    }
    return "STATUS: UNKNOWN BLOCKER";
}

ResourceArray missing_project_inputs(
    const BuildingInstance& host,
    const DiscoveryProjectDefinition& project)
{
    auto missing = empty_resources();
    for (std::size_t index = 0; index < resource_count; ++index) {
        const auto required = project.inputs[index];
        if (required <= 0) {
            continue;
        }
        const auto available = host.inventory.available(static_cast<ResourceId>(index));
        if (available < required) {
            missing[index] = required - available;
        }
    }
    return missing;
}

std::string strip_status_prefix(std::string status)
{
    constexpr auto prefix = std::string_view{"STATUS: "};
    if (status.rfind(prefix, 0) == 0) {
        status.erase(0, prefix.size());
    }
    return status;
}

std::string map_resource_availability_line(
    const Simulation& simulation,
    const BuildingInstance& host,
    const DiscoveryProjectDefinition& project)
{
    if (!host.position.has_value()) {
        return std::string{map_resource_display_name(project.map_resource)}
            + " IN RANGE: 0/" + std::to_string(project.map_resource_quantity);
    }

    const auto& host_definition = simulation.definition(host.kind);
    const auto available = simulation.map().map_resource_quantity_within_radius(
        *host.position,
        host_definition.footprint,
        project.map_resource,
        project.map_radius);
    return std::string{map_resource_display_name(project.map_resource)}
        + " IN RANGE: " + std::to_string(available)
        + "/" + std::to_string(project.map_resource_quantity);
}

int draw_selected_discovery_project(SDL_Renderer* renderer,
    const Simulation& simulation,
    const BuildingInstance& building,
    int x,
    int y,
    Color text,
    Color muted)
{
    const auto selected_project = simulation.discovery_project_for_host(building.id);
    if (!selected_project.has_value()) {
        return y;
    }
    const auto project_id = *selected_project;
    const auto& project = discovery_project_definition(project_id);
    const auto* active = active_project(simulation, project_id);
    if (active != nullptr && active->host != building.id) {
        return y;
    }

    draw_text(renderer, x, y, "DISCOVERY", text, 2);
    y += 24;
    draw_text(renderer, x, y, std::string{project.name}, muted, 2);
    y += 20;
    draw_text(renderer, x, y, discovery_project_requirement_line(project), muted, 1);
    y += 16;
    draw_text(renderer,
        x,
        y,
        std::string{"LABOR: "} + duration_text(project.labor_minutes)
            + "  WORKERS: " + std::to_string(project.worker_slots),
        muted,
        1);
    y += 16;

    if (active != nullptr) {
        const auto hosted_here = active->host == building.id;
        draw_text(renderer,
            x,
            y,
            hosted_here
                ? "STATUS: ACTIVE HERE"
                : "STATUS: ACTIVE AT " + short_building_label(simulation, active->host),
            muted,
            1);
        y += 16;

        if (!active->materials_consumed) {
            const auto* host = find_active_building(simulation, active->host);
            if (host != nullptr) {
                const auto missing = missing_project_inputs(*host, project);
                if (resource_total(missing) > 0) {
                    draw_text(renderer,
                        x,
                        y,
                        "WAITING FOR: " + resource_amounts_text(missing),
                        muted,
                        1);
                    y += 16;
                } else {
                    draw_text(renderer, x, y, "WAITING TO BEGIN", muted, 1);
                    y += 16;
                }

                draw_text(renderer,
                    x,
                    y,
                    map_resource_availability_line(simulation, *host, project),
                    muted,
                    1);
                y += 16;
            }
            return y + 8;
        }

        draw_text(renderer,
            x,
            y,
            std::string{"LABOR REMAINING: "}
                + duration_text(std::max<Tick>(0, project.labor_minutes - active->labor_completed)),
            muted,
            1);
        y += 24;
        return y;
    }

    const auto start_status = simulation.discovery_project_start_status(project_id, building.id);
    draw_text(renderer, x, y, discovery_project_status_line(project, start_status), muted, 1);
    y += 16;

    if (start_status.blocker == DiscoveryProjectStartBlocker::MissingInputs) {
        draw_text(renderer,
            x,
            y,
            "MISSING: " + resource_amounts_text(start_status.missing_inputs),
            muted,
            1);
        y += 16;
    }

    if (start_status.blocker != DiscoveryProjectStartBlocker::WrongHost
        && start_status.blocker != DiscoveryProjectStartBlocker::InvalidHost
        && start_status.blocker != DiscoveryProjectStartBlocker::MissingCapability
        && start_status.blocker != DiscoveryProjectStartBlocker::MissingPathAccess) {
        draw_text(renderer,
            x,
            y,
            std::string{map_resource_display_name(project.map_resource)}
                + " IN RANGE: " + std::to_string(start_status.map_resource_available)
                + "/" + std::to_string(project.map_resource_quantity),
            muted,
            1);
        y += 16;
    }

    return y + 8;
}

std::string objective_progress_line(const VillageObjectiveStatus& status)
{
    auto output = std::ostringstream{};
    output << (status.complete ? "OK " : "-- ") << status.label;
    if (status.target > 1) {
        output << " " << std::min(status.current, status.target) << "/" << status.target;
    }
    return output.str();
}

std::string objective_short_line(const VillageObjectiveStatus& status)
{
    auto output = std::ostringstream{};
    output << status.label;
    if (status.target > 1) {
        output << " " << std::min(status.current, status.target) << "/" << status.target;
    }
    return output.str();
}

int draw_objective_summary(SDL_Renderer* renderer,
    const VillageObjectiveTracker& objectives,
    int x,
    int y,
    Color text,
    Color muted)
{
    draw_text(renderer, x, y, "OBJECTIVE", text, 2);
    y += 24;

    const auto completed = objectives.completed_count();
    const auto* active = objectives.active_status();

    if (active == nullptr) {
        draw_text(renderer, x, y, "OK VILLAGE STABLE", text, 2);
    } else {
        draw_text(renderer, x, y, objective_progress_line(*active), muted, 2);
    }
    y += 20;

    if (active != nullptr) {
        auto after_active = false;
        auto next_count = 0;
        for (const auto& status : objectives.statuses()) {
            if (&status == active) {
                after_active = true;
                continue;
            }
            if (!after_active || status.complete) {
                continue;
            }

            draw_text(
                renderer,
                x,
                y,
                "NEXT: " + objective_short_line(status),
                muted,
                1);
            y += 16;
            ++next_count;
            if (next_count >= 2) {
                break;
            }
        }
        if (next_count > 0) {
            y += 4;
        }
    }

    draw_text(renderer,
        x,
        y,
        std::string{"DONE: "} + std::to_string(completed)
            + "/" + std::to_string(village_objective_count),
        muted,
        2);
    y += 28;

    return y;
}

int draw_economy_summary(SDL_Renderer* renderer,
    const Simulation& simulation,
    const VillageObjectiveTracker& objectives,
    int x,
    int y,
    Color text,
    Color muted)
{
    draw_text(renderer, x, y, "SETTLEMENT", text, 2);
    y += 24;

    draw_text(renderer, x, y, clock_text(simulation), muted, 2);
    y += 20;

    draw_text(renderer, x, y,
        std::string{"POPULATION: "} + std::to_string(simulation.total_residents())
            + " / " + std::to_string(simulation.total_housing_capacity()),
        muted, 2);
    y += 20;

    draw_text(renderer, x, y,
        std::string{"FREE HOMES: "} + std::to_string(simulation.free_housing_capacity()),
        muted, 2);
    y += 20;

    draw_text(renderer, x, y,
        std::string{"BREAD: "} + std::to_string(simulation.stored_bread())
            + "  DAYS: " + std::to_string(simulation.bread_days_remaining()),
        muted, 2);
    y += 20;

    draw_text(renderer, x, y,
        std::string{"DAILY BREAD NEED: "} + std::to_string(simulation.daily_bread_need()),
        muted, 2);
    y += 20;

    if (simulation.free_housing_capacity() > 0) {
        draw_text(renderer, x, y,
            std::string{"GROWTH BREAD: "} + std::to_string(simulation.stored_bread())
                + " / " + std::to_string(simulation.bread_required_for_population_growth()),
            muted, 2);
        y += 20;
    }

    draw_text(renderer, x, y,
        std::string{"GROWTH: "}
            + std::string{population_growth_blocker_text(simulation.population_growth_blocker())},
        muted, 2);
    y += 20;

    draw_text(renderer, x, y,
        std::string{"WORKERS: "} + std::to_string(total_assigned_workers(simulation))
            + " ASSIGNED  " + std::to_string(simulation.idle_workers()) + " IDLE",
        muted, 1);
    y += 20;

    const auto construction = simulation.construction_summary();
    if (construction.sites > 0) {
        y += 8;
        draw_text(renderer, x, y, "CONSTRUCTION", text, 2);
        y += 24;
        draw_text(renderer, x, y,
            std::string{"SITES: "} + std::to_string(construction.sites)
                + "  ACTIVE BUILDERS: " + std::to_string(construction.active_builders),
            muted, 1);
        y += 16;
        draw_text(renderer, x, y,
            std::string{"WAITING: MATERIALS "} + std::to_string(construction.waiting_materials)
                + "  HAULERS " + std::to_string(construction.waiting_logistics),
            muted, 1);
        y += 16;
        draw_text(renderer, x, y,
            std::string{"WAITING FOR BUILDERS: "} + std::to_string(construction.waiting_builders),
            muted, 1);
        y += 20;

        if (construction.next_site.has_value() && construction.next_target.has_value()) {
            draw_text(renderer, x, y,
                std::string{"NEXT: #"} + std::to_string(*construction.next_site)
                    + " " + simulation.definition(*construction.next_target).name,
                muted, 2);
            y += 20;
            draw_text(renderer, x, y,
                std::string{"LABOR REMAINING: "}
                    + duration_text(construction.next_labor_remaining),
                muted, 1);
            y += 20;
        }
    }

    const auto roadwork = simulation.roadwork_summary();
    if (roadwork.sites > 0) {
        y += 8;
        draw_text(renderer, x, y, "ROADWORK", text, 2);
        y += 24;
        draw_text(renderer, x, y,
            std::string{"SITES: "} + std::to_string(roadwork.sites)
                + "  ACTIVE BUILDERS: " + std::to_string(roadwork.active_builders),
            muted, 1);
        y += 16;
        draw_text(renderer, x, y,
            std::string{"WAITING FOR BUILDERS: "} + std::to_string(roadwork.waiting_builders),
            muted, 1);
        y += 16;
        if (roadwork.next_site.has_value()) {
            draw_text(renderer, x, y,
                std::string{"NEXT: "}
                    + std::to_string(roadwork.next_site->x)
                    + "," + std::to_string(roadwork.next_site->y),
                muted, 2);
            y += 20;
            draw_text(renderer, x, y,
                std::string{"LABOR REMAINING: "}
                    + duration_text(roadwork.next_labor_remaining),
                muted, 1);
            y += 20;
        }
    }

    const auto projects = simulation.discovery_project_summary();
    if (projects.active_projects > 0) {
        y += 8;
        draw_text(renderer, x, y, "DISCOVERY", text, 2);
        y += 24;
        draw_text(renderer, x, y,
            std::string{"PROJECTS: "} + std::to_string(projects.active_projects)
                + "  WORKERS: " + std::to_string(projects.active_workers),
            muted, 1);
        y += 16;
        if (projects.next_project.has_value()) {
            draw_text(renderer, x, y,
                std::string{"NEXT: "}
                    + std::string{discovery_project_definition(*projects.next_project).name},
                muted, 2);
            y += 20;
            draw_text(renderer, x, y,
                std::string{"LABOR REMAINING: "}
                    + duration_text(projects.next_labor_remaining),
                muted, 1);
            y += 20;
        }
    }

    const auto logistics = simulation.logistics_summary();
    if (logistics.active_jobs > 0
        || resource_total(logistics.reserved_incoming) > 0
        || resource_total(logistics.reserved_outgoing) > 0) {
        y += 8;
        draw_text(renderer, x, y, "LOGISTICS", text, 2);
        y += 24;
        draw_text(renderer, x, y,
            std::string{"JOBS: "} + std::to_string(logistics.active_jobs)
                + "  PICKUP: " + std::to_string(logistics.going_to_pickup)
                + "  CARRYING: " + std::to_string(logistics.carrying_goods),
            muted, 1);
        y += 16;
        draw_text(renderer, x, y,
            std::string{"RESERVED IN: "}
                + std::to_string(resource_total(logistics.reserved_incoming))
                + "  OUT: " + std::to_string(resource_total(logistics.reserved_outgoing)),
            muted, 1);
        y += 16;
        draw_text(renderer, x, y,
            std::string{"GOODS IN TRANSIT: "}
                + std::to_string(resource_total(logistics.in_transit)),
            muted, 1);
        y += 24;
    } else {
        y += 8;
    }

    y = draw_objective_summary(renderer, objectives, x, y, text, muted);

    draw_text(renderer, x, y, "CAPABILITIES", text, 2);
    y += 24;
    draw_text(renderer, x, y, capability_summary_line(simulation), muted, 2);
    y += 28;

    const auto totals = simulation.total_inventory();
    draw_text(renderer, x, y, "STORED", text, 2);
    y += 24;
    draw_text(renderer, x, y, stored_pair_line(totals, ResourceId::Bread, ResourceId::Grain), muted, 2);
    y += 20;
    draw_text(renderer, x, y, stored_pair_line(totals, ResourceId::Timber, ResourceId::Firewood), muted, 2);
    y += 20;
    draw_text(renderer, x, y, stored_pair_line(totals, ResourceId::Stone, ResourceId::Tools), muted, 2);
    y += 28;

    draw_text(renderer, x, y, "CUMULATIVE FLOW", text, 2);
    y += 24;

    auto drew_flow = false;
    for (std::size_t index = 0; index < resource_count; ++index) {
        const auto resource = static_cast<ResourceId>(index);
        if (!has_flow(simulation.stats(), resource)) {
            continue;
        }
        draw_text(renderer, x, y, flow_line(simulation.stats(), resource), muted, 1);
        y += 16;
        drew_flow = true;
    }

    if (!drew_flow) {
        draw_text(renderer, x, y, "NO PRODUCTION OR TRANSPORT YET", muted, 1);
        y += 20;
    }

    return y + 10;
}

int draw_inspector_content(SDL_Renderer* renderer,
    const Simulation& simulation,
    const VillageObjectiveTracker& objectives,
    std::optional<BuildingId> selected,
    int x,
    int y,
    Color text,
    Color muted)
{
    y = draw_economy_summary(renderer, simulation, objectives, x, y, text, muted);

    if (!selected.has_value()) {
        draw_text(renderer, x, y, "NO SELECTION", muted, 2);
        return y + 30;
    }

    draw_text(renderer, x, y, "SELECTION", text, 2);
    y += 24;

    const auto& building = simulation.building(*selected);
    auto title = std::ostringstream{};
    title << "#" << building.id << " " << simulation.definition(building.kind).name;
    draw_text(renderer, x, y, title.str(), text, 2);
    y += 24;

    auto block = std::ostringstream{};
    block << "BLOCK: " << blocking_reason_text(building.blocking_reason);
    draw_text(renderer, x, y, block.str(), muted, 2);
    y += 24;

    draw_text(renderer,
        x,
        y,
        std::string{"WORK: "} + (building.work_enabled ? "ON" : "OFF"),
        muted,
        2);
    y += 20;

    auto workers = std::ostringstream{};
    workers << "WORKERS: " << building.assigned_workers;
    draw_text(renderer, x, y, workers.str(), muted, 2);
    y += 20;

    auto residents = std::ostringstream{};
    residents << "RESIDENTS: " << building.residents;
    draw_text(renderer, x, y, residents.str(), muted, 2);
    y += 28;

    y = draw_selected_discovery_project(
        renderer,
        simulation,
        building,
        x,
        y,
        text,
        muted);

    const auto& instance_definition = simulation.definition(building.kind);
    const auto* operating_definition = &instance_definition;
    if (instance_definition.internal_construction_site
        && building.construction_target.has_value()) {
        operating_definition = &simulation.definition(*building.construction_target);
        const auto construction_lines = construction_site_detail_lines(simulation, building.id);
        if (!construction_lines.empty()) {
            draw_text(renderer, x, y, "CONSTRUCTION", text, 2);
            y += 24;
            for (const auto& line : construction_lines) {
                draw_text(renderer, x, y, line, muted, 1);
                y += 16;
            }
            y += 8;
        }
    }

    if (operating_definition->recipe.has_value()) {
        const auto& recipe = *operating_definition->recipe;
        draw_text(renderer, x, y, "PRODUCTION", text, 2);
        y += 24;
        draw_text(
            renderer,
            x,
            y,
            std::string{"INPUT EACH CYCLE: "} + resource_amounts_text(recipe.inputs),
            muted,
            1);
        y += 16;
        draw_text(
            renderer,
            x,
            y,
            std::string{"OUTPUT EACH CYCLE: "} + resource_amounts_text(recipe.outputs),
            muted,
            1);
        y += 16;
        draw_text(
            renderer,
            x,
            y,
            std::string{"CYCLE AT FULL STAFF: "} + duration_text(recipe.cycle_minutes),
            muted,
            1);
        y += 16;
        draw_text(
            renderer,
            x,
            y,
            std::string{"DAILY INPUT CAPACITY: "}
                + resource_amounts_text(daily_recipe_amounts(recipe.inputs, recipe.cycle_minutes)),
            muted,
            1);
        y += 16;
        draw_text(
            renderer,
            x,
            y,
            std::string{"DAILY OUTPUT CAPACITY: "}
                + resource_amounts_text(daily_recipe_amounts(recipe.outputs, recipe.cycle_minutes)),
            muted,
            1);
        y += 24;
    } else if (operating_definition->consumes_bread) {
        draw_text(renderer, x, y, "CONSUMPTION", text, 2);
        y += 24;
        draw_text(renderer, x, y, "1 BREAD PER RESIDENT PER DAY", muted, 1);
        y += 16;
        draw_text(
            renderer,
            x,
            y,
            std::string{"CURRENT DAILY NEED: "} + std::to_string(building.residents) + " BREAD",
            muted,
            1);
        y += 24;
    }

    if (operating_definition->gathering.has_value()
        && building.position.has_value()) {
        const auto& gathering = *operating_definition->gathering;
        const auto available = simulation.map().map_resource_quantity_within_radius(
            *building.position,
            operating_definition->footprint,
            gathering.resource,
            gathering.radius);
        draw_text(renderer, x, y, "MAP RESOURCE", text, 2);
        y += 24;
        draw_text(renderer, x, y,
            std::string{"USES: "} + std::to_string(gathering.units_per_cycle)
                + " " + std::string{map_resource_display_name(gathering.resource)}
                + " EACH CYCLE",
            muted, 1);
        y += 16;
        draw_text(renderer, x, y,
            std::string{"COLLECTION RADIUS: "} + std::to_string(gathering.radius)
                + " TILES",
            muted, 1);
        y += 16;
        draw_text(renderer, x, y,
            std::string{map_resource_display_name(gathering.resource)}
                + " IN RANGE: " + std::to_string(available),
            muted, 1);
        y += 24;
    }

    const auto logistics_lines = selected_logistics_lines(simulation, building.id);
    if (!logistics_lines.empty()) {
        draw_text(renderer, x, y, "LOGISTICS", text, 2);
        y += 24;
        for (const auto& line : logistics_lines) {
            draw_text(renderer, x, y, line, muted, 1);
            y += 16;
        }
        y += 8;
    }

    draw_text(renderer, x, y, "INVENTORY", text, 2);
    y += 24;

    auto drew_inventory = false;
    for (std::size_t index = 0; index < resource_count; ++index) {
        const auto resource = static_cast<ResourceId>(index);
        const auto line = resource_line(building, resource);
        if (line.empty()) {
            continue;
        }
        draw_text(renderer, x, y, line, muted, 2);
        y += 20;
        drew_inventory = true;
    }

    if (!drew_inventory) {
        draw_text(renderer, x, y, "EMPTY", muted, 2);
        y += 20;
    }

    return y + 18;
}

void draw_scrollbar(SDL_Renderer* renderer,
    const SDL_Rect& content_rect,
    int scroll_offset,
    int max_scroll,
    Color track,
    Color thumb)
{
    if (max_scroll <= 0 || content_rect.h <= 0) {
        return;
    }

    const auto content_height = content_rect.h + max_scroll;
    const auto thumb_height = std::max(24, content_rect.h * content_rect.h / content_height);
    const auto thumb_travel = std::max(0, content_rect.h - thumb_height);
    const auto thumb_y = content_rect.y
        + (max_scroll > 0 ? scroll_offset * thumb_travel / max_scroll : 0);

    const auto track_rect = SDL_Rect{
        .x = content_rect.x + content_rect.w - 6,
        .y = content_rect.y,
        .w = 4,
        .h = content_rect.h
    };
    const auto thumb_rect = SDL_Rect{
        .x = track_rect.x,
        .y = thumb_y,
        .w = track_rect.w,
        .h = thumb_height
    };

    set_color(renderer, track);
    SDL_RenderFillRect(renderer, &track_rect);
    set_color(renderer, thumb);
    SDL_RenderFillRect(renderer, &thumb_rect);
}

void draw_discovery_project_action_button(SDL_Renderer* renderer,
    const Simulation& simulation,
    std::optional<BuildingId> selected,
    int window_width,
    int window_height,
    Color text,
    Color muted)
{
    const auto action = discovery_project_action(simulation, selected);
    const auto rect = inspector_discovery_project_action_rect(window_width, window_height);
    if (!action.has_value() || !rect.has_value()) {
        return;
    }

    const auto footer = SDL_Rect{
        .x = rect->x - 8,
        .y = rect->y - 8,
        .w = rect->w + 16,
        .h = window_height - rect->y + 8
    };
    set_color(renderer, Color{20, 23, 23, 252});
    SDL_RenderFillRect(renderer, &footer);

    if (action->can_start) {
        set_color(renderer, Color{66, 112, 84, 255});
    } else if (action->active) {
        set_color(renderer, Color{96, 90, 66, 255});
    } else {
        set_color(renderer, Color{74, 70, 68, 255});
    }
    SDL_RenderFillRect(renderer, &*rect);
    set_color(renderer, action->can_start ? Color{142, 190, 132, 255} : Color{92, 96, 94, 255});
    SDL_RenderDrawRect(renderer, &*rect);

    draw_text(renderer, rect->x + 10, rect->y + 7, truncate_text(action->label, 26), text, 2);
    draw_text(renderer, rect->x + 10, rect->y + 25, truncate_text(action->status, 40), muted, 1);
}

}

std::vector<std::string> selected_logistics_lines(
    const Simulation& simulation,
    BuildingId selected)
{
    auto lines = std::vector<std::string>{};
    const auto* building = find_active_building(simulation, selected);
    if (building == nullptr) {
        return lines;
    }

    const auto incoming_reserved = reserved_amounts(*building, true);
    const auto outgoing_reserved = reserved_amounts(*building, false);
    if (resource_total(incoming_reserved) > 0) {
        lines.push_back("RESERVED IN: " + resource_amounts_text(incoming_reserved));
    }
    if (resource_total(outgoing_reserved) > 0) {
        lines.push_back("RESERVED OUT: " + resource_amounts_text(outgoing_reserved));
    }

    auto suppliers = std::vector<BuildingId>{};
    auto customers = std::vector<BuildingId>{};
    auto active_in = Quantity{0};
    auto active_out = Quantity{0};
    for (const auto& job : simulation.transport_jobs()) {
        if (job.destination == selected) {
            add_unique_building(suppliers, job.source);
            active_in += job.quantity;
        }
        if (job.source == selected) {
            add_unique_building(customers, job.destination);
            active_out += job.quantity;
        }
    }

    if (!suppliers.empty()) {
        lines.push_back(
            "SUPPLIERS: " + std::to_string(suppliers.size())
            + "  ACTIVE IN: " + std::to_string(active_in));
    }
    if (!customers.empty()) {
        lines.push_back(
            "CUSTOMERS: " + std::to_string(customers.size())
            + "  ACTIVE OUT: " + std::to_string(active_out));
    }

    for (const auto& job : simulation.transport_jobs()) {
        if (job.destination != selected && job.source != selected) {
            continue;
        }

        const auto incoming = job.destination == selected;
        auto line = std::ostringstream{};
        line << (incoming ? "IN " : "OUT ")
             << job.quantity << " " << resource_display_name(job.resource)
             << (incoming ? " FROM " : " TO ")
             << short_building_label(simulation, incoming ? job.source : job.destination)
             << " " << transport_state_label(job.state)
             << " " << std::max<Tick>(0, job.ticks_remaining) << "M";
        lines.push_back(truncate_text(line.str(), 48));
    }

    return lines;
}

std::vector<std::string> construction_site_detail_lines(
    const Simulation& simulation,
    BuildingId selected)
{
    auto lines = std::vector<std::string>{};
    const auto* site = find_active_building(simulation, selected);
    if (site == nullptr || !site->construction_target.has_value()
        || !simulation.definition(site->kind).internal_construction_site) {
        return lines;
    }

    const auto& target = simulation.definition(*site->construction_target);
    const auto delivered = construction_delivered_amounts(*site);
    const auto incoming = construction_incoming_amounts(*site);
    const auto missing = construction_missing_amounts(*site);
    const auto labor_left = std::max<Tick>(
        0,
        site->construction_labor_required - site->construction_labor_completed);

    lines.push_back("TARGET: " + target.name);
    lines.push_back("STATUS: " + construction_status_label(*site));
    lines.push_back("REQUIRED: " + resource_capacities_text(*site));
    lines.push_back("DELIVERED: " + resource_amounts_text(delivered));
    if (resource_total(incoming) > 0) {
        lines.push_back("INCOMING: " + resource_amounts_text(incoming));
    }
    lines.push_back("MISSING: " + resource_amounts_text(missing));
    lines.push_back(
        "BUILDERS: " + std::to_string(site->assigned_builders)
        + "/" + std::to_string(prototype_builders_per_site));
    lines.push_back("LABOR LEFT: " + duration_text(labor_left));
    return lines;
}

std::vector<std::string> discovery_project_detail_lines(
    const Simulation& simulation,
    std::optional<BuildingId> selected)
{
    auto lines = std::vector<std::string>{};
    if (!selected.has_value()) {
        return lines;
    }

    const auto* selected_building = find_active_building(simulation, *selected);
    if (selected_building == nullptr) {
        return lines;
    }

    const auto selected_project = simulation.discovery_project_for_host(selected_building->id);
    if (!selected_project.has_value()) {
        return lines;
    }

    const auto project_id = *selected_project;
    const auto& project = discovery_project_definition(project_id);
    const auto* active = active_project(simulation, project_id);
    if (active != nullptr && active->host != selected_building->id) {
        return lines;
    }

    lines.push_back(std::string{project.name});
    lines.push_back("HOST: " + discovery_project_required_host_name(simulation, project));
    lines.push_back("ROAD: HOST NEEDS ACCESS");
    lines.push_back("INPUT: " + resource_amounts_text(project.inputs));
    lines.push_back(
        "MAP: " + std::to_string(project.map_resource_quantity)
        + " " + std::string{map_resource_display_name(project.map_resource)}
        + " WITHIN " + std::to_string(project.map_radius) + " TILES");
    lines.push_back(
        "LABOR: " + duration_text(project.labor_minutes)
        + ", " + std::to_string(project.worker_slots) + " WORKERS");
    lines.push_back("UNLOCKS: " + uppercase_ascii(capability_name(project.grants_capability)));

    if (active != nullptr) {
        lines.push_back("STATUS: ACTIVE HERE");

        const auto* host = find_active_building(simulation, active->host);
        if (host == nullptr) {
            return lines;
        }

        if (!active->materials_consumed) {
            const auto missing = missing_project_inputs(*host, project);
            if (resource_total(missing) > 0) {
                lines.push_back("WAITING: " + resource_amounts_text(missing));
            } else {
                lines.push_back("WAITING: MATERIALS READY");
            }
            lines.push_back(map_resource_availability_line(simulation, *host, project));
            return lines;
        }

        lines.push_back(
            "LABOR LEFT: "
            + duration_text(std::max<Tick>(0, project.labor_minutes - active->labor_completed)));
        lines.push_back("WORKERS NOW: " + std::to_string(active->assigned_workers));
        return lines;
    }

    const auto start_status = simulation.discovery_project_start_status(project_id, selected_building->id);
    lines.push_back(discovery_project_status_line(project, start_status));
    if (start_status.blocker == DiscoveryProjectStartBlocker::MissingInputs) {
        lines.push_back("MISSING: " + resource_amounts_text(start_status.missing_inputs));
    }
    if (start_status.blocker != DiscoveryProjectStartBlocker::WrongHost
        && start_status.blocker != DiscoveryProjectStartBlocker::InvalidHost
        && start_status.blocker != DiscoveryProjectStartBlocker::MissingCapability
        && start_status.blocker != DiscoveryProjectStartBlocker::MissingPathAccess) {
        lines.push_back(map_resource_availability_line(simulation, *selected_building, project));
    }

    return lines;
}

std::optional<DiscoveryProjectAction> discovery_project_action(
    const Simulation& simulation,
    std::optional<BuildingId> selected)
{
    if (!selected.has_value()) {
        return std::nullopt;
    }

    const auto* selected_building = find_active_building(simulation, *selected);
    if (selected_building == nullptr) {
        return std::nullopt;
    }

    const auto project_id = simulation.discovery_project_for_host(selected_building->id);
    if (!project_id.has_value()) {
        return std::nullopt;
    }

    const auto& project = discovery_project_definition(*project_id);
    const auto* active = active_project(simulation, *project_id);
    if (active != nullptr && active->host != selected_building->id) {
        return std::nullopt;
    }

    auto action = DiscoveryProjectAction{
        .host = selected_building->id,
        .project = *project_id,
        .can_start = false,
        .active = false,
        .label = {},
        .status = {}
    };

    if (active != nullptr) {
        action.active = true;
        action.label = "ACTIVE " + std::string{project.name};
        if (!active->materials_consumed) {
            action.status = "WAITING FOR INPUTS";
        } else {
            action.status = "LABOR LEFT "
                + duration_text(std::max<Tick>(
                    0,
                    project.labor_minutes - active->labor_completed));
        }
        return action;
    }

    const auto start_status = simulation.discovery_project_start_status(
        *project_id,
        selected_building->id);
    action.can_start = simulation.can_start_discovery_project(
        *project_id,
        selected_building->id);
    action.label = (action.can_start ? "START " : "BLOCKED ") + std::string{project.name};
    action.status = strip_status_prefix(discovery_project_status_line(project, start_status));
    return action;
}

std::optional<SDL_Rect> inspector_discovery_project_action_rect(
    int window_width,
    int window_height)
{
    if (window_width < inspector_width || window_height < hud_height + 150) {
        return std::nullopt;
    }

    return SDL_Rect{
        .x = window_width - inspector_width + 18,
        .y = window_height - 58,
        .w = inspector_width - 36,
        .h = 40
    };
}

std::optional<DiscoveryProjectAction> discovery_project_action_at(
    const Simulation& simulation,
    std::optional<BuildingId> selected,
    int window_width,
    int window_height,
    int screen_x,
    int screen_y)
{
    const auto action = discovery_project_action(simulation, selected);
    const auto rect = inspector_discovery_project_action_rect(window_width, window_height);
    if (!action.has_value() || !rect.has_value()) {
        return std::nullopt;
    }

    if (screen_x < rect->x
        || screen_y < rect->y
        || screen_x >= rect->x + rect->w
        || screen_y >= rect->y + rect->h) {
        return std::nullopt;
    }
    return action;
}

void draw_discovery_project_popup(SDL_Renderer* renderer,
    const Simulation& simulation,
    std::optional<BuildingId> selected)
{
    const auto lines = discovery_project_detail_lines(simulation, selected);
    if (lines.empty()) {
        return;
    }

    auto width = 0;
    auto height = 0;
    SDL_GetRendererOutputSize(renderer, &width, &height);

    const auto map_left = build_menu_width + 18;
    const auto map_right = width - inspector_width - 18;
    const auto available_width = map_right - map_left;
    if (available_width < 360 || height < hud_height + 240) {
        return;
    }

    const auto popup_width = std::min(480, available_width - 24);
    const auto line_height = 22;
    const auto popup_height = 28 + static_cast<int>(lines.size()) * line_height + 18;
    const auto popup = SDL_Rect{
        .x = map_left + (available_width - popup_width) / 2,
        .y = hud_height + 20,
        .w = popup_width,
        .h = popup_height
    };

    set_color(renderer, Color{18, 22, 22, 238});
    SDL_RenderFillRect(renderer, &popup);
    set_color(renderer, Color{92, 104, 100, 255});
    SDL_RenderDrawRect(renderer, &popup);

    const auto text = Color{220, 224, 210, 255};
    const auto muted = Color{156, 166, 160, 255};
    auto y = popup.y + 14;
    const auto max_chars = static_cast<std::size_t>(
        std::max(1, (popup.w - 28) / 12));
    for (std::size_t index = 0; index < lines.size(); ++index) {
        draw_text(
            renderer,
            popup.x + 14,
            y,
            truncate_text(lines[index], max_chars),
            index == 0 ? text : muted,
            2);
        y += line_height;
    }
}

std::string selected_summary(const Simulation& simulation, std::optional<BuildingId> selected)
{
    if (!selected.has_value()) {
        return "none";
    }

    const auto& building = simulation.building(*selected);
    auto output = std::ostringstream{};
    output << "#" << building.id << " " << simulation.definition(building.kind).name
           << " block=" << blocking_reason_text(building.blocking_reason);

    const auto bread = building.inventory.quantity(ResourceId::Bread);
    const auto grain = building.inventory.quantity(ResourceId::Grain);
    const auto timber = building.inventory.quantity(ResourceId::Timber);
    if (bread > 0) {
        output << " bread=" << bread;
    }
    if (grain > 0) {
        output << " grain=" << grain;
    }
    if (timber > 0) {
        output << " timber=" << timber;
    }

    return output.str();
}

InspectorScrollMetrics draw_inspector(SDL_Renderer* renderer,
    const Simulation& simulation,
    const VillageObjectiveTracker& objectives,
    std::optional<BuildingId> selected,
    int requested_scroll)
{
    int width = 0;
    int height = 0;
    SDL_GetRendererOutputSize(renderer, &width, &height);

    const auto panel_x = width - inspector_width;
    auto panel = SDL_Rect{
        .x = panel_x,
        .y = hud_height,
        .w = inspector_width,
        .h = height - hud_height
    };
    set_color(renderer, Color{20, 23, 23, 255});
    SDL_RenderFillRect(renderer, &panel);

    set_color(renderer, Color{60, 64, 64, 255});
    SDL_RenderDrawRect(renderer, &panel);

    const auto text = Color{210, 214, 204, 255};
    const auto muted = Color{126, 136, 134, 255};
    draw_text(renderer, panel_x + 18, hud_height + 18, "INSPECTOR", text, 2);

    auto content_rect = SDL_Rect{
        .x = panel_x + 1,
        .y = hud_height + 50,
        .w = inspector_width - 2,
        .h = std::max(0, height - hud_height - 51)
    };
    const auto content_x = panel_x + 18;
    const auto unclamped_scroll = std::max(0, requested_scroll);

    const auto clip_was_enabled = SDL_RenderIsClipEnabled(renderer) == SDL_TRUE;
    auto previous_clip = SDL_Rect{};
    SDL_RenderGetClipRect(renderer, &previous_clip);
    SDL_RenderSetClipRect(renderer, &content_rect);

    auto content_start = content_rect.y - unclamped_scroll;
    auto content_end = draw_inspector_content(
        renderer,
        simulation,
        objectives,
        selected,
        content_x,
        content_start,
        text,
        muted);
    const auto content_height = std::max(0, content_end - content_start);
    const auto max_scroll = std::max(0, content_height - content_rect.h);
    const auto scroll_offset = std::clamp(unclamped_scroll, 0, max_scroll);

    if (scroll_offset != unclamped_scroll) {
        set_color(renderer, Color{20, 23, 23, 255});
        SDL_RenderFillRect(renderer, &content_rect);
        content_start = content_rect.y - scroll_offset;
        draw_inspector_content(
            renderer,
            simulation,
            objectives,
            selected,
            content_x,
            content_start,
            text,
            muted);
    }

    SDL_RenderSetClipRect(renderer, clip_was_enabled ? &previous_clip : nullptr);
    draw_scrollbar(
        renderer,
        content_rect,
        scroll_offset,
        max_scroll,
        Color{44, 48, 48, 255},
        Color{116, 126, 124, 255});
    draw_discovery_project_action_button(
        renderer,
        simulation,
        selected,
        width,
        height,
        text,
        muted);

    return InspectorScrollMetrics{
        .offset = scroll_offset,
        .max_offset = max_scroll
    };
}

}
