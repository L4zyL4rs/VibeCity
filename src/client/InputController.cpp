#include "client/InputController.hpp"

#include "client/BuildMenu.hpp"
#include "client/Hud.hpp"
#include "client/Inspector.hpp"

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <optional>
#include <sstream>

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
    const Simulation& simulation,
    const BuildingDefinition& definition)
{
    if (const auto missing = simulation.missing_capability(definition.kind)) {
        state.mode = ClientMode::Select;
        state.build_target = std::nullopt;
        state.status = "locked: " + std::string{capability_name(*missing)};
        return;
    }
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
        const auto blocker = building_placement_blocker_text(simulation, target, tile);
        status += blocker.empty() ? "blocked" : blocker;
        status += " ";
    }
    status += definition.name;
    status += " ";
    status += construction_cost_text(construction_materials_for_footprint(
        definition,
        simulation.map(),
        tile));

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

std::string status_resource_amounts_text(const ResourceArray& amounts)
{
    auto output = std::ostringstream{};
    auto wrote_amount = false;
    for (std::size_t index = 0; index < resource_count; ++index) {
        if (amounts[index] <= 0) {
            continue;
        }
        output << (wrote_amount ? " + " : "")
               << amounts[index] << " "
               << resource_name(static_cast<ResourceId>(index));
        wrote_amount = true;
    }
    return wrote_amount ? output.str() : "materials";
}

std::string discovery_project_required_host_name(
    const Simulation& simulation,
    const DiscoveryProjectDefinition& project)
{
    const auto kind = simulation.building_catalog().find_kind(project.required_host_stable_id);
    if (!kind.has_value()) {
        return std::string{project.required_host_stable_id};
    }
    return simulation.definition(*kind).name;
}

std::string discovery_project_status_text(
    const Simulation& simulation,
    DiscoveryProjectId project_id,
    BuildingId host)
{
    const auto& project = discovery_project_definition(project_id);
    const auto status = simulation.discovery_project_start_status(project_id, host);
    switch (status.blocker) {
    case DiscoveryProjectStartBlocker::None:
        return std::string{project.name} + " ready";
    case DiscoveryProjectStartBlocker::CapabilityAlreadyDiscovered:
        return std::string{capability_name(project.grants_capability)} + " already discovered";
    case DiscoveryProjectStartBlocker::AlreadyActive:
        return std::string{project.name} + " already active";
    case DiscoveryProjectStartBlocker::MissingCapability:
        if (project.required_capability.has_value()) {
            return std::string{project.name} + " needs "
                + std::string{capability_name(*project.required_capability)};
        }
        return std::string{project.name} + " needs prerequisite";
    case DiscoveryProjectStartBlocker::InvalidHost:
        return std::string{project.name} + " needs a placed host";
    case DiscoveryProjectStartBlocker::WrongHost:
        return std::string{project.name} + " needs "
            + discovery_project_required_host_name(simulation, project);
    case DiscoveryProjectStartBlocker::MissingPathAccess:
        return std::string{project.name} + " needs road access";
    case DiscoveryProjectStartBlocker::MissingInputs:
        return std::string{project.name} + " can start; needs "
            + status_resource_amounts_text(status.missing_inputs);
    case DiscoveryProjectStartBlocker::MissingMapResource:
        return std::string{project.name} + " needs "
            + std::to_string(project.map_resource_quantity) + " "
            + std::string{map_resource_name(project.map_resource)}
            + " nearby (" + std::to_string(status.map_resource_available) + ")";
    }
    return std::string{project.name} + " blocked";
}

