#include "client/ClientMode.hpp"

namespace vibecity::client {

const char* mode_name(ClientMode mode)
{
    switch (mode) {
    case ClientMode::Select:
        return "select";
    case ClientMode::PlacePath:
        return "path";
    case ClientMode::BuildHouse:
        return "build house";
    case ClientMode::BuildFarm:
        return "build farm";
    case ClientMode::BuildWoodcutter:
        return "build woodcutter";
    case ClientMode::BuildBakery:
        return "build bakery";
    case ClientMode::BuildStorehouse:
        return "build storehouse";
    }
    return "unknown";
}

std::optional<BuildingKind> target_kind_for_mode(ClientMode mode)
{
    switch (mode) {
    case ClientMode::BuildHouse:
        return BuildingKind::House;
    case ClientMode::BuildFarm:
        return BuildingKind::Farm;
    case ClientMode::BuildWoodcutter:
        return BuildingKind::Woodcutter;
    case ClientMode::BuildBakery:
        return BuildingKind::Bakery;
    case ClientMode::BuildStorehouse:
        return BuildingKind::Storehouse;
    case ClientMode::Select:
    case ClientMode::PlacePath:
        return std::nullopt;
    }
    return std::nullopt;
}

Color mode_color(ClientMode mode)
{
    switch (mode) {
    case ClientMode::Select:
        return Color{170, 176, 176, 255};
    case ClientMode::PlacePath:
        return Color{82, 86, 86, 255};
    case ClientMode::BuildHouse:
        return Color{92, 142, 210, 255};
    case ClientMode::BuildFarm:
        return Color{78, 156, 86, 255};
    case ClientMode::BuildWoodcutter:
        return Color{93, 128, 62, 255};
    case ClientMode::BuildBakery:
        return Color{196, 126, 54, 255};
    case ClientMode::BuildStorehouse:
        return Color{132, 118, 151, 255};
    }
    return Color{160, 160, 160, 255};
}

}
