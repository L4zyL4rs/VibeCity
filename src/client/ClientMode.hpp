#pragma once

#include "client/Text.hpp"
#include "core/Building.hpp"

#include <array>
#include <optional>

namespace vibecity::client {

enum class ClientMode {
    Select,
    PlacePath,
    BuildHouse,
    BuildFarm,
    BuildWoodcutter,
    BuildBakery,
    BuildStorehouse
};

inline constexpr std::array<ClientMode, 7> client_modes{{
    ClientMode::Select,
    ClientMode::PlacePath,
    ClientMode::BuildFarm,
    ClientMode::BuildWoodcutter,
    ClientMode::BuildBakery,
    ClientMode::BuildHouse,
    ClientMode::BuildStorehouse
}};

[[nodiscard]] const char* mode_name(ClientMode mode);
[[nodiscard]] std::optional<BuildingKind> target_kind_for_mode(ClientMode mode);
[[nodiscard]] Color mode_color(ClientMode mode);

}
