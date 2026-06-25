#include "client/BuildMenu.hpp"

#include "client/Hud.hpp"
#include "client/Text.hpp"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <vector>

namespace vibecity::client {
namespace {

constexpr int panel_padding = 12;
constexpr int panel_header_height = 42;
constexpr int entry_height = 96;
constexpr int entry_gap = 6;
constexpr int entry_stride = entry_height + entry_gap;

std::string uppercase(std::string_view value)
{
    auto result = std::string{value};
    std::transform(result.begin(), result.end(), result.begin(), [](unsigned char character) {
        return static_cast<char>(std::toupper(character));
    });
    return result;
}

std::string fit_text(std::string value, std::size_t maximum_characters)
{
    if (value.size() <= maximum_characters) {
        return value;
    }
    value.resize(maximum_characters);
    value.back() = '.';
    return value;
}

std::vector<std::string> construction_cost_lines(const BuildingDefinition& definition)
{
    constexpr auto maximum_characters = std::size_t{40};
    auto lines = std::vector<std::string>{};
    auto line = std::string{"NEEDS"};
    auto has_material = false;

    for (std::size_t index = 0; index < resource_count; ++index) {
        const auto amount = definition.construction_materials[index];
        if (amount <= 0) {
            continue;
        }
        const auto item = std::to_string(amount) + " "
            + uppercase(resource_name(static_cast<ResourceId>(index)));
        const auto separator = has_material ? " + " : " ";
        if (line.size() + std::string_view{separator}.size() + item.size() > maximum_characters
            && line != "NEEDS") {
            lines.push_back(line);
            line = "      " + item;
        } else {
            line += separator + item;
        }
        has_material = true;
    }

    if (!has_material) {
        line += " LABOR ONLY";
    }
    lines.push_back(fit_text(line, maximum_characters));
    if (lines.size() > 2) {
        lines.resize(2);
        lines.back() = fit_text(lines.back(), maximum_characters);
    }
    return lines;
}

std::string details_text(const BuildingDefinition& definition)
{
    auto output = std::ostringstream{};
    output << "SIZE " << definition.footprint.width << "X" << definition.footprint.height;
    if (definition.worker_slots > 0) {
        output << "  JOBS " << definition.worker_slots;
    }
    if (definition.resident_capacity > 0) {
        output << "  HOMES " << definition.resident_capacity;
    }
    if (definition.construction_labor_minutes % ticks_per_hour == 0) {
        output << "  LABOR "
               << definition.construction_labor_minutes / ticks_per_hour
               << "H";
    } else {
        output << "  LABOR " << definition.construction_labor_minutes << "M";
    }
    return output.str();
}

int menu_content_height(const BuildingCatalog& catalog)
{
    return static_cast<int>(build_menu_kinds(catalog).size()) * entry_stride;
}

int menu_viewport_height(int viewport_height)
{
    return std::max(0, viewport_height - hud_height - panel_header_height - panel_padding);
}

Color definition_color(const BuildingDefinition& definition)
{
    return Color{
        definition.map_color[0],
        definition.map_color[1],
        definition.map_color[2],
        255
    };
}

}

std::vector<BuildingKind> build_menu_kinds(const BuildingCatalog& catalog)
{
    auto kinds = std::vector<BuildingKind>{};
    kinds.reserve(catalog.definitions().size());
    for (const auto& definition : catalog.definitions()) {
        if (!definition.internal_construction_site) {
            kinds.push_back(definition.kind);
        }
    }
    return kinds;
}

std::string construction_cost_text(const BuildingDefinition& definition)
{
    auto output = std::ostringstream{};
    output << "NEEDS";
    auto has_material = false;
    for (std::size_t index = 0; index < resource_count; ++index) {
        const auto amount = definition.construction_materials[index];
        if (amount <= 0) {
            continue;
        }
        output << (has_material ? " + " : " ")
               << amount << " "
               << uppercase(resource_name(static_cast<ResourceId>(index)));
        has_material = true;
    }
    if (!has_material) {
        output << " LABOR ONLY";
    }
    return output.str();
}

std::string operation_summary_text(const BuildingDefinition& definition)
{
    if (definition.recipe.has_value()) {
        auto output = std::ostringstream{};
        auto wrote_input = false;
        if (definition.gathering.has_value()) {
            output << "HARVESTS "
                   << uppercase(map_resource_name(definition.gathering->resource));
            wrote_input = true;
        }
        for (std::size_t index = 0; index < resource_count; ++index) {
            const auto amount = definition.recipe->inputs[index];
            if (amount <= 0) {
                continue;
            }
            output << (wrote_input ? " + " : "")
                   << amount << " "
                   << uppercase(resource_name(static_cast<ResourceId>(index)));
            wrote_input = true;
        }
        if (!wrote_input) {
            output << "PRODUCES ";
            auto wrote_output = false;
            for (std::size_t index = 0; index < resource_count; ++index) {
                const auto amount = definition.recipe->outputs[index];
                if (amount <= 0) {
                    continue;
                }
                output << (wrote_output ? " + " : "")
                       << amount << " "
                       << uppercase(resource_name(static_cast<ResourceId>(index)));
                wrote_output = true;
            }
            output << " / ";
            if (definition.recipe->cycle_minutes % ticks_per_hour == 0) {
                output << definition.recipe->cycle_minutes / ticks_per_hour << "H";
            } else {
                output << definition.recipe->cycle_minutes << "M";
            }
            return output.str();
        }
        output << " -> ";
        auto wrote_output = false;
        for (std::size_t index = 0; index < resource_count; ++index) {
            const auto amount = definition.recipe->outputs[index];
            if (amount <= 0) {
                continue;
            }
            output << (wrote_output ? " + " : "")
                   << amount << " "
                   << uppercase(resource_name(static_cast<ResourceId>(index)));
            wrote_output = true;
        }
        output << " / ";
        if (definition.recipe->cycle_minutes % ticks_per_hour == 0) {
            output << definition.recipe->cycle_minutes / ticks_per_hour << "H";
        } else {
            output << definition.recipe->cycle_minutes << "M";
        }
        return output.str();
    }
    if (definition.consumes_bread) {
        return "USES 1 BREAD / RESIDENT / DAY";
    }
    if (definition.requests_storage_inputs) {
        return "COLLECTS AND REDISTRIBUTES GOODS";
    }
    return "NO PRODUCTION";
}

std::optional<BuildingKind> build_menu_kind_at(
    const BuildingCatalog& catalog,
    int screen_y,
    int viewport_height,
    int scroll)
{
    const auto local_y = screen_y - hud_height - panel_header_height + scroll;
    if (local_y < 0 || screen_y >= viewport_height - panel_padding) {
        return std::nullopt;
    }
    const auto entry_index = local_y / entry_stride;
    if (local_y % entry_stride >= entry_height) {
        return std::nullopt;
    }
    const auto kinds = build_menu_kinds(catalog);
    if (entry_index < 0 || static_cast<std::size_t>(entry_index) >= kinds.size()) {
        return std::nullopt;
    }
    return kinds[static_cast<std::size_t>(entry_index)];
}

BuildMenuMetrics draw_build_menu(
    SDL_Renderer* renderer,
    const Simulation& simulation,
    std::optional<BuildingKind> selected,
    int requested_scroll)
{
    auto viewport_height = 0;
    SDL_GetRendererOutputSize(renderer, nullptr, &viewport_height);

    const auto visible_height = menu_viewport_height(viewport_height);
    const auto content_height = menu_content_height(simulation.building_catalog());
    const auto max_scroll = std::max(0, content_height - visible_height);
    const auto scroll = std::clamp(requested_scroll, 0, max_scroll);

    const auto panel = SDL_Rect{
        .x = 0,
        .y = hud_height,
        .w = build_menu_width,
        .h = std::max(0, viewport_height - hud_height)
    };
    set_color(renderer, Color{20, 23, 23, 248});
    SDL_RenderFillRect(renderer, &panel);

    auto separator = SDL_Rect{
        .x = build_menu_width - 2,
        .y = hud_height,
        .w = 2,
        .h = panel.h
    };
    set_color(renderer, Color{62, 66, 64, 255});
    SDL_RenderFillRect(renderer, &separator);

    draw_text(
        renderer,
        panel_padding,
        hud_height + 12,
        "CONSTRUCTION",
        Color{218, 220, 206, 255},
        2);

    const auto clip = SDL_Rect{
        .x = panel_padding,
        .y = hud_height + panel_header_height,
        .w = build_menu_width - panel_padding * 2 - 4,
        .h = visible_height
    };
    SDL_RenderSetClipRect(renderer, &clip);

    const auto kinds = build_menu_kinds(simulation.building_catalog());
    for (std::size_t index = 0; index < kinds.size(); ++index) {
        const auto& definition = simulation.definition(kinds[index]);
        const auto y = clip.y + static_cast<int>(index) * entry_stride - scroll;
        const auto entry = SDL_Rect{
            .x = panel_padding,
            .y = y,
            .w = clip.w - 6,
            .h = entry_height
        };
        if (entry.y + entry.h < clip.y || entry.y > clip.y + clip.h) {
            continue;
        }

        set_color(renderer, selected == definition.kind
                ? Color{48, 54, 50, 255}
                : Color{29, 33, 32, 255});
        SDL_RenderFillRect(renderer, &entry);
        set_color(renderer, selected == definition.kind
                ? Color{220, 222, 204, 255}
                : Color{70, 76, 72, 255});
        SDL_RenderDrawRect(renderer, &entry);

        auto swatch = SDL_Rect{
            .x = entry.x + 8,
            .y = entry.y + 9,
            .w = 18,
            .h = 18
        };
        set_color(renderer, definition_color(definition));
        SDL_RenderFillRect(renderer, &swatch);

        const auto shortcut = index < 7
            ? std::to_string(index + 3) + " "
            : std::string{};
        auto display_name = shortcut + uppercase(definition.name);
        const auto name_scale = display_name.size() <= 18 ? 2 : 1;
        display_name = fit_text(display_name, name_scale == 2 ? 18 : 37);
        draw_text(
            renderer,
            entry.x + 34,
            entry.y + (name_scale == 2 ? 10 : 14),
            display_name,
            Color{214, 218, 204, 255},
            name_scale);
        const auto cost_lines = construction_cost_lines(definition);
        for (std::size_t line_index = 0; line_index < cost_lines.size(); ++line_index) {
            draw_text(
                renderer,
                entry.x + 8,
                entry.y + 36 + static_cast<int>(line_index) * 13,
                cost_lines[line_index],
                Color{202, 186, 126, 255},
                1);
        }
        draw_text(
            renderer,
            entry.x + 8,
            entry.y + 62,
            fit_text(operation_summary_text(definition), 40),
            Color{160, 178, 154, 255},
            1);
        draw_text(
            renderer,
            entry.x + 8,
            entry.y + 80,
            fit_text(details_text(definition), 40),
            Color{132, 144, 138, 255},
            1);
    }

    SDL_RenderSetClipRect(renderer, nullptr);

    if (max_scroll > 0 && visible_height > 0) {
        const auto track = SDL_Rect{
            .x = build_menu_width - 8,
            .y = clip.y,
            .w = 4,
            .h = visible_height
        };
        const auto thumb_height = std::max(24, visible_height * visible_height / content_height);
        const auto thumb_y = track.y
            + (track.h - thumb_height) * scroll / max_scroll;
        const auto thumb = SDL_Rect{
            .x = track.x,
            .y = thumb_y,
            .w = track.w,
            .h = thumb_height
        };
        set_color(renderer, Color{48, 52, 50, 255});
        SDL_RenderFillRect(renderer, &track);
        set_color(renderer, Color{126, 134, 126, 255});
        SDL_RenderFillRect(renderer, &thumb);
    }

    return BuildMenuMetrics{
        .offset = scroll,
        .max_offset = max_scroll
    };
}

}
