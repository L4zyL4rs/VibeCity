#include "client/Palette.hpp"

namespace vibecity::client {

Color building_color(const BuildingInstance& building)
{
    if (building.kind == BuildingKind::ConstructionSite) {
        return Color{178, 140, 64, 255};
    }

    switch (building.kind) {
    case BuildingKind::House:
        return Color{92, 142, 210, 255};
    case BuildingKind::Farm:
        return Color{78, 156, 86, 255};
    case BuildingKind::Bakery:
        return Color{196, 126, 54, 255};
    case BuildingKind::Woodcutter:
        return Color{93, 128, 62, 255};
    case BuildingKind::Storehouse:
        return Color{132, 118, 151, 255};
    case BuildingKind::ConstructionSite:
    case BuildingKind::Count:
        return Color{160, 160, 160, 255};
    }
    return Color{160, 160, 160, 255};
}

Color resource_color(ResourceId resource)
{
    switch (resource) {
    case ResourceId::Grain:
        return Color{198, 176, 78, 255};
    case ResourceId::Bread:
        return Color{220, 178, 112, 255};
    case ResourceId::Timber:
        return Color{136, 90, 48, 255};
    case ResourceId::Firewood:
        return Color{206, 82, 48, 255};
    case ResourceId::Tools:
        return Color{168, 176, 184, 255};
    case ResourceId::Count:
        return Color{210, 214, 204, 255};
    }
    return Color{210, 214, 204, 255};
}

}
