#include "game/Scenario.hpp"

#include <SDL.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>

namespace {

constexpr int initial_window_width = 1280;
constexpr int initial_window_height = 800;
constexpr int tile_size = 10;
constexpr int hud_height = 52;
constexpr int inspector_width = 340;

enum class ClientMode {
    Select,
    PlacePath,
    BuildHouse,
    BuildFarm,
    BuildWoodcutter,
    BuildBakery,
    BuildStorehouse
};

struct Camera {
    int offset_x = 80;
    int offset_y = hud_height + 24;
};

struct ScreenPoint {
    float x = 0.0F;
    float y = 0.0F;
};

struct Color {
    Uint8 r = 0;
    Uint8 g = 0;
    Uint8 b = 0;
    Uint8 a = 255;
};

void set_color(SDL_Renderer* renderer, Color color)
{
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
}

std::array<Uint8, 7> glyph_rows(char character)
{
    if (character >= 'a' && character <= 'z') {
        character = static_cast<char>(character - 'a' + 'A');
    }

    switch (character) {
    case 'A':
        return {0b01110, 0b10001, 0b10001, 0b11111, 0b10001, 0b10001, 0b10001};
    case 'B':
        return {0b11110, 0b10001, 0b10001, 0b11110, 0b10001, 0b10001, 0b11110};
    case 'C':
        return {0b01111, 0b10000, 0b10000, 0b10000, 0b10000, 0b10000, 0b01111};
    case 'D':
        return {0b11110, 0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b11110};
    case 'E':
        return {0b11111, 0b10000, 0b10000, 0b11110, 0b10000, 0b10000, 0b11111};
    case 'F':
        return {0b11111, 0b10000, 0b10000, 0b11110, 0b10000, 0b10000, 0b10000};
    case 'G':
        return {0b01111, 0b10000, 0b10000, 0b10011, 0b10001, 0b10001, 0b01110};
    case 'H':
        return {0b10001, 0b10001, 0b10001, 0b11111, 0b10001, 0b10001, 0b10001};
    case 'I':
        return {0b11111, 0b00100, 0b00100, 0b00100, 0b00100, 0b00100, 0b11111};
    case 'J':
        return {0b00111, 0b00010, 0b00010, 0b00010, 0b10010, 0b10010, 0b01100};
    case 'K':
        return {0b10001, 0b10010, 0b10100, 0b11000, 0b10100, 0b10010, 0b10001};
    case 'L':
        return {0b10000, 0b10000, 0b10000, 0b10000, 0b10000, 0b10000, 0b11111};
    case 'M':
        return {0b10001, 0b11011, 0b10101, 0b10101, 0b10001, 0b10001, 0b10001};
    case 'N':
        return {0b10001, 0b11001, 0b10101, 0b10011, 0b10001, 0b10001, 0b10001};
    case 'O':
        return {0b01110, 0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b01110};
    case 'P':
        return {0b11110, 0b10001, 0b10001, 0b11110, 0b10000, 0b10000, 0b10000};
    case 'Q':
        return {0b01110, 0b10001, 0b10001, 0b10001, 0b10101, 0b10010, 0b01101};
    case 'R':
        return {0b11110, 0b10001, 0b10001, 0b11110, 0b10100, 0b10010, 0b10001};
    case 'S':
        return {0b01111, 0b10000, 0b10000, 0b01110, 0b00001, 0b00001, 0b11110};
    case 'T':
        return {0b11111, 0b00100, 0b00100, 0b00100, 0b00100, 0b00100, 0b00100};
    case 'U':
        return {0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b01110};
    case 'V':
        return {0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b01010, 0b00100};
    case 'W':
        return {0b10001, 0b10001, 0b10001, 0b10101, 0b10101, 0b11011, 0b10001};
    case 'X':
        return {0b10001, 0b10001, 0b01010, 0b00100, 0b01010, 0b10001, 0b10001};
    case 'Y':
        return {0b10001, 0b10001, 0b01010, 0b00100, 0b00100, 0b00100, 0b00100};
    case 'Z':
        return {0b11111, 0b00001, 0b00010, 0b00100, 0b01000, 0b10000, 0b11111};
    case '0':
        return {0b01110, 0b10001, 0b10011, 0b10101, 0b11001, 0b10001, 0b01110};
    case '1':
        return {0b00100, 0b01100, 0b00100, 0b00100, 0b00100, 0b00100, 0b01110};
    case '2':
        return {0b01110, 0b10001, 0b00001, 0b00010, 0b00100, 0b01000, 0b11111};
    case '3':
        return {0b11110, 0b00001, 0b00001, 0b01110, 0b00001, 0b00001, 0b11110};
    case '4':
        return {0b00010, 0b00110, 0b01010, 0b10010, 0b11111, 0b00010, 0b00010};
    case '5':
        return {0b11111, 0b10000, 0b10000, 0b11110, 0b00001, 0b00001, 0b11110};
    case '6':
        return {0b01110, 0b10000, 0b10000, 0b11110, 0b10001, 0b10001, 0b01110};
    case '7':
        return {0b11111, 0b00001, 0b00010, 0b00100, 0b01000, 0b01000, 0b01000};
    case '8':
        return {0b01110, 0b10001, 0b10001, 0b01110, 0b10001, 0b10001, 0b01110};
    case '9':
        return {0b01110, 0b10001, 0b10001, 0b01111, 0b00001, 0b00001, 0b01110};
    case '#':
        return {0b01010, 0b11111, 0b01010, 0b01010, 0b11111, 0b01010, 0b00000};
    case ':':
        return {0b00000, 0b00100, 0b00100, 0b00000, 0b00100, 0b00100, 0b00000};
    case '-':
        return {0b00000, 0b00000, 0b00000, 0b11111, 0b00000, 0b00000, 0b00000};
    case '+':
        return {0b00000, 0b00100, 0b00100, 0b11111, 0b00100, 0b00100, 0b00000};
    case '=':
        return {0b00000, 0b00000, 0b11111, 0b00000, 0b11111, 0b00000, 0b00000};
    case '/':
        return {0b00001, 0b00001, 0b00010, 0b00100, 0b01000, 0b10000, 0b10000};
    case '.':
        return {0b00000, 0b00000, 0b00000, 0b00000, 0b00000, 0b01100, 0b01100};
    case ' ':
        return {0b00000, 0b00000, 0b00000, 0b00000, 0b00000, 0b00000, 0b00000};
    default:
        return {0b01110, 0b10001, 0b00001, 0b00010, 0b00100, 0b00000, 0b00100};
    }
}

void draw_text(SDL_Renderer* renderer, int x, int y, std::string_view text, Color color, int scale = 2)
{
    set_color(renderer, color);
    auto cursor_x = x;

    for (const auto character : text) {
        const auto rows = glyph_rows(character);
        for (std::size_t row = 0; row < rows.size(); ++row) {
            for (int col = 0; col < 5; ++col) {
                const auto bit = 1 << (4 - col);
                if ((rows[row] & bit) == 0) {
                    continue;
                }

                auto pixel = SDL_Rect{
                    .x = cursor_x + col * scale,
                    .y = y + static_cast<int>(row) * scale,
                    .w = scale,
                    .h = scale
                };
                SDL_RenderFillRect(renderer, &pixel);
            }
        }
        cursor_x += 6 * scale;
    }
}

const char* mode_name(ClientMode mode)
{
    switch (mode) {
    case ClientMode::Select:
        return "select";
    case ClientMode::PlacePath:
        return "path";
    case ClientMode::BuildHouse:
        return "build house";
    case ClientMode::BuildFarm:
        return "build farm";
    case ClientMode::BuildWoodcutter:
        return "build woodcutter";
    case ClientMode::BuildBakery:
        return "build bakery";
    case ClientMode::BuildStorehouse:
        return "build storehouse";
    }
    return "unknown";
}

std::optional<vibecity::BuildingKind> target_kind_for_mode(ClientMode mode)
{
    switch (mode) {
    case ClientMode::BuildHouse:
        return vibecity::BuildingKind::House;
    case ClientMode::BuildFarm:
        return vibecity::BuildingKind::Farm;
    case ClientMode::BuildWoodcutter:
        return vibecity::BuildingKind::Woodcutter;
    case ClientMode::BuildBakery:
        return vibecity::BuildingKind::Bakery;
    case ClientMode::BuildStorehouse:
        return vibecity::BuildingKind::Storehouse;
    case ClientMode::Select:
    case ClientMode::PlacePath:
        return std::nullopt;
    }
    return std::nullopt;
}

vibecity::GridPosition screen_to_map(int screen_x, int screen_y, Camera camera)
{
    return vibecity::GridPosition{
        .x = (screen_x - camera.offset_x) / tile_size,
        .y = (screen_y - camera.offset_y) / tile_size
    };
}

SDL_Rect tile_rect(vibecity::GridPosition position, vibecity::Footprint footprint, Camera camera)
{
    return SDL_Rect{
        .x = camera.offset_x + position.x * tile_size,
        .y = camera.offset_y + position.y * tile_size,
        .w = footprint.width * tile_size,
        .h = footprint.height * tile_size
    };
}

bool contains(vibecity::GridPosition position, vibecity::Footprint footprint, vibecity::GridPosition point)
{
    return point.x >= position.x && point.y >= position.y
        && point.x < position.x + footprint.width
        && point.y < position.y + footprint.height;
}

vibecity::Footprint footprint_for(const vibecity::BuildingInstance& building)
{
    if (building.kind == vibecity::BuildingKind::ConstructionSite && building.construction_target.has_value()) {
        return vibecity::building_definition(*building.construction_target).footprint;
    }
    return vibecity::building_definition(building.kind).footprint;
}

std::optional<vibecity::BuildingId> building_at(const vibecity::Simulation& simulation, vibecity::GridPosition tile)
{
    for (const auto& building : simulation.buildings()) {
        if (!building.position.has_value()) {
            continue;
        }

        if (contains(*building.position, footprint_for(building), tile)) {
            return building.id;
        }
    }
    return std::nullopt;
}

Color building_color(const vibecity::BuildingInstance& building)
{
    if (building.kind == vibecity::BuildingKind::ConstructionSite) {
        return Color{178, 140, 64, 255};
    }

    switch (building.kind) {
    case vibecity::BuildingKind::House:
        return Color{92, 142, 210, 255};
    case vibecity::BuildingKind::Farm:
        return Color{78, 156, 86, 255};
    case vibecity::BuildingKind::Bakery:
        return Color{196, 126, 54, 255};
    case vibecity::BuildingKind::Woodcutter:
        return Color{93, 128, 62, 255};
    case vibecity::BuildingKind::Storehouse:
        return Color{132, 118, 151, 255};
    case vibecity::BuildingKind::ConstructionSite:
    case vibecity::BuildingKind::Count:
        return Color{160, 160, 160, 255};
    }
    return Color{160, 160, 160, 255};
}

Color resource_color(vibecity::ResourceId resource)
{
    switch (resource) {
    case vibecity::ResourceId::Grain:
        return Color{198, 176, 78, 255};
    case vibecity::ResourceId::Bread:
        return Color{220, 178, 112, 255};
    case vibecity::ResourceId::Timber:
        return Color{136, 90, 48, 255};
    case vibecity::ResourceId::Firewood:
        return Color{206, 82, 48, 255};
    case vibecity::ResourceId::Tools:
        return Color{168, 176, 184, 255};
    case vibecity::ResourceId::Count:
        return Color{210, 214, 204, 255};
    }
    return Color{210, 214, 204, 255};
}

std::string_view resource_short_name(vibecity::ResourceId resource)
{
    switch (resource) {
    case vibecity::ResourceId::Grain:
        return "GRN";
    case vibecity::ResourceId::Bread:
        return "BRD";
    case vibecity::ResourceId::Timber:
        return "TIM";
    case vibecity::ResourceId::Firewood:
        return "FIR";
    case vibecity::ResourceId::Tools:
        return "TLS";
    case vibecity::ResourceId::Count:
        return "UNK";
    }
    return "UNK";
}

Color mode_color(ClientMode mode)
{
    switch (mode) {
    case ClientMode::Select:
        return Color{170, 176, 176, 255};
    case ClientMode::PlacePath:
        return Color{82, 86, 86, 255};
    case ClientMode::BuildHouse:
        return Color{92, 142, 210, 255};
    case ClientMode::BuildFarm:
        return Color{78, 156, 86, 255};
    case ClientMode::BuildWoodcutter:
        return Color{93, 128, 62, 255};
    case ClientMode::BuildBakery:
        return Color{196, 126, 54, 255};
    case ClientMode::BuildStorehouse:
        return Color{132, 118, 151, 255};
    }
    return Color{160, 160, 160, 255};
}

std::string clock_text(const vibecity::Simulation& simulation)
{
    const auto minute_of_day = simulation.minute_of_day();
    const auto hour = minute_of_day / vibecity::ticks_per_hour;
    const auto minute = minute_of_day % vibecity::ticks_per_hour;

    auto output = std::ostringstream{};
    output << "DAY: " << simulation.current_day() << " ";
    if (hour < 10) {
        output << "0";
    }
    output << hour << ":";
    if (minute < 10) {
        output << "0";
    }
    output << minute;
    return output.str();
}

int total_residents(const vibecity::Simulation& simulation)
{
    auto residents = 0;
    for (const auto& building : simulation.buildings()) {
        residents += building.residents;
    }
    return residents;
}

int total_assigned_workers(const vibecity::Simulation& simulation)
{
    auto workers = 0;
    for (const auto& building : simulation.buildings()) {
        workers += building.assigned_workers;
    }
    return workers;
}

void draw_hud(SDL_Renderer* renderer,
    const vibecity::Simulation& simulation,
    ClientMode mode,
    bool running,
    int ticks_per_frame)
{
    int width = 0;
    SDL_GetRendererOutputSize(renderer, &width, nullptr);

    auto hud_rect = SDL_Rect{0, 0, width, hud_height};
    set_color(renderer, Color{18, 20, 20, 255});
    SDL_RenderFillRect(renderer, &hud_rect);

    auto separator = SDL_Rect{0, hud_height - 2, width, 2};
    set_color(renderer, Color{60, 64, 64, 255});
    SDL_RenderFillRect(renderer, &separator);

    constexpr std::array<ClientMode, 7> modes{{
        ClientMode::Select,
        ClientMode::PlacePath,
        ClientMode::BuildFarm,
        ClientMode::BuildWoodcutter,
        ClientMode::BuildBakery,
        ClientMode::BuildHouse,
        ClientMode::BuildStorehouse
    }};

    for (std::size_t index = 0; index < modes.size(); ++index) {
        auto box = SDL_Rect{
            .x = 12 + static_cast<int>(index) * 38,
            .y = 12,
            .w = 28,
            .h = 28
        };
        set_color(renderer, mode_color(modes[index]));
        SDL_RenderFillRect(renderer, &box);

        set_color(renderer, modes[index] == mode ? Color{230, 230, 218, 255} : Color{44, 48, 48, 255});
        SDL_RenderDrawRect(renderer, &box);
    }

    auto speed_x = 304;
    auto speed_value = ticks_per_frame;
    for (int index = 0; index < 8; ++index) {
        auto bar = SDL_Rect{
            .x = speed_x + index * 8,
            .y = 30 - index * 2,
            .w = 5,
            .h = 8 + index * 2
        };
        set_color(renderer, speed_value > 0 ? Color{164, 174, 116, 255} : Color{58, 62, 62, 255});
        SDL_RenderFillRect(renderer, &bar);
        speed_value /= 2;
    }

    auto run_indicator = SDL_Rect{width - 40, 14, 24, 24};
    set_color(renderer, running ? Color{82, 170, 92, 255} : Color{190, 74, 70, 255});
    SDL_RenderFillRect(renderer, &run_indicator);

    draw_text(renderer, 390, 10, std::string{"MODE: "} + mode_name(mode), Color{210, 214, 204, 255}, 2);
    draw_text(renderer, 390, 30, running ? "SPACE PAUSE   +/- SPEED   WASD PAN" : "SPACE RUN     +/- SPEED   WASD PAN", Color{128, 138, 136, 255}, 2);
    draw_text(renderer,
        760,
        30,
        clock_text(simulation) + " JOBS: " + std::to_string(simulation.transport_jobs().size()),
        Color{128, 138, 136, 255},
        2);
}

void draw_status(SDL_Renderer* renderer, std::string_view status)
{
    draw_text(renderer, 760, 10, std::string{"STATUS: "} + std::string{status}, Color{202, 204, 176, 255}, 2);
}

std::string selected_summary(const vibecity::Simulation& simulation, std::optional<vibecity::BuildingId> selected)
{
    if (!selected.has_value()) {
        return "none";
    }

    const auto& building = simulation.building(*selected);
    auto output = std::ostringstream{};
    output << "#" << building.id << " " << vibecity::building_kind_name(building.kind)
           << " block=" << vibecity::blocking_reason_text(building.blocking_reason);

    const auto bread = building.inventory.quantity(vibecity::ResourceId::Bread);
    const auto grain = building.inventory.quantity(vibecity::ResourceId::Grain);
    const auto timber = building.inventory.quantity(vibecity::ResourceId::Timber);
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

std::string resource_line(const vibecity::BuildingInstance& building, vibecity::ResourceId resource)
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

std::string stored_pair_line(const vibecity::ResourceArray& totals,
    vibecity::ResourceId first,
    vibecity::ResourceId second)
{
    auto output = std::ostringstream{};
    output << resource_short_name(first) << ": " << totals[vibecity::resource_index(first)]
           << "  " << resource_short_name(second) << ": " << totals[vibecity::resource_index(second)];
    return output.str();
}

std::string stored_single_line(const vibecity::ResourceArray& totals, vibecity::ResourceId resource)
{
    auto output = std::ostringstream{};
    output << resource_short_name(resource) << ": " << totals[vibecity::resource_index(resource)];
    return output.str();
}

bool has_flow(const vibecity::ResourceStats& stats, vibecity::ResourceId resource)
{
    const auto index = vibecity::resource_index(resource);
    return stats.produced[index] != 0 || stats.consumed[index] != 0 || stats.transported[index] != 0;
}

std::string flow_line(const vibecity::ResourceStats& stats, vibecity::ResourceId resource)
{
    const auto index = vibecity::resource_index(resource);
    auto output = std::ostringstream{};
    output << resource_short_name(resource)
           << " P:" << stats.produced[index]
           << " C:" << stats.consumed[index]
           << " T:" << stats.transported[index];
    return output.str();
}

int draw_economy_summary(SDL_Renderer* renderer,
    const vibecity::Simulation& simulation,
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
        std::string{"POP: "} + std::to_string(total_residents(simulation))
            + "  WORK: " + std::to_string(total_assigned_workers(simulation)),
        muted,
        2);
    y += 20;

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
    y += 28;

    const auto totals = simulation.total_inventory();
    draw_text(renderer, x, y, "STORED", text, 2);
    y += 24;
    draw_text(renderer, x, y, stored_pair_line(totals, vibecity::ResourceId::Bread, vibecity::ResourceId::Grain), muted, 2);
    y += 20;
    draw_text(renderer, x, y, stored_pair_line(totals, vibecity::ResourceId::Timber, vibecity::ResourceId::Firewood), muted, 2);
    y += 20;
    draw_text(renderer, x, y, stored_single_line(totals, vibecity::ResourceId::Tools), muted, 2);
    y += 28;

    draw_text(renderer, x, y, "FLOW TOTAL", text, 2);
    y += 24;

    auto drew_flow = false;
    for (std::size_t index = 0; index < vibecity::resource_count; ++index) {
        const auto resource = static_cast<vibecity::ResourceId>(index);
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

void draw_inspector(SDL_Renderer* renderer, const vibecity::Simulation& simulation, std::optional<vibecity::BuildingId> selected)
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
    set_color(renderer, Color{20, 23, 23, 235});
    SDL_RenderFillRect(renderer, &panel);

    set_color(renderer, Color{60, 64, 64, 255});
    SDL_RenderDrawRect(renderer, &panel);

    const auto text = Color{210, 214, 204, 255};
    const auto muted = Color{126, 136, 134, 255};
    auto y = hud_height + 18;
    draw_text(renderer, panel_x + 18, y, "INSPECTOR", text, 2);
    y += 28;

    y = draw_economy_summary(renderer, simulation, panel_x + 18, y, text, muted);

    if (!selected.has_value()) {
        draw_text(renderer, panel_x + 18, y, "NO SELECTION", muted, 2);
        return;
    }

    draw_text(renderer, panel_x + 18, y, "SELECTION", text, 2);
    y += 24;

    const auto& building = simulation.building(*selected);
    auto title = std::ostringstream{};
    title << "#" << building.id << " " << building_kind_name(building.kind);
    draw_text(renderer, panel_x + 18, y, title.str(), text, 2);
    y += 24;

    auto block = std::ostringstream{};
    block << "BLOCK: " << blocking_reason_text(building.blocking_reason);
    draw_text(renderer, panel_x + 18, y, block.str(), muted, 2);
    y += 24;

    auto workers = std::ostringstream{};
    workers << "WORKERS: " << building.assigned_workers;
    draw_text(renderer, panel_x + 18, y, workers.str(), muted, 2);
    y += 20;

    auto residents = std::ostringstream{};
    residents << "RES: " << building.residents;
    draw_text(renderer, panel_x + 18, y, residents.str(), muted, 2);
    y += 28;

    if (building.kind == vibecity::BuildingKind::ConstructionSite && building.construction_target.has_value()) {
        auto target = std::ostringstream{};
        target << "TARGET: " << building_kind_name(*building.construction_target);
        draw_text(renderer, panel_x + 18, y, target.str(), muted, 2);
        y += 20;

        auto labor = std::ostringstream{};
        labor << "LABOR: " << building.construction_labor_completed << "/" << building.construction_labor_required;
        draw_text(renderer, panel_x + 18, y, labor.str(), muted, 2);
        y += 28;
    }

    draw_text(renderer, panel_x + 18, y, "INVENTORY", text, 2);
    y += 24;

    auto drew_inventory = false;
    for (std::size_t index = 0; index < vibecity::resource_count; ++index) {
        const auto resource = static_cast<vibecity::ResourceId>(index);
        const auto line = resource_line(building, resource);
        if (line.empty()) {
            continue;
        }
        draw_text(renderer, panel_x + 18, y, line, muted, 2);
        y += 20;
        drew_inventory = true;
    }

    if (!drew_inventory) {
        draw_text(renderer, panel_x + 18, y, "EMPTY", muted, 2);
    }
}

std::optional<ScreenPoint> building_center_screen(const vibecity::Simulation& simulation,
    vibecity::BuildingId building_id,
    Camera camera)
{
    for (const auto& building : simulation.buildings()) {
        if (building.id != building_id || !building.position.has_value()) {
            continue;
        }

        const auto footprint = footprint_for(building);
        return ScreenPoint{
            .x = static_cast<float>(camera.offset_x)
                + (static_cast<float>(building.position->x) + static_cast<float>(footprint.width) * 0.5F)
                    * static_cast<float>(tile_size),
            .y = static_cast<float>(camera.offset_y)
                + (static_cast<float>(building.position->y) + static_cast<float>(footprint.height) * 0.5F)
                    * static_cast<float>(tile_size)
        };
    }

    return std::nullopt;
}

void draw_transport_jobs(SDL_Renderer* renderer, const vibecity::Simulation& simulation, Camera camera)
{
    for (const auto& job : simulation.transport_jobs()) {
        const auto source = building_center_screen(simulation, job.source, camera);
        const auto destination = building_center_screen(simulation, job.destination, camera);
        if (!source.has_value() || !destination.has_value()) {
            continue;
        }

        auto color = resource_color(job.resource);
        set_color(renderer, Color{color.r, color.g, color.b, 72});
        SDL_RenderDrawLine(renderer,
            static_cast<int>(source->x),
            static_cast<int>(source->y),
            static_cast<int>(destination->x),
            static_cast<int>(destination->y));

        auto marker = *source;
        if (job.state == vibecity::TransportJobState::CarryingGoods) {
            const auto total_ticks = std::max<vibecity::Tick>(1, job.leg_ticks_total);
            const auto remaining = std::clamp(job.ticks_remaining, vibecity::Tick{0}, total_ticks);
            const auto progress = 1.0F - static_cast<float>(remaining) / static_cast<float>(total_ticks);
            marker.x = source->x + (destination->x - source->x) * progress;
            marker.y = source->y + (destination->y - source->y) * progress;
        }

        const auto marker_size = job.state == vibecity::TransportJobState::GoingToPickup ? 5 : 7;
        auto marker_rect = SDL_Rect{
            .x = static_cast<int>(marker.x) - marker_size / 2,
            .y = static_cast<int>(marker.y) - marker_size / 2,
            .w = marker_size,
            .h = marker_size
        };

        set_color(renderer, color);
        SDL_RenderFillRect(renderer, &marker_rect);
        set_color(renderer, Color{18, 20, 20, 255});
        SDL_RenderDrawRect(renderer, &marker_rect);
    }
}

std::optional<vibecity::Footprint> preview_footprint_for_mode(ClientMode mode)
{
    if (mode == ClientMode::PlacePath) {
        return vibecity::Footprint{1, 1};
    }

    if (const auto target = target_kind_for_mode(mode)) {
        return vibecity::building_definition(*target).footprint;
    }

    return std::nullopt;
}

bool can_place_preview(const vibecity::Simulation& simulation, ClientMode mode, vibecity::GridPosition tile)
{
    if (mode == ClientMode::PlacePath) {
        return simulation.map().in_bounds(tile)
            && !simulation.map().has_path(tile)
            && !building_at(simulation, tile).has_value();
    }

    if (const auto target = target_kind_for_mode(mode)) {
        return simulation.map().can_place_building(tile, vibecity::building_definition(*target).footprint);
    }

    return false;
}

void draw_placement_preview(SDL_Renderer* renderer,
    const vibecity::Simulation& simulation,
    Camera camera,
    ClientMode mode,
    std::optional<vibecity::GridPosition> hover_tile)
{
    if (!hover_tile.has_value()) {
        return;
    }

    const auto footprint = preview_footprint_for_mode(mode);
    if (!footprint.has_value()) {
        return;
    }

    const auto valid = can_place_preview(simulation, mode, *hover_tile);
    auto rect = tile_rect(*hover_tile, *footprint, camera);

    set_color(renderer, valid ? Color{96, 190, 116, 76} : Color{220, 76, 72, 76});
    SDL_RenderFillRect(renderer, &rect);
    set_color(renderer, valid ? Color{140, 236, 156, 230} : Color{255, 112, 96, 230});
    SDL_RenderDrawRect(renderer, &rect);
}

void update_title(SDL_Window* window,
    const vibecity::Simulation& simulation,
    ClientMode mode,
    bool running,
    int ticks_per_frame,
    std::optional<vibecity::BuildingId> selected)
{
    auto title = std::ostringstream{};
    title << "VibeCity"
          << " | day " << simulation.current_day()
          << " | " << (running ? "running" : "paused")
          << " | speed " << ticks_per_frame
          << " | mode " << mode_name(mode)
          << " | selected " << selected_summary(simulation, selected);
    SDL_SetWindowTitle(window, title.str().c_str());
}

void draw_map(SDL_Renderer* renderer,
    const vibecity::Simulation& simulation,
    Camera camera,
    std::optional<vibecity::BuildingId> selected,
    std::optional<vibecity::GridPosition> hover_tile,
    ClientMode mode,
    bool running,
    int ticks_per_frame,
    std::string_view status)
{
    set_color(renderer, Color{24, 28, 28, 255});
    SDL_RenderClear(renderer);

    const auto& map = simulation.map();
    set_color(renderer, Color{48, 52, 52, 255});
    for (int y = 0; y < map.height(); ++y) {
        for (int x = 0; x < map.width(); ++x) {
            const auto position = vibecity::GridPosition{x, y};
            if (!map.has_path(position)) {
                continue;
            }

            auto rect = tile_rect(position, vibecity::Footprint{1, 1}, camera);
            SDL_RenderFillRect(renderer, &rect);
        }
    }

    for (const auto& building : simulation.buildings()) {
        if (!building.position.has_value()) {
            continue;
        }

        const auto footprint = footprint_for(building);
        auto rect = tile_rect(*building.position, footprint, camera);
        set_color(renderer, building_color(building));
        SDL_RenderFillRect(renderer, &rect);

        if (building.blocking_reason != vibecity::BlockingReason::None) {
            set_color(renderer, Color{202, 76, 66, 255});
        } else {
            set_color(renderer, Color{28, 31, 31, 255});
        }
        SDL_RenderDrawRect(renderer, &rect);

        if (building.blocking_reason != vibecity::BlockingReason::None) {
            auto blocker = SDL_Rect{
                .x = rect.x + rect.w - 7,
                .y = rect.y + 1,
                .w = 6,
                .h = 6
            };
            set_color(renderer, Color{230, 74, 68, 255});
            SDL_RenderFillRect(renderer, &blocker);
            set_color(renderer, Color{18, 20, 20, 255});
            SDL_RenderDrawRect(renderer, &blocker);
        }

        if (selected.has_value() && *selected == building.id) {
            auto selected_rect = rect;
            selected_rect.x -= 2;
            selected_rect.y -= 2;
            selected_rect.w += 4;
            selected_rect.h += 4;
            set_color(renderer, Color{230, 230, 218, 255});
            SDL_RenderDrawRect(renderer, &selected_rect);
        }
    }

    draw_transport_jobs(renderer, simulation, camera);
    draw_placement_preview(renderer, simulation, camera, mode, hover_tile);
    draw_inspector(renderer, simulation, selected);
    draw_hud(renderer, simulation, mode, running, ticks_per_frame);
    draw_status(renderer, status);
    SDL_RenderPresent(renderer);
}

bool is_smoke_test(int argc, char** argv)
{
    for (int i = 1; i < argc; ++i) {
        if (std::string{argv[i]} == "--smoke-test") {
            return true;
        }
    }
    return false;
}

void place_path_tile(vibecity::GameSession& game,
    vibecity::GridPosition tile,
    std::optional<vibecity::BuildingId>& selected,
    std::string& status,
    bool quiet_failure)
{
    const auto result = game.execute(vibecity::PlacePathCommand{.position = tile});
    if (result.success) {
        status = result.message;
        return;
    }

    if (!quiet_failure) {
        selected = std::nullopt;
        status = result.message;
    }
}

}

int main(int argc, char** argv)
{
    const auto smoke_test = is_smoke_test(argc, argv);
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
        return 1;
    }

