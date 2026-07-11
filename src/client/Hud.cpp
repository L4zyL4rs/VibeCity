#include "client/Hud.hpp"

#include "client/MapView.hpp"
#include "client/Text.hpp"

#include <algorithm>
#include <array>
#include <sstream>
#include <string>
#include <string_view>

namespace vibecity::client {

std::string clock_text(const Simulation& simulation)
{
    const auto minute_of_day = simulation.minute_of_day();
    const auto hour = minute_of_day / ticks_per_hour;
    const auto minute = minute_of_day % ticks_per_hour;

    auto output = std::ostringstream{};
    output << "DAY: " << simulation.current_day() << " ";
    if (hour < 10) {
        output << "0";
    }
    output << hour << ":";
    if (minute < 10) {
        output << "0";
    }
    output << minute;
    return output.str();
}

void draw_hud(SDL_Renderer* renderer,
    const Simulation& simulation,
    ClientMode mode,
    std::optional<BuildingKind> build_target,
    MapLens lens,
    bool running,
    int ticks_per_frame)
{
    int width = 0;
    SDL_GetRendererOutputSize(renderer, &width, nullptr);

    auto hud_rect = SDL_Rect{0, 0, width, hud_height};
    set_color(renderer, Color{18, 20, 20, 255});
    SDL_RenderFillRect(renderer, &hud_rect);

    auto separator = SDL_Rect{0, hud_height - 2, width, 2};
    set_color(renderer, Color{60, 64, 64, 255});
    SDL_RenderFillRect(renderer, &separator);

    constexpr std::array modes{ClientMode::Select, ClientMode::PlacePath, ClientMode::Demolish};
    constexpr std::array colors{
        Color{170, 176, 176, 255},
        Color{82, 86, 86, 255},
        Color{178, 82, 70, 255}
    };
    constexpr std::array labels{
        std::string_view{"1"},
        std::string_view{"2"},
        std::string_view{"X"}
    };
    for (std::size_t index = 0; index < modes.size(); ++index) {
        auto box = SDL_Rect{
            .x = 12 + static_cast<int>(index) * 38,
            .y = 12,
            .w = 28,
            .h = 28
        };
        set_color(renderer, colors[index]);
        SDL_RenderFillRect(renderer, &box);

        set_color(renderer, modes[index] == mode ? Color{230, 230, 218, 255} : Color{44, 48, 48, 255});
        SDL_RenderDrawRect(renderer, &box);
        draw_text(
            renderer,
            box.x + 9,
            box.y + 7,
            labels[index],
            Color{18, 20, 20, 255},
            2);
    }

    auto speed_x = 138;
    auto speed_value = ticks_per_frame;
    for (int index = 0; index < 8; ++index) {
        auto bar = SDL_Rect{
            .x = speed_x + index * 8,
            .y = 30 - index * 2,
            .w = 5,
            .h = 8 + index * 2
        };
        set_color(renderer, speed_value > 0 ? Color{164, 174, 116, 255} : Color{58, 62, 62, 255});
        SDL_RenderFillRect(renderer, &bar);
        speed_value /= 2;
    }

    auto run_indicator = SDL_Rect{width - 40, 14, 24, 24};
    set_color(renderer, running ? Color{82, 170, 92, 255} : Color{190, 74, 70, 255});
    SDL_RenderFillRect(renderer, &run_indicator);

    auto mode_text = std::string{"MODE: "} + mode_name(mode);
    if (mode == ClientMode::Build && build_target.has_value()) {
        mode_text += " " + simulation.definition(*build_target).name;
    }
    mode_text += "  LENS: ";
    mode_text += map_lens_name(lens);
    if (mode_text.size() > 42) {
        mode_text.resize(42);
        mode_text.back() = '.';
    }
    draw_text(renderer, 188, 10, mode_text, Color{210, 214, 204, 255}, 2);
    draw_text(renderer,
        188,
        30,
        running ? "SPACE PAUSE  +/- SPEED  WASD PAN  L LENS" : "SPACE RUN    +/- SPEED  WASD PAN  L LENS",
        Color{128, 138, 136, 255},
        2);
    draw_text(renderer,
        700,
        30,
        clock_text(simulation) + " JOBS: " + std::to_string(simulation.transport_jobs().size()),
        Color{128, 138, 136, 255},
        2);
}

void draw_status(SDL_Renderer* renderer, std::string_view status)
{
    auto text = std::string{"STATUS: "} + std::string{status};
    if (text.size() > 46) {
        text.resize(46);
        text.back() = '.';
    }
    draw_text(renderer, 700, 10, text, Color{202, 204, 176, 255}, 2);
}

void draw_objective_completion_banner(SDL_Renderer* renderer,
    const VillageObjectiveTracker& objectives,
    int reserved_left_width,
    int reserved_right_width)
{
    if (!objectives.all_complete()) {
        return;
    }

    int width = 0;
    SDL_GetRendererOutputSize(renderer, &width, nullptr);

    const auto available_width = std::max(
        0,
        width - reserved_left_width - reserved_right_width - 32);
    if (available_width < 320) {
        return;
    }
    const auto banner_width = std::min(520, available_width);

    const auto banner = SDL_Rect{
        .x = reserved_left_width + 16,
        .y = hud_height + 16,
        .w = banner_width,
        .h = 58
    };

    set_color(renderer, Color{24, 42, 34, 230});
    SDL_RenderFillRect(renderer, &banner);
    set_color(renderer, Color{120, 176, 116, 255});
    SDL_RenderDrawRect(renderer, &banner);

    draw_text(renderer, banner.x + 14, banner.y + 10, "MILESTONE COMPLETE", Color{220, 232, 204, 255}, 2);
    draw_text(renderer, banner.x + 14, banner.y + 32, "25 FED RESIDENTS FOR 5 DAYS", Color{150, 176, 144, 255}, 2);
}

}
