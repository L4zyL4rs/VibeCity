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

void draw_hud(SDL_Renderer* renderer, ClientMode mode, bool running, int ticks_per_frame)
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

std::string resource_line(const vibecity::BuildingInstance& building, vibecity::ResourceId resource, std::string_view label)
{
    const auto quantity = building.inventory.quantity(resource);
    if (quantity <= 0) {
        return {};
    }

    auto output = std::ostringstream{};
    output << label << ": " << quantity;
    return output.str();
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

    if (!selected.has_value()) {
        draw_text(renderer, panel_x + 18, y, "NO SELECTION", muted, 2);
        return;
    }

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

    for (const auto& line : {
             resource_line(building, vibecity::ResourceId::Bread, "BREAD"),
             resource_line(building, vibecity::ResourceId::Grain, "GRAIN"),
             resource_line(building, vibecity::ResourceId::Timber, "TIMBER"),
             resource_line(building, vibecity::ResourceId::Firewood, "FIREWOOD"),
             resource_line(building, vibecity::ResourceId::Tools, "TOOLS"),
         }) {
        if (line.empty()) {
            continue;
        }
        draw_text(renderer, panel_x + 18, y, line, muted, 2);
        y += 20;
    }
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
    ClientMode mode,
    bool running,
    int ticks_per_frame)
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

    draw_inspector(renderer, simulation, selected);
    draw_hud(renderer, mode, running, ticks_per_frame);
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

    auto mode = ClientMode::Select;
    auto running = false;
    auto ticks_per_frame = 10;
    auto selected = std::optional<vibecity::BuildingId>{};
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
                    break;
                case SDLK_1:
                    mode = ClientMode::Select;
                    break;
                case SDLK_2:
                    mode = ClientMode::PlacePath;
                    break;
                case SDLK_3:
                    mode = ClientMode::BuildFarm;
                    break;
                case SDLK_4:
                    mode = ClientMode::BuildWoodcutter;
                    break;
                case SDLK_5:
                    mode = ClientMode::BuildBakery;
                    break;
                case SDLK_6:
                    mode = ClientMode::BuildHouse;
                    break;
                case SDLK_7:
                    mode = ClientMode::BuildStorehouse;
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

            if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
                const auto tile = screen_to_map(event.button.x, event.button.y, camera);
                if (mode == ClientMode::Select) {
                    selected = building_at(game.simulation(), tile);
                } else if (mode == ClientMode::PlacePath) {
                    const auto result = game.execute(vibecity::PlacePathCommand{.position = tile});
                    if (!result.success) {
                        selected = std::nullopt;
                    }
                } else if (auto target = target_kind_for_mode(mode)) {
                    const auto result = game.execute(vibecity::PlaceConstructionCommand{
                        .target_kind = *target,
                        .position = tile
                    });
                    if (result.success) {
                        selected = result.building;
                    }
                }
            }
        }

        if (running || smoke_test) {
            const auto result = game.execute(vibecity::AdvanceTimeCommand{.ticks = ticks_per_frame});
            if (!result.success) {
                quit = true;
            }
        }

        draw_map(renderer, game.simulation(), camera, selected, mode, running, ticks_per_frame);
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
