#pragma once

#include "client/ClientMode.hpp"
#include "client/MapView.hpp"
#include "game/GameSession.hpp"

#include <SDL.h>

#include <optional>
#include <string>

namespace vibecity::client {

struct ClientInteractionState {
    ClientMode mode = ClientMode::Select;
    std::optional<BuildingKind> build_target;
    bool running = false;
    int ticks_per_frame = 2;
    std::optional<BuildingId> selected;
    std::optional<GridPosition> hover_tile;
    bool path_dragging = false;
    std::optional<GridPosition> last_path_drag_tile;
    int inspector_scroll = 0;
    int inspector_max_scroll = 0;
    int build_menu_scroll = 0;
    int build_menu_max_scroll = 0;
    std::string status = "ready";
    Camera camera;
    MapLens map_lens = MapLens::Default;
    bool quit = false;
};

void cancel_interaction(ClientInteractionState& state);
void handle_event(GameSession& game, ClientInteractionState& state, const SDL_Event& event);

}
