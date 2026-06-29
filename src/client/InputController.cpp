#include "client/InputController.hpp"

#include "client/BuildMenu.hpp"
#include "client/Hud.hpp"
#include "client/Inspector.hpp"

#include <algorithm>
#include <filesystem>

namespace vibecity::client {
namespace {

const auto default_save_path = std::filesystem::path{"vibecity-save.vcs"};

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
        && x >= build_menu_width
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

bool point_is_over_build_menu(Uint32 window_id, int x, int y)
{
    auto* window = SDL_GetWindowFromID(window_id);
    if (window == nullptr) {
        return false;
    }

    auto width = 0;
    auto height = 0;
    SDL_GetWindowSize(window, &width, &height);
    return x >= 0
        && x < std::min(width, build_menu_width)
        && y >= hud_height
        && y < height;
}

void select_build_target(
    ClientInteractionState& state,
    const BuildingDefinition& definition)
{
    state.mode = ClientMode::Build;
    state.build_target = definition.kind;
    state.status = "building " + definition.name;
}

std::string placement_hover_status(
    const Simulation& simulation,
    BuildingKind target,
    GridPosition tile)
{
    const auto& definition = simulation.definition(target);
    auto status = std::string{};
    if (!can_place_building_preview(simulation, target, tile)) {
        const auto blocker = placement_blocker_text(simulation, tile, definition.footprint);
        status += blocker.empty() ? "blocked" : blocker;
        status += " ";
    }
    status += definition.name;

    if (definition.gathering.has_value()) {
        status += " ";
        status += map_resource_name(definition.gathering->resource);
        status += ": ";
        status += std::to_string(gathering_resource_quantity_for_placement(
            simulation,
            target,
            tile));
    }

    return status;
}

std::string demolition_hover_status(const Simulation& simulation, GridPosition tile)
{
    const auto building = building_at(simulation, tile);
    if (building.has_value()) {
        return "demolish " + std::string{simulation.definition(
            simulation.building(*building).kind).name};
    }
    if (simulation.map().has_path(tile)) {
        return "remove path";
    }
    return "nothing to demolish";
}

void demolish_building(
    GameSession& game,
    ClientInteractionState& state,
    BuildingId building)
{
    const auto result = game.execute(DemolishBuildingCommand{.building = building});
    if (result.success && state.selected == building) {
        state.selected = std::nullopt;
        state.inspector_scroll = 0;
    }
    state.status = result.message;
}

void demolish_tile(GameSession& game, ClientInteractionState& state, GridPosition tile)
{
    const auto building = building_at(game.simulation(), tile);
    if (building.has_value()) {
        demolish_building(game, state, *building);
        return;
    }

    if (!game.simulation().map().has_path(tile)) {
        state.status = "nothing to demolish";
        return;
    }

    const auto result = game.execute(RemovePathCommand{.position = tile});
    state.status = result.message;
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

void handle_keydown(GameSession& game, ClientInteractionState& state, SDL_Keycode key)
{
    switch (key) {
    case SDLK_ESCAPE:
        cancel_interaction(state);
        break;
    case SDLK_SPACE:
        state.running = !state.running;
        state.status = state.running ? "simulation running" : "simulation paused";
        break;
    case SDLK_1:
        state.mode = ClientMode::Select;
        state.build_target = std::nullopt;
        state.status = "select mode";
        break;
    case SDLK_2:
        state.mode = ClientMode::PlacePath;
        state.build_target = std::nullopt;
        state.status = "path mode";
        break;
    case SDLK_x:
        state.mode = ClientMode::Demolish;
        state.build_target = std::nullopt;
        state.status = "demolish mode";
        break;
    case SDLK_DELETE:
    case SDLK_BACKSPACE:
        if (state.selected.has_value()) {
            demolish_building(game, state, *state.selected);
        } else {
            state.status = "no building selected";
        }
        break;
    case SDLK_F5: {
        const auto result = game.save_to_file(default_save_path);
        state.status = result.message;
        break;
    }
    case SDLK_F9: {
        const auto result = game.load_from_file(default_save_path);
        state.status = result.message;
        if (result.success) {
            state.running = false;
            state.selected = std::nullopt;
            state.hover_tile = std::nullopt;
            state.path_dragging = false;
            state.last_path_drag_tile = std::nullopt;
            state.inspector_scroll = 0;
            state.inspector_max_scroll = 0;
        }
        break;
    }
    case SDLK_EQUALS:
    case SDLK_PLUS:
        state.ticks_per_frame = std::min(240, state.ticks_per_frame * 2);
        break;
    case SDLK_MINUS:
        state.ticks_per_frame = std::max(1, state.ticks_per_frame / 2);
        break;
    case SDLK_LEFT:
    case SDLK_a:
        state.camera.offset_x += state.camera.tile_size * 4;
        break;
    case SDLK_RIGHT:
    case SDLK_d:
        state.camera.offset_x -= state.camera.tile_size * 4;
        break;
    case SDLK_UP:
    case SDLK_w:
        state.camera.offset_y += state.camera.tile_size * 4;
        break;
    case SDLK_DOWN:
    case SDLK_s:
        state.camera.offset_y -= state.camera.tile_size * 4;
        break;
    default:
        if (key >= SDLK_3 && key <= SDLK_9) {
            const auto kinds = build_menu_kinds(game.simulation().building_catalog());
            const auto index = static_cast<std::size_t>(key - SDLK_3);
            if (index < kinds.size()) {
                select_build_target(state, game.simulation().definition(kinds[index]));
            }
        }
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
    if (state.mode == ClientMode::Build && state.build_target.has_value()) {
        state.status = placement_hover_status(
            game.simulation(),
            *state.build_target,
            *state.hover_tile);
    } else if (state.mode == ClientMode::Demolish) {
        state.status = demolition_hover_status(game.simulation(), *state.hover_tile);
    } else if (state.mode == ClientMode::Select) {
        state.status = tile_inspection_text(game.simulation(), *state.hover_tile);
    } else if (state.mode == ClientMode::PlacePath && !state.path_dragging) {
        const auto blocker = placement_blocker_text(
            game.simulation(),
            *state.hover_tile,
            Footprint{1, 1});
        state.status = blocker.empty()
            ? std::string{"place path: "} + tile_inspection_text(game.simulation(), *state.hover_tile)
            : blocker;
    }
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
    } else if (state.mode == ClientMode::Demolish) {
        demolish_tile(game, state, tile);
    } else if (state.mode == ClientMode::Build && state.build_target.has_value()) {
        const auto result = game.execute(PlaceConstructionCommand{
            .target_kind = *state.build_target,
            .position = tile
        });
        if (result.success) {
            state.selected = result.building;
            state.inspector_scroll = 0;
        }
        state.status = result.message;
    }
}

void handle_build_menu_mouse_down(
    GameSession& game,
    ClientInteractionState& state,
    Uint32 window_id,
    int screen_y)
{
    auto* window = SDL_GetWindowFromID(window_id);
    if (window == nullptr) {
        return;
    }
    auto size = SDL_Point{};
    SDL_GetWindowSize(window, &size.x, &size.y);
    const auto kind = build_menu_kind_at(
        game.simulation().building_catalog(),
        screen_y,
        size.y,
        state.build_menu_scroll);
    if (kind.has_value()) {
        select_build_target(state, game.simulation().definition(*kind));
    }
}

void handle_mouse_wheel(ClientInteractionState& state, const SDL_MouseWheelEvent& wheel)
{
    auto mouse_x = 0;
    auto mouse_y = 0;
    SDL_GetMouseState(&mouse_x, &mouse_y);
    const auto direction = wheel.direction == SDL_MOUSEWHEEL_FLIPPED ? -wheel.y : wheel.y;
    if (point_is_over_inspector(wheel.windowID, mouse_x, mouse_y)) {
        state.inspector_scroll = std::clamp(
            state.inspector_scroll - direction * 60,
            0,
            state.inspector_max_scroll);
    } else if (point_is_over_build_menu(wheel.windowID, mouse_x, mouse_y)) {
        state.build_menu_scroll = std::clamp(
            state.build_menu_scroll - direction * build_menu_scroll_step,
            0,
            state.build_menu_max_scroll);
    } else if (point_is_over_world(wheel.windowID, mouse_x, mouse_y)) {
        zoom_camera_at(state.camera, mouse_x, mouse_y, direction);
        state.hover_tile = screen_to_map(mouse_x, mouse_y, state.camera);
        state.status = "zoom " + std::to_string(state.camera.tile_size) + " px";
    }
}

}

void cancel_interaction(ClientInteractionState& state)
{
    state.path_dragging = false;
    state.last_path_drag_tile = std::nullopt;
    if (state.mode != ClientMode::Select || state.build_target.has_value()) {
        const auto was_demolishing = state.mode == ClientMode::Demolish;
        state.mode = ClientMode::Select;
        state.build_target = std::nullopt;
        state.status = was_demolishing ? "demolition cancelled" : "placement cancelled";
    } else if (state.selected.has_value()) {
        state.selected = std::nullopt;
        state.inspector_scroll = 0;
        state.status = "selection cleared";
    } else {
        state.status = "nothing to cancel";
    }
}

void handle_event(GameSession& game, ClientInteractionState& state, const SDL_Event& event)
{
    if (event.type == SDL_QUIT) {
        state.quit = true;
    }

    if (event.type == SDL_KEYDOWN) {
        handle_keydown(game, state, event.key.keysym.sym);
    }

    if (event.type == SDL_MOUSEMOTION) {
        handle_mouse_motion(game, state, event.motion);
    }

    if (event.type == SDL_MOUSEBUTTONDOWN
        && event.button.button == SDL_BUTTON_LEFT
        && point_is_over_world(event.button.windowID, event.button.x, event.button.y)) {
        handle_left_mouse_down(game, state, event.button.x, event.button.y);
    }

    if (event.type == SDL_MOUSEBUTTONDOWN
        && event.button.button == SDL_BUTTON_LEFT
        && point_is_over_build_menu(event.button.windowID, event.button.x, event.button.y)) {
        handle_build_menu_mouse_down(
            game,
            state,
            event.button.windowID,
            event.button.y);
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