    const auto window_flags = smoke_test ? SDL_WINDOW_HIDDEN : SDL_WINDOW_SHOWN;
    auto* window = SDL_CreateWindow("VibeCity",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        initial_window_width,
        initial_window_height,
        window_flags);
    if (window == nullptr) {
        SDL_Quit();
        return 1;
    }

    auto* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (renderer == nullptr) {
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    }
    if (renderer == nullptr) {
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    auto mode = ClientMode::Select;
    auto running = false;
    auto ticks_per_frame = 10;
    auto selected = std::optional<vibecity::BuildingId>{};
    auto hover_tile = std::optional<vibecity::GridPosition>{};
    auto path_dragging = false;
    auto last_path_drag_tile = std::optional<vibecity::GridPosition>{};
    auto status = std::string{"ready"};
    auto game = vibecity::GameSession{};
    const auto scenario = vibecity::create_starting_village(game);
    selected = scenario.storehouse;
    auto camera = Camera{};
    auto quit = false;
    auto frame_count = 0;

    while (!quit) {
        auto event = SDL_Event{};
        while (SDL_PollEvent(&event) != 0) {
            if (event.type == SDL_QUIT) {
                quit = true;
            }

            if (event.type == SDL_KEYDOWN) {
                switch (event.key.keysym.sym) {
                case SDLK_ESCAPE:
                    quit = true;
                    break;
                case SDLK_SPACE:
                    running = !running;
                    status = running ? "simulation running" : "simulation paused";
                    break;
                case SDLK_1:
                    mode = ClientMode::Select;
                    status = "select mode";
                    break;
                case SDLK_2:
                    mode = ClientMode::PlacePath;
                    status = "path mode";
                    break;
                case SDLK_3:
                    mode = ClientMode::BuildFarm;
                    status = "farm construction mode";
                    break;
                case SDLK_4:
                    mode = ClientMode::BuildWoodcutter;
                    status = "woodcutter construction mode";
                    break;
                case SDLK_5:
                    mode = ClientMode::BuildBakery;
                    status = "bakery construction mode";
                    break;
                case SDLK_6:
                    mode = ClientMode::BuildHouse;
                    status = "house construction mode";
                    break;
                case SDLK_7:
                    mode = ClientMode::BuildStorehouse;
                    status = "storehouse construction mode";
                    break;
                case SDLK_EQUALS:
                case SDLK_PLUS:
                    ticks_per_frame = std::min(240, ticks_per_frame * 2);
                    break;
                case SDLK_MINUS:
                    ticks_per_frame = std::max(1, ticks_per_frame / 2);
                    break;
                case SDLK_LEFT:
                case SDLK_a:
                    camera.offset_x += tile_size * 4;
                    break;
                case SDLK_RIGHT:
                case SDLK_d:
                    camera.offset_x -= tile_size * 4;
                    break;
                case SDLK_UP:
                case SDLK_w:
                    camera.offset_y += tile_size * 4;
                    break;
                case SDLK_DOWN:
                case SDLK_s:
                    camera.offset_y -= tile_size * 4;
                    break;
                default:
                    break;
                }
            }

            if (event.type == SDL_MOUSEMOTION) {
                hover_tile = screen_to_map(event.motion.x, event.motion.y, camera);
                if (path_dragging && mode == ClientMode::PlacePath && (event.motion.state & SDL_BUTTON_LMASK) != 0) {
                    if (!last_path_drag_tile.has_value() || !(*last_path_drag_tile == *hover_tile)) {
                        place_path_tile(game, *hover_tile, selected, status, true);
                        last_path_drag_tile = hover_tile;
                    }
                }
            }

            if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
                const auto tile = screen_to_map(event.button.x, event.button.y, camera);
                hover_tile = tile;
                if (mode == ClientMode::Select) {
                    selected = building_at(game.simulation(), tile);
                    status = selected.has_value() ? "building selected" : "no building selected";
                } else if (mode == ClientMode::PlacePath) {
                    path_dragging = true;
                    last_path_drag_tile = tile;
                    place_path_tile(game, tile, selected, status, false);
                } else if (auto target = target_kind_for_mode(mode)) {
                    const auto result = game.execute(vibecity::PlaceConstructionCommand{
                        .target_kind = *target,
                        .position = tile
                    });
                    if (result.success) {
                        selected = result.building;
                    }
                    status = result.message;
                }
            }

            if (event.type == SDL_MOUSEBUTTONUP && event.button.button == SDL_BUTTON_LEFT) {
                path_dragging = false;
                last_path_drag_tile = std::nullopt;
            }
        }

        if (running || smoke_test) {
            const auto result = game.execute(vibecity::AdvanceTimeCommand{.ticks = ticks_per_frame});
            if (!result.success) {
                status = result.message;
                quit = true;
            }
        }

        draw_map(renderer, game.simulation(), camera, selected, hover_tile, mode, running, ticks_per_frame, status);
        update_title(window, game.simulation(), mode, running, ticks_per_frame, selected);

        ++frame_count;
        if (smoke_test && frame_count >= 3) {
            quit = true;
        }
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
