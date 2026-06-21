#pragma once

#include <SDL.h>

#include <string_view>

namespace vibecity::client {

struct Color {
    Uint8 r = 0;
    Uint8 g = 0;
    Uint8 b = 0;
    Uint8 a = 255;
};

void set_color(SDL_Renderer* renderer, Color color);
void draw_text(SDL_Renderer* renderer, int x, int y, std::string_view text, Color color, int scale = 2);

}
