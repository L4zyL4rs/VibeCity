#include "client/InputController.hpp"

#include "client/Hud.hpp"
#include "client/Inspector.hpp"

#include <algorithm>

namespace vibecity::client {
namespace {

bool point_is_over_world(Uint32 window_id, int x, int y)
{
    auto* window = SDL_GetWindowFromID(window_id);
    if (window == nullptr) {
        return false;
    }

    auto width = 0;
    auto height = 0;
    SDL_GetWindowSize(window, &width, &height);
    return x >= 0
        && y >= hud_height
        && x < std::max(0, width - inspector_width)
        && y < height;
}

bool point_is_over_inspector(Uint32 window_id, int x, int y)
{
    auto* window = SDL_GetWindowFromID(window_id);
    if (window == nullptr) {
        return false;
    }

    auto width = 0;
    auto height = 0;
    SDL_GetWindowSize(window, &width, &height);
    return x >= std::max(0, width - inspector_width)
        && x < width
        && y >= hud_height
        && y < height;
}

void place_path_tile(GameSession& game,
    GridPosition tile,
    std::optional<BuildingId>& selected,
    std::string& status,
    bool quiet_failure)
{
    const auto result = game.execute(PlacePathCommand{.position = tile});
    if (result.success) {
        status = result.message;
        return;
    }

    if (!quiet_failure) {
        selected = std::nullopt;
        status = result.message;
    }
}

void handle_keydown(ClientInteractionState& state, SDL_Keycode key)
{
    switch (key) {
    case SDLK_ESCAPE:
        state.quit = true;
        break;
    case SDLK_SPACE:
        state.running = !state.running;
        state.status = state.running ? "simulation running" : "simulation paused";
        break;
    case SDLK_1:
        state.mode = ClientMode::Select;
        state.status = "select mode";
        break;
    case SDLK_2:
        state.mode = ClientMode::PlacePath;
        state.status = "path mode";
        break;
    case SDLK_3:
        state.mode = ClientMode::BuildFarm;
        state.status = "farm construction mode";
        break;
    case SDLK_4:
        state.mode = ClientMode::BuildWoodcutter;
        state.status = "woodcutter construction mode";
        break;
    case SDLK_5:
        state.mode = ClientMode::BuildBakery;
        state.status = "bakery construction mode";
        break;
    case SDLK_6:
        state.mode = ClientMode::BuildHouse;
        state.status = "house construction mode";
        break;
    case SDLK_7:
        state.mode = ClientMode::BuildStorehouse;
        state.status = "storehouse construction mode";
        break;
    case SDLK_EQUALS:
    case SDLK_PLUS:
        state.ticks_per_frame = std::min(240, state.ticks_per_frame * 2);
        break;
    case SDLK_MINUS:
        state.ticks_per_frame = std::max(1, state.ticks_per_frame / 2);
        break;
    case SDLK_LEFT:
    case SDLK_a:
        state.camera.offset_x += tile_size * 4;
        break;
    case SDLK_RIGHT:
    case SDLK_d:
        state.camera.offset_x -= tile_size * 4;
        break;
    case SDLK_UP:
    case SDLK_w:
        state.camera.offset_y += tile_size * 4;
        break;
    case SDLK_DOWN:
    case SDLK_s:
        state.camera.offset_y -= tile_size * 4;
        break;
    default:
        break;
    }
}

void handle_mouse_motion(GameSession& game, ClientInteractionState& state, const SDL_MouseMotionEvent& motion)
{
    if (!point_is_over_world(motion.windowID, motion.x, motion.y)) {
        state.hover_tile = std::nullopt;
        return;
    }

    state.hover_tile = screen_to_map(motion.x, motion.y, state.camera);
    if (state.path_dragging && state.mode == ClientMode::PlacePath && (motion.state & SDL_BUTTON_LMASK) != 0) {
        if (!state.last_path_drag_tile.has_value() || !(*state.last_path_drag_tile == *state.hover_tile)) {
            place_path_tile(game, *state.hover_tile, state.selected, state.status, true);
            state.last_path_drag_tile = state.hover_tile;
        }
    }
}

void handle_left_mouse_down(GameSession& game, ClientInteractionState& state, int screen_x, int screen_y)
{
    const auto tile = screen_to_map(screen_x, screen_y, state.camera);
    state.hover_tile = tile;
    if (state.mode == ClientMode::Select) {
        state.selected = building_at(game.simulation(), tile);
        state.inspector_scroll = 0;
        state.status = state.selected.has_value() ? "building selected" : "no building selected";
    } else if (state.mode == ClientMode::PlacePath) {
        state.path_dragging = true;
        state.last_path_drag_tile = tile;
        place_path_tile(game, tile, state.selected, state.status, false);
    } else if (auto target = target_kind_for_mode(state.mode)) {
        const auto result = game.execute(PlaceConstructionCommand{
            .target_kind = *target,
            .position = tile
        });
        if (result.success) {
            state.selected = result.building;
            state.inspector_scroll = 0;
        }
        state.status = result.message;
    }
}

void handle_mouse_wheel(ClientInteractionState& state, const SDL_MouseWheelEvent& wheel)
{
    auto mouse_x = 0;
    auto mouse_y = 0;
    SDL_GetMouseState(&mouse_x, &mouse_y);
    if (!point_is_over_inspector(wheel.windowID, mouse_x, mouse_y)) {
        return;
    }

    const auto direction = wheel.direction == SDL_MOUSEWHEEL_FLIPPED ? -wheel.y : wheel.y;
    state.inspector_scroll = std::clamp(
        state.inspector_scroll - direction * 60,
        0,
        state.inspector_max_scroll);
}

}

void handle_event(GameSession& game, ClientInteractionState& state, const SDL_Event& event)
{
    if (event.type == SDL_QUIT) {
        state.quit = true;
    }

    if (event.type == SDL_KEYDOWN) {
        handle_keydown(state, event.key.keysym.sym);
    }

    if (event.type == SDL_MOUSEMOTION) {
        handle_mouse_motion(game, state, event.motion);
    }

    if (event.type == SDL_MOUSEBUTTONDOWN
        && event.button.button == SDL_BUTTON_LEFT
        && point_is_over_world(event.button.windowID, event.button.x, event.button.y)) {
        handle_left_mouse_down(game, state, event.button.x, event.button.y);
    }

    if (event.type == SDL_MOUSEBUTTONUP && event.button.button == SDL_BUTTON_LEFT) {
        state.path_dragging = false;
        state.last_path_drag_tile = std::nullopt;
    }

    if (event.type == SDL_MOUSEWHEEL) {
        handle_mouse_wheel(state, event.wheel);
    }
}

}
