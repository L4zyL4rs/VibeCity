#include "client/Inspector.hpp"

#include "client/Hud.hpp"
#include "client/Text.hpp"

#include <algorithm>
#include <cstddef>
#include <sstream>
#include <string>
#include <string_view>

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
    case ResourceId::Tools:
        return "TOOLS";
    case ResourceId::Count:
        return "UNK";
    }
    return "UNK";
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

std::string stored_single_line(const ResourceArray& totals, ResourceId resource)
{
    auto output = std::ostringstream{};
    output << resource_display_name(resource) << ": " << totals[resource_index(resource)];
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

std::string objective_progress_line(const VillageObjectiveStatus& status)
{
    auto output = std::ostringstream{};
    output << (status.complete ? "OK " : "-- ") << status.label;
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

    const auto totals = simulation.total_inventory();
    draw_text(renderer, x, y, "STORED", text, 2);
    y += 24;
    draw_text(renderer, x, y, stored_pair_line(totals, ResourceId::Bread, ResourceId::Grain), muted, 2);
    y += 20;
    draw_text(renderer, x, y, stored_pair_line(totals, ResourceId::Timber, ResourceId::Firewood), muted, 2);
    y += 20;
    draw_text(renderer, x, y, stored_single_line(totals, ResourceId::Tools), muted, 2);
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

    auto workers = std::ostringstream{};
    workers << "WORKERS: " << building.assigned_workers;
    draw_text(renderer, x, y, workers.str(), muted, 2);
    y += 20;

    auto residents = std::ostringstream{};
    residents << "RESIDENTS: " << building.residents;
    draw_text(renderer, x, y, residents.str(), muted, 2);
    y += 28;

    const auto& instance_definition = simulation.definition(building.kind);
    const auto* operating_definition = &instance_definition;
    if (instance_definition.internal_construction_site
        && building.construction_target.has_value()) {
        operating_definition = &simulation.definition(*building.construction_target);
        auto target = std::ostringstream{};
        target << "TARGET: " << operating_definition->name;
        draw_text(renderer, x, y, target.str(), muted, 2);
        y += 20;

        auto labor = std::ostringstream{};
        labor << "LABOR: " << building.construction_labor_completed << "/" << building.construction_labor_required;
        draw_text(renderer, x, y, labor.str(), muted, 2);
        y += 20;

        auto builders = std::ostringstream{};
        builders << "BUILDERS: " << building.assigned_builders;
        draw_text(renderer, x, y, builders.str(), muted, 2);
        y += 28;
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

    return InspectorScrollMetrics{
        .offset = scroll_offset,
        .max_offset = max_scroll
    };
}

}