std::string selected_building_status(const Simulation& simulation, BuildingId building_id)
{
    const auto project = simulation.discovery_project_for_host(building_id);
    if (!project.has_value()) {
        return "building selected";
    }
    return discovery_project_status_text(
        simulation,
        *project,
        building_id);
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

std::string path_paving_hover_status(const Simulation& simulation, GridPosition tile)
{
    const auto status = simulation.path_paving_status(tile);
    if (status.blocker == PathPavingBlocker::None) {
        return "roadwork: 1 brick, "
            + std::to_string(status.labor_required / ticks_per_hour)
            + " builder-hours";
    }
    if (status.blocker == PathPavingBlocker::MissingBricks) {
        return "roadwork: needs 1 brick ("
            + std::to_string(status.connected_bricks) + " connected)";
    }
    return std::string{path_paving_blocker_text(status.blocker)};
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

void pave_path_tile(
    GameSession& game,
    ClientInteractionState& state,
    GridPosition tile,
    bool quiet_failure)
{
    const auto result = game.execute(PavePathCommand{.position = tile});
    if (result.success) {
        state.status = result.message;
        return;
    }

    if (!quiet_failure) {
        state.status = result.message;
    }
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
    case SDLK_r:
        state.mode = ClientMode::UpgradePath;
        state.build_target = std::nullopt;
        state.path_dragging = false;
        state.last_path_drag_tile = std::nullopt;
        state.status = "road upgrade mode";
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
    case SDLK_p:
        if (!state.selected.has_value()) {
            state.status = "no project host selected";
        } else {
            const auto project = game.simulation().discovery_project_for_host(*state.selected);
            if (!project.has_value()) {
                state.status = "no project available";
                break;
            }
            if (!game.simulation().can_start_discovery_project(*project, *state.selected)) {
                state.status = discovery_project_status_text(
                    game.simulation(),
                    *project,
                    *state.selected);
                break;
            }
            const auto result = game.execute(StartDiscoveryProjectCommand{
                .project = *project,
                .host = *state.selected
            });
            state.status = result.message;
        }
        break;
    case SDLK_o:
        if (!state.selected.has_value()) {
            state.status = "no building selected";
        } else {
            const auto enabled = !game.simulation().building(*state.selected).work_enabled;
            const auto result = game.execute(SetBuildingWorkEnabledCommand{
                .building = *state.selected,
                .enabled = enabled
            });
            state.status = result.message;
        }
        break;
    case SDLK_l:
    case SDLK_TAB:
        state.map_lens = next_map_lens(state.map_lens);
        state.status = "lens " + std::string{map_lens_name(state.map_lens)};
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
                select_build_target(
                    state,
                    game.simulation(),
                    game.simulation().definition(kinds[index]));
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
    } else if (state.mode == ClientMode::UpgradePath && !state.path_dragging) {
        state.status = path_paving_hover_status(game.simulation(), *state.hover_tile);
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
    if (state.path_dragging
        && (state.mode == ClientMode::PlacePath || state.mode == ClientMode::UpgradePath)
        && (motion.state & SDL_BUTTON_LMASK) != 0) {
        if (!state.last_path_drag_tile.has_value() || !(*state.last_path_drag_tile == *state.hover_tile)) {
            if (state.mode == ClientMode::PlacePath) {
                place_path_tile(game, *state.hover_tile, state.selected, state.status, true);
            } else {
                pave_path_tile(game, state, *state.hover_tile, true);
            }
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
        state.status = state.selected.has_value()
            ? selected_building_status(game.simulation(), *state.selected)
            : "no building selected";
    } else if (state.mode == ClientMode::PlacePath) {
        state.path_dragging = true;
        state.last_path_drag_tile = tile;
        place_path_tile(game, tile, state.selected, state.status, false);
    } else if (state.mode == ClientMode::UpgradePath) {
        state.path_dragging = true;
        state.last_path_drag_tile = tile;
        pave_path_tile(game, state, tile, false);
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
        select_build_target(state, game.simulation(), game.simulation().definition(*kind));
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
        const auto was_upgrading_path = state.mode == ClientMode::UpgradePath;
        state.mode = ClientMode::Select;
        state.build_target = std::nullopt;
        state.status = was_demolishing
            ? "demolition cancelled"
            : (was_upgrading_path ? "road upgrade cancelled" : "placement cancelled");
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
