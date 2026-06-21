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
    bool running = false;
    int ticks_per_frame = 10;
    std::optional<BuildingId> selected;
    std::optional<GridPosition> hover_tile;
    bool path_dragging = false;
    std::optional<GridPosition> last_path_drag_tile;
    std::string status = "ready";
    Camera camera;
    bool quit = false;
};

void handle_event(GameSession& game, ClientInteractionState& state, const SDL_Event& event);

}
