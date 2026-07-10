#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace vibecity {

using Quantity = std::int64_t;
using Tick = std::int64_t;

enum class ResourceId : std::uint8_t {
    Grain,
    Bread,
    Timber,
    Firewood,
    Stone,
    Tools,
    Bricks,
    Count
};

inline constexpr auto resource_count = static_cast<std::size_t>(ResourceId::Count);

using ResourceArray = std::array<Quantity, resource_count>;

struct ResourceAmount {
    ResourceId resource;
    Quantity quantity;
};

constexpr std::size_t resource_index(ResourceId resource)
{
    return static_cast<std::size_t>(resource);
}

constexpr std::array<std::string_view, resource_count> resource_names{
    "grain",
    "bread",
    "timber",
    "firewood",
    "stone",
    "tools",
    "bricks"
};

constexpr std::string_view resource_name(ResourceId resource)
{
    return resource_names[resource_index(resource)];
}

[[nodiscard]] std::optional<ResourceId> resource_id_from_string(std::string_view id);

constexpr ResourceArray empty_resources()
{
    return ResourceArray{};
}

}
