#include "client/ClientMode.hpp"
#include "client/Hud.hpp"
#include "client/Inspector.hpp"
#include "client/MapView.hpp"
#include "client/Text.hpp"
#include "game/Scenario.hpp"

#include <SDL.h>

#include <algorithm>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>

namespace {

using vibecity::client::Color;
using vibecity::client::Camera;
using vibecity::client::ClientMode;
using vibecity::client::building_at;
using vibecity::client::can_place_building_preview;
using vibecity::client::can_place_path_preview;
using vibecity::client::draw_hud;
using vibecity::client::draw_inspector;
using vibecity::client::draw_placement_preview;
using vibecity::client::draw_status;
using vibecity::client::draw_transport_jobs;
using vibecity::client::draw_world;
using vibecity::client::mode_name;
using vibecity::client::screen_to_map;
using vibecity::client::selected_summary;
using vibecity::client::set_color;
using vibecity::client::target_kind_for_mode;
using vibecity::client::tile_size;

constexpr int initial_window_width = 1280;
constexpr int initial_window_height = 800;

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
        return can_place_path_preview(simulation, tile);
    }

    if (const auto target = target_kind_for_mode(mode)) {
        return can_place_building_preview(simulation, *target, tile);
    }

    return false;
}

void draw_mode_placement_preview(SDL_Renderer* renderer,
    const vibecity::Simulation& simulation,
    Camera camera,
    ClientMode mode,
    std::optional<vibecity::GridPosition> hover_tile)
{
    const auto footprint = preview_footprint_for_mode(mode);
    const auto valid = hover_tile.has_value() && can_place_preview(simulation, mode, *hover_tile);
    draw_placement_preview(renderer, camera, hover_tile, footprint, valid);
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
    const vibecity::VillageObjectiveTracker& objectives,
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

    draw_world(renderer, simulation, camera, selected);
    draw_transport_jobs(renderer, simulation, camera);
    draw_mode_placement_preview(renderer, simulation, camera, mode, hover_tile);
    draw_inspector(renderer, simulation, objectives, selected);
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

        draw_map(renderer, game.simulation(), game.objectives(), camera, selected, hover_tile, mode, running, ticks_per_frame, status);
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
