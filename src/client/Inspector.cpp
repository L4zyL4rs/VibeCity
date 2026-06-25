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

std::string_view resource_short_name(ResourceId resource)
{
    switch (resource) {
    case ResourceId::Grain:
        return "GRN";
    case ResourceId::Bread:
        return "BRD";
    case ResourceId::Timber:
        return "TIM";
    case ResourceId::Firewood:
        return "FIR";
    case ResourceId::Tools:
        return "TLS";
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
    output << resource_short_name(resource) << ": " << quantity << "/" << capacity;
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
    output << resource_short_name(first) << ": " << totals[resource_index(first)]
           << "  " << resource_short_name(second) << ": " << totals[resource_index(second)];
    return output.str();
}

std::string stored_single_line(const ResourceArray& totals, ResourceId resource)
{
    auto output = std::ostringstream{};
    output << resource_short_name(resource) << ": " << totals[resource_index(resource)];
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
    output << resource_short_name(resource)
           << " P:" << stats.produced[index]
           << " C:" << stats.consumed[index]
           << " T:" << stats.transported[index];
    return output.str();
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

    draw_text(renderer,
        x,
        y,
        std::string{"POP: "} + std::to_string(simulation.total_residents())
            + "/" + std::to_string(simulation.total_housing_capacity())
            + "  WORK: " + std::to_string(total_assigned_workers(simulation)),
        muted,
        2);
    y += 20;

    draw_text(renderer,
        x,
        y,
        std::string{"HOME: "} + std::to_string(simulation.free_housing_capacity())
            + "  BREAD/DAY: " + std::to_string(simulation.daily_bread_need()),
        muted,
        2);
    y += 20;

    draw_text(renderer,
        x,
        y,
        std::string{"FOOD: "} + std::to_string(simulation.stored_bread())
            + " BRD  " + std::to_string(simulation.bread_days_remaining()) + " DAYS",
        muted,
        2);
    y += 20;

    draw_text(renderer,
        x,
        y,
        std::string{"GROW: "} + std::string{population_growth_blocker_text(simulation.population_growth_blocker())},
        muted,
        2);
    y += 20;

    const auto construction = simulation.construction_summary();
    draw_text(renderer,
        x,
        y,
        std::string{"SITE: "} + std::to_string(construction.sites)
            + " MAT:" + std::to_string(construction.waiting_materials)
            + " LOG:" + std::to_string(construction.waiting_logistics)
            + " LAB:" + std::to_string(construction.waiting_builders),
        muted,
        2);
    y += 20;

    if (construction.next_site.has_value() && construction.next_target.has_value()) {
        draw_text(renderer,
            x,
            y,
            std::string{"NEXT: #"} + std::to_string(*construction.next_site)
                + " " + simulation.definition(*construction.next_target).name,
            muted,
            2);
        y += 20;

        draw_text(renderer,
            x,
            y,
            std::string{"LABOR: "} + std::to_string(construction.next_labor_remaining)
                + "  BLDR: " + std::to_string(construction.active_builders),
            muted,
            2);
        y += 20;
    }

    draw_text(renderer,
        x,
        y,
        std::string{"IDLE: "} + std::to_string(simulation.idle_workers())
            + "  HAUL: " + std::to_string(simulation.available_haulers()),
        muted,
        2);
    y += 20;

    draw_text(renderer,
        x,
        y,
        std::string{"BLD: "} + std::to_string(simulation.buildings().size())
            + "  JOBS: " + std::to_string(simulation.transport_jobs().size()),
        muted,
        2);
    y += 20;

    const auto logistics = simulation.logistics_summary();
    draw_text(renderer,
        x,
        y,
        std::string{"MOVE: "} + std::to_string(logistics.active_jobs)
            + " P:" + std::to_string(logistics.going_to_pickup)
            + " C:" + std::to_string(logistics.carrying_goods),
        muted,
        2);
    y += 20;

    draw_text(renderer,
        x,
        y,
        std::string{"RSV: IN:"} + std::to_string(resource_total(logistics.reserved_incoming))
            + " OUT:" + std::to_string(resource_total(logistics.reserved_outgoing)),
        muted,
        2);
    y += 20;

    draw_text(renderer,
        x,
        y,
        std::string{"LOAD: "} + std::to_string(resource_total(logistics.in_transit)),
        muted,
        2);
    y += 28;

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

    draw_text(renderer, x, y, "FLOW TOTAL", text, 2);
    y += 24;

    auto drew_flow = false;
    for (std::size_t index = 0; index < resource_count; ++index) {
        const auto resource = static_cast<ResourceId>(index);
        if (!has_flow(simulation.stats(), resource)) {
            continue;
        }
        draw_text(renderer, x, y, flow_line(simulation.stats(), resource), muted, 2);
        y += 20;
        drew_flow = true;
    }

    if (!drew_flow) {
        draw_text(renderer, x, y, "NO FLOW YET", muted, 2);
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
    residents << "RES: " << building.residents;
    draw_text(renderer, x, y, residents.str(), muted, 2);
    y += 28;

    if (simulation.definition(building.kind).internal_construction_site
        && building.construction_target.has_value()) {
        auto target = std::ostringstream{};
        target << "TARGET: " << simulation.definition(*building.construction_target).name;
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
