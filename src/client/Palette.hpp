#pragma once

#include "client/Text.hpp"
#include "core/Building.hpp"
#include "core/Resource.hpp"

namespace vibecity::client {

[[nodiscard]] Color building_color(const BuildingInstance& building);
[[nodiscard]] Color resource_color(ResourceId resource);

}
