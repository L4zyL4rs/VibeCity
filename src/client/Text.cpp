#include "client/Text.hpp"

#include <array>
#include <cstddef>

namespace vibecity::client {
namespace {

std::array<Uint8, 7> glyph_rows(char character)
{
    if (character >= 'a' && character <= 'z') {
        character = static_cast<char>(character - 'a' + 'A');
    }

    switch (character) {
    case 'A':
        return {0b01110, 0b10001, 0b10001, 0b11111, 0b10001, 0b10001, 0b10001};
    case 'B':
        return {0b11110, 0b10001, 0b10001, 0b11110, 0b10001, 0b10001, 0b11110};
    case 'C':
        return {0b01111, 0b10000, 0b10000, 0b10000, 0b10000, 0b10000, 0b01111};
    case 'D':
        return {0b11110, 0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b11110};
    case 'E':
        return {0b11111, 0b10000, 0b10000, 0b11110, 0b10000, 0b10000, 0b11111};
    case 'F':
        return {0b11111, 0b10000, 0b10000, 0b11110, 0b10000, 0b10000, 0b10000};
    case 'G':
        return {0b01111, 0b10000, 0b10000, 0b10011, 0b10001, 0b10001, 0b01110};
    case 'H':
        return {0b10001, 0b10001, 0b10001, 0b11111, 0b10001, 0b10001, 0b10001};
    case 'I':
        return {0b11111, 0b00100, 0b00100, 0b00100, 0b00100, 0b00100, 0b11111};
    case 'J':
        return {0b00111, 0b00010, 0b00010, 0b00010, 0b10010, 0b10010, 0b01100};
    case 'K':
        return {0b10001, 0b10010, 0b10100, 0b11000, 0b10100, 0b10010, 0b10001};
    case 'L':
        return {0b10000, 0b10000, 0b10000, 0b10000, 0b10000, 0b10000, 0b11111};
    case 'M':
        return {0b10001, 0b11011, 0b10101, 0b10101, 0b10001, 0b10001, 0b10001};
    case 'N':
        return {0b10001, 0b11001, 0b10101, 0b10011, 0b10001, 0b10001, 0b10001};
    case 'O':
        return {0b01110, 0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b01110};
    case 'P':
        return {0b11110, 0b10001, 0b10001, 0b11110, 0b10000, 0b10000, 0b10000};
    case 'Q':
        return {0b01110, 0b10001, 0b10001, 0b10001, 0b10101, 0b10010, 0b01101};
    case 'R':
        return {0b11110, 0b10001, 0b10001, 0b11110, 0b10100, 0b10010, 0b10001};
    case 'S':
        return {0b01111, 0b10000, 0b10000, 0b01110, 0b00001, 0b00001, 0b11110};
    case 'T':
        return {0b11111, 0b00100, 0b00100, 0b00100, 0b00100, 0b00100, 0b00100};
    case 'U':
        return {0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b01110};
    case 'V':
        return {0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b01010, 0b00100};
    case 'W':
        return {0b10001, 0b10001, 0b10001, 0b10101, 0b10101, 0b11011, 0b10001};
    case 'X':
        return {0b10001, 0b10001, 0b01010, 0b00100, 0b01010, 0b10001, 0b10001};
    case 'Y':
        return {0b10001, 0b10001, 0b01010, 0b00100, 0b00100, 0b00100, 0b00100};
    case 'Z':
        return {0b11111, 0b00001, 0b00010, 0b00100, 0b01000, 0b10000, 0b11111};
    case '0':
        return {0b01110, 0b10001, 0b10011, 0b10101, 0b11001, 0b10001, 0b01110};
    case '1':
        return {0b00100, 0b01100, 0b00100, 0b00100, 0b00100, 0b00100, 0b01110};
    case '2':
        return {0b01110, 0b10001, 0b00001, 0b00010, 0b00100, 0b01000, 0b11111};
    case '3':
        return {0b11110, 0b00001, 0b00001, 0b01110, 0b00001, 0b00001, 0b11110};
    case '4':
        return {0b00010, 0b00110, 0b01010, 0b10010, 0b11111, 0b00010, 0b00010};
    case '5':
        return {0b11111, 0b10000, 0b10000, 0b11110, 0b00001, 0b00001, 0b11110};
    case '6':
        return {0b01110, 0b10000, 0b10000, 0b11110, 0b10001, 0b10001, 0b01110};
    case '7':
        return {0b11111, 0b00001, 0b00010, 0b00100, 0b01000, 0b01000, 0b01000};
    case '8':
        return {0b01110, 0b10001, 0b10001, 0b01110, 0b10001, 0b10001, 0b01110};
    case '9':
        return {0b01110, 0b10001, 0b10001, 0b01111, 0b00001, 0b00001, 0b01110};
    case '#':
        return {0b01010, 0b11111, 0b01010, 0b01010, 0b11111, 0b01010, 0b00000};
    case ':':
        return {0b00000, 0b00100, 0b00100, 0b00000, 0b00100, 0b00100, 0b00000};
    case '-':
        return {0b00000, 0b00000, 0b00000, 0b11111, 0b00000, 0b00000, 0b00000};
    case '+':
        return {0b00000, 0b00100, 0b00100, 0b11111, 0b00100, 0b00100, 0b00000};
    case '=':
        return {0b00000, 0b00000, 0b11111, 0b00000, 0b11111, 0b00000, 0b00000};
    case '/':
        return {0b00001, 0b00001, 0b00010, 0b00100, 0b01000, 0b10000, 0b10000};
    case '.':
        return {0b00000, 0b00000, 0b00000, 0b00000, 0b00000, 0b01100, 0b01100};
    case ' ':
        return {0b00000, 0b00000, 0b00000, 0b00000, 0b00000, 0b00000, 0b00000};
    default:
        return {0b01110, 0b10001, 0b00001, 0b00010, 0b00100, 0b00000, 0b00100};
    }
}

}

void set_color(SDL_Renderer* renderer, Color color)
{
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
}

void draw_text(SDL_Renderer* renderer, int x, int y, std::string_view text, Color color, int scale)
{
    set_color(renderer, color);
    auto cursor_x = x;

    for (const auto character : text) {
        const auto rows = glyph_rows(character);
        for (std::size_t row = 0; row < rows.size(); ++row) {
            for (int col = 0; col < 5; ++col) {
                const auto bit = 1 << (4 - col);
                if ((rows[row] & bit) == 0) {
                    continue;
                }

                auto pixel = SDL_Rect{
                    .x = cursor_x + col * scale,
                    .y = y + static_cast<int>(row) * scale,
                    .w = scale,
                    .h = scale
                };
                SDL_RenderFillRect(renderer, &pixel);
            }
        }
        cursor_x += 6 * scale;
    }
}

}
