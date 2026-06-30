#include "client/Palette.hpp"

namespace vibecity::client {

Color building_color(const Simulation& simulation, const BuildingInstance& building)
{
    const auto color = simulation.definition(building.kind).map_color;
    return Color{color[0], color[1], color[2], 255};
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
    case ResourceId::Stone:
        return Color{146, 148, 142, 255};
    case ResourceId::Tools:
        return Color{168, 176, 184, 255};
    case ResourceId::Count:
        return Color{210, 214, 204, 255};
    }
    return Color{210, 214, 204, 255};
}

}
