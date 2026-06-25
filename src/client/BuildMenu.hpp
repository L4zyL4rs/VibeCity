#pragma once

#include "core/Simulation.hpp"

#include <SDL.h>

#include <optional>
#include <string>
#include <vector>

namespace vibecity::client {

inline constexpr int build_menu_width = 292;
inline constexpr int build_menu_scroll_step = 84;

struct BuildMenuMetrics {
    int offset = 0;
    int max_offset = 0;
};

[[nodiscard]] std::vector<BuildingKind> build_menu_kinds(const BuildingCatalog& catalog);
[[nodiscard]] std::string construction_cost_text(const BuildingDefinition& definition);
[[nodiscard]] std::optional<BuildingKind> build_menu_kind_at(
    const BuildingCatalog& catalog,
    int screen_y,
    int viewport_height,
    int scroll);

[[nodiscard]] BuildMenuMetrics draw_build_menu(
    SDL_Renderer* renderer,
    const Simulation& simulation,
    std::optional<BuildingKind> selected,
    int requested_scroll);

}
