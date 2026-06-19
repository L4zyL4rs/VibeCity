#include "game/Scenario.hpp"

#include <SDL.h>

#include <algorithm>
#include <optional>
#include <sstream>
#include <string>

namespace {

constexpr int initial_window_width = 1280;
constexpr int initial_window_height = 800;
constexpr int tile_size = 10;

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
    int offset_x = 24;
    int offset_y = 24;
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
    std::optional<vibecity::BuildingId> selected)
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

        draw_map(renderer, game.simulation(), camera, selected);
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
