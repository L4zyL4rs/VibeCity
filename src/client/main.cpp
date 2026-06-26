#include "client/BuildMenu.hpp"
#include "client/ClientMode.hpp"
#include "client/Hud.hpp"
#include "client/Inspector.hpp"
#include "client/InputController.hpp"
#include "client/MapView.hpp"
#include "client/Text.hpp"
#include "game/Scenario.hpp"

#include <SDL.h>

#include <filesystem>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>

namespace {

using vibecity::client::Color;
using vibecity::client::Camera;
using vibecity::client::BuildMenuMetrics;
using vibecity::client::ClientMode;
using vibecity::client::can_place_building_preview;
using vibecity::client::can_place_path_preview;
using vibecity::client::ClientInteractionState;
using vibecity::client::draw_building_placement_preview;
using vibecity::client::draw_hud;
using vibecity::client::draw_build_menu;
using vibecity::client::draw_demolition_preview;
using vibecity::client::draw_inspector;
using vibecity::client::draw_objective_completion_banner;
using vibecity::client::draw_placement_preview;
using vibecity::client::draw_status;
using vibecity::client::draw_world;
using vibecity::client::gathering_resource_quantity_for_placement;
using vibecity::client::handle_event;
using vibecity::client::InspectorScrollMetrics;
using vibecity::client::mode_name;
using vibecity::client::selected_summary;
using vibecity::client::set_color;
using vibecity::client::TransportOverlay;

constexpr int initial_window_width = 1280;
constexpr int initial_window_height = 800;

std::optional<vibecity::Footprint> preview_footprint_for_mode(
    const vibecity::Simulation& simulation,
    ClientMode mode,
    std::optional<vibecity::BuildingKind> build_target)
{
    if (mode == ClientMode::PlacePath) {
        return vibecity::Footprint{1, 1};
    }

    if (mode == ClientMode::Demolish) {
        return vibecity::Footprint{1, 1};
    }

    if (mode == ClientMode::Build && build_target.has_value()) {
        return simulation.definition(*build_target).footprint;
    }

    return std::nullopt;
}

bool can_place_preview(
    const vibecity::Simulation& simulation,
    ClientMode mode,
    std::optional<vibecity::BuildingKind> build_target,
    vibecity::GridPosition tile)
{
    if (mode == ClientMode::PlacePath) {
        return can_place_path_preview(simulation, tile);
    }

    if (mode == ClientMode::Demolish) {
        return vibecity::client::building_at(simulation, tile).has_value()
            || simulation.map().has_path(tile);
    }

    if (mode == ClientMode::Build && build_target.has_value()) {
        return can_place_building_preview(simulation, *build_target, tile);
    }

    return false;
}

void draw_mode_placement_preview(SDL_Renderer* renderer,
    const vibecity::Simulation& simulation,
    Camera camera,
    ClientMode mode,
    std::optional<vibecity::BuildingKind> build_target,
    std::optional<vibecity::GridPosition> hover_tile)
{
    const auto valid = hover_tile.has_value()
        && can_place_preview(simulation, mode, build_target, *hover_tile);
    if (mode == ClientMode::Demolish) {
        draw_demolition_preview(renderer, simulation, camera, hover_tile);
        return;
    }
    if (mode == ClientMode::Build && build_target.has_value()) {
        draw_building_placement_preview(
            renderer,
            simulation,
            camera,
            hover_tile,
            *build_target,
            valid);
        return;
    }

    const auto footprint = preview_footprint_for_mode(simulation, mode, build_target);
    draw_placement_preview(renderer, camera, hover_tile, footprint, valid);
}

void update_title(SDL_Window* window,
    const vibecity::Simulation& simulation,
    ClientMode mode,
    std::optional<vibecity::BuildingKind> build_target,
    bool running,
    int ticks_per_frame,
    std::optional<vibecity::BuildingId> selected)
{
    auto title = std::ostringstream{};
    title << "VibeCity"
          << " | day " << simulation.current_day()
          << " | " << (running ? "running" : "paused")
          << " | speed " << ticks_per_frame
          << " | mode " << mode_name(mode);
    if (mode == ClientMode::Build && build_target.has_value()) {
        title << " " << simulation.definition(*build_target).name;
    }
    title
          << " | selected " << selected_summary(simulation, selected);
    SDL_SetWindowTitle(window, title.str().c_str());
}

struct ClientPanelMetrics {
    InspectorScrollMetrics inspector;
    BuildMenuMetrics build_menu;
};

ClientPanelMetrics draw_map(SDL_Renderer* renderer,
    const vibecity::Simulation& simulation,
    const vibecity::VillageObjectiveTracker& objectives,
    const TransportOverlay& transport_overlay,
    Camera camera,
    std::optional<vibecity::BuildingId> selected,
    std::optional<vibecity::GridPosition> hover_tile,
    ClientMode mode,
    std::optional<vibecity::BuildingKind> build_target,
    bool running,
    int ticks_per_frame,
    int inspector_scroll,
    int build_menu_scroll,
    std::string_view status)
{
    set_color(renderer, Color{24, 28, 28, 255});
    SDL_RenderClear(renderer);

    draw_world(renderer, simulation, camera, selected);
    transport_overlay.draw(renderer, camera);
    draw_mode_placement_preview(renderer, simulation, camera, mode, build_target, hover_tile);
    const auto build_menu_metrics = draw_build_menu(
        renderer,
        simulation,
        build_target,
        build_menu_scroll);
    const auto inspector_metrics = draw_inspector(
        renderer,
        simulation,
        objectives,
        selected,
        inspector_scroll);
    draw_hud(renderer, simulation, mode, build_target, running, ticks_per_frame);
    draw_status(renderer, status);
    draw_objective_completion_banner(
        renderer,
        objectives,
        vibecity::client::build_menu_width,
        vibecity::client::inspector_width);
    SDL_RenderPresent(renderer);
    return ClientPanelMetrics{
        .inspector = inspector_metrics,
        .build_menu = build_menu_metrics
    };
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

std::optional<std::filesystem::path> requested_screenshot_path(int argc, char** argv)
{
    for (int i = 1; i < argc; ++i) {
        if (std::string{argv[i]} == "--screenshot" && i + 1 < argc) {
            return std::filesystem::path{argv[i + 1]};
        }
    }
    return std::nullopt;
}

bool save_screenshot(SDL_Renderer* renderer, const std::filesystem::path& path)
{
    auto width = 0;
    auto height = 0;
    SDL_GetRendererOutputSize(renderer, &width, &height);
    if (width <= 0 || height <= 0) {
        return false;
    }

    auto* surface = SDL_CreateRGBSurfaceWithFormat(
        0,
        width,
        height,
        32,
        SDL_PIXELFORMAT_ARGB8888);
    if (surface == nullptr) {
        return false;
    }

    const auto read_result = SDL_RenderReadPixels(
        renderer,
        nullptr,
        SDL_PIXELFORMAT_ARGB8888,
        surface->pixels,
        surface->pitch);
    const auto save_result = read_result == 0
        ? SDL_SaveBMP(surface, path.string().c_str())
        : -1;
    SDL_FreeSurface(surface);
    return save_result == 0;
}

}

int main(int argc, char** argv)
{
    const auto screenshot_path = requested_screenshot_path(argc, argv);
    const auto smoke_test = is_smoke_test(argc, argv) || screenshot_path.has_value();
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
    auto transport_overlay = TransportOverlay{};
    state.selected = scenario.storehouse;
    if (smoke_test) {
        state.mode = ClientMode::Build;
        state.build_target = vibecity::BuildingKind::Woodcutter;
        state.hover_tile = vibecity::GridPosition{10, vibecity::starting_village_building_y};
        state.status = "Woodcutter forest: "
            + std::to_string(gathering_resource_quantity_for_placement(
                game.simulation(),
                vibecity::BuildingKind::Woodcutter,
                *state.hover_tile));
    }
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

        transport_overlay.update(game.simulation());
        const auto panel_metrics = draw_map(renderer,
            game.simulation(),
            game.objectives(),
            transport_overlay,
            state.camera,
            state.selected,
            state.hover_tile,
            state.mode,
            state.build_target,
            state.running,
            state.ticks_per_frame,
            state.inspector_scroll,
            state.build_menu_scroll,
            state.status);
        state.inspector_scroll = panel_metrics.inspector.offset;
        state.inspector_max_scroll = panel_metrics.inspector.max_offset;
        state.build_menu_scroll = panel_metrics.build_menu.offset;
        state.build_menu_max_scroll = panel_metrics.build_menu.max_offset;
        update_title(
            window,
            game.simulation(),
            state.mode,
            state.build_target,
            state.running,
            state.ticks_per_frame,
            state.selected);

        if (smoke_test && frame_count == 0) {
            state.inspector_scroll = state.inspector_max_scroll;
        }
        if (screenshot_path.has_value() && frame_count == 0) {
            if (!save_screenshot(renderer, *screenshot_path)) {
                SDL_DestroyRenderer(renderer);
                SDL_DestroyWindow(window);
                SDL_Quit();
                return 1;
            }
            state.quit = true;
        }

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
