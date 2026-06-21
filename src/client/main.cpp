#include "client/ClientMode.hpp"
#include "client/Hud.hpp"
#include "client/MapView.hpp"
#include "client/Text.hpp"
#include "game/Scenario.hpp"

#include <SDL.h>

#include <algorithm>
#include <cstddef>
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
using vibecity::client::clock_text;
using vibecity::client::draw_hud;
using vibecity::client::draw_placement_preview;
using vibecity::client::draw_status;
using vibecity::client::draw_text;
using vibecity::client::draw_transport_jobs;
using vibecity::client::draw_world;
using vibecity::client::hud_height;
using vibecity::client::mode_name;
using vibecity::client::screen_to_map;
using vibecity::client::set_color;
using vibecity::client::target_kind_for_mode;
using vibecity::client::tile_size;

constexpr int initial_window_width = 1280;
constexpr int initial_window_height = 800;
constexpr int inspector_width = 340;

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

int total_assigned_workers(const vibecity::Simulation& simulation)
{
    auto workers = 0;
    for (const auto& building : simulation.buildings()) {
        workers += building.assigned_workers;
    }
    return workers;
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

std::string objective_progress_line(const vibecity::VillageObjectiveStatus& status)
{
    auto output = std::ostringstream{};
    output << (status.complete ? "OK " : "-- ") << status.label;
    if (status.target > 1) {
        output << " " << std::min(status.current, status.target) << "/" << status.target;
    }
    return output.str();
}

int draw_objective_summary(SDL_Renderer* renderer,
    const vibecity::VillageObjectiveTracker& objectives,
    int x,
    int y,
    Color text,
    Color muted)
{
    draw_text(renderer, x, y, "OBJECTIVE", text, 2);
    y += 24;

    auto completed = 0;
    const auto* active = static_cast<const vibecity::VillageObjectiveStatus*>(nullptr);
    for (const auto& status : objectives.statuses()) {
        if (status.complete) {
            ++completed;
            continue;
        }

        if (active == nullptr) {
            active = &status;
        }
    }

    if (active == nullptr) {
        draw_text(renderer, x, y, "OK VILLAGE STABLE", text, 2);
    } else {
        draw_text(renderer, x, y, objective_progress_line(*active), muted, 2);
    }
    y += 20;

    draw_text(renderer,
        x,
        y,
        std::string{"DONE: "} + std::to_string(completed)
            + "/" + std::to_string(vibecity::village_objective_count),
        muted,
        2);
    y += 28;

    return y;
}

int draw_economy_summary(SDL_Renderer* renderer,
    const vibecity::Simulation& simulation,
    const vibecity::VillageObjectiveTracker& objectives,
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
        std::string{"POP: "} + std::to_string(simulation.total_residents())
            + "/" + std::to_string(simulation.total_housing_capacity())
            + "  WORK: " + std::to_string(total_assigned_workers(simulation)),
        muted,
        2);
    y += 20;

    draw_text(renderer,
        x,
        y,
        std::string{"HOME: "} + std::to_string(simulation.free_housing_capacity())
            + "  BREAD/DAY: " + std::to_string(simulation.daily_bread_need()),
        muted,
        2);
    y += 20;

    draw_text(renderer,
        x,
        y,
        std::string{"FOOD: "} + std::to_string(simulation.stored_bread())
            + " BRD  " + std::to_string(simulation.bread_days_remaining()) + " DAYS",
        muted,
        2);
    y += 20;

    draw_text(renderer,
        x,
        y,
        std::string{"GROW: "} + std::string{vibecity::population_growth_blocker_text(simulation.population_growth_blocker())},
        muted,
        2);
    y += 20;

    const auto construction = simulation.construction_summary();
    draw_text(renderer,
        x,
        y,
        std::string{"SITE: "} + std::to_string(construction.sites)
            + " MAT:" + std::to_string(construction.waiting_materials)
            + " LAB:" + std::to_string(construction.waiting_builders),
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

    y = draw_objective_summary(renderer, objectives, x, y, text, muted);

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

void draw_inspector(SDL_Renderer* renderer,
    const vibecity::Simulation& simulation,
    const vibecity::VillageObjectiveTracker& objectives,
    std::optional<vibecity::BuildingId> selected)
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

    y = draw_economy_summary(renderer, simulation, objectives, panel_x + 18, y, text, muted);

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
