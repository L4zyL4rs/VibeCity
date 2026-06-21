#pragma once

#include "client/ClientMode.hpp"
#include "core/Simulation.hpp"

#include <SDL.h>

#include <string>
#include <string_view>

namespace vibecity::client {

inline constexpr int hud_height = 52;

[[nodiscard]] std::string clock_text(const Simulation& simulation);

void draw_hud(SDL_Renderer* renderer,
    const Simulation& simulation,
    ClientMode mode,
    bool running,
    int ticks_per_frame);

void draw_status(SDL_Renderer* renderer, std::string_view status);

}
