#include "client/ClientMode.hpp"
#include "client/Hud.hpp"
#include "client/Inspector.hpp"
#include "client/InputController.hpp"
#include "client/MapView.hpp"
#include "client/Text.hpp"
#include "game/Scenario.hpp"

#include <SDL.h>

#include <optional>
#include <sstream>
#include <string>
#include <string_view>

namespace {

using vibecity::client::Color;
using vibecity::client::Camera;
using vibecity::client::ClientMode;
using vibecity::client::can_place_building_preview;
using vibecity::client::can_place_path_preview;
using vibecity::client::ClientInteractionState;
using vibecity::client::draw_hud;
using vibecity::client::draw_inspector;
using vibecity::client::draw_placement_preview;
using vibecity::client::draw_status;
using vibecity::client::draw_transport_jobs;
using vibecity::client::draw_world;
using vibecity::client::handle_event;
using vibecity::client::mode_name;
using vibecity::client::selected_summary;
using vibecity::client::set_color;
using vibecity::client::target_kind_for_mode;

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

    auto state = ClientInteractionState{};
    auto game = vibecity::GameSession{};
    const auto scenario = vibecity::create_starting_village(game);
    state.selected = scenario.storehouse;
    auto frame_count = 0;

    while (!state.quit) {
        auto event = SDL_Event{};
        while (SDL_PollEvent(&event) != 0) {
            handle_event(game, state, event);
        }

        if (state.running || smoke_test) {
            const auto result = game.execute(vibecity::AdvanceTimeCommand{.ticks = state.ticks_per_frame});
            if (!result.success) {
                state.status = result.message;
                state.quit = true;
            }
        }

        draw_map(renderer,
            game.simulation(),
            game.objectives(),
            state.camera,
            state.selected,
            state.hover_tile,
            state.mode,
            state.running,
            state.ticks_per_frame,
            state.status);
        update_title(window, game.simulation(), state.mode, state.running, state.ticks_per_frame, state.selected);

        ++frame_count;
        if (smoke_test && frame_count >= 3) {
            state.quit = true;
        }
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
