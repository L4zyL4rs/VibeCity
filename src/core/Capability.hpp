#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <string_view>

namespace vibecity {

enum class CapabilityId : std::uint8_t {
    Pottery,
    Brickmaking,
    Count
};

inline constexpr auto capability_count = static_cast<std::size_t>(CapabilityId::Count);

using CapabilityMask = std::uint64_t;

constexpr std::size_t capability_index(CapabilityId capability)
{
    return static_cast<std::size_t>(capability);
}

constexpr std::array<std::string_view, capability_count> capability_names{
    "pottery",
    "brickmaking"
};

constexpr std::string_view capability_name(CapabilityId capability)
{
    return capability_names[capability_index(capability)];
}

constexpr CapabilityMask capability_bit(CapabilityId capability)
{
    static_assert(capability_count < std::numeric_limits<CapabilityMask>::digits);
    return CapabilityMask{1} << capability_index(capability);
}

constexpr CapabilityMask all_capability_mask()
{
    static_assert(capability_count < std::numeric_limits<CapabilityMask>::digits);
    return (CapabilityMask{1} << capability_count) - 1;
}

inline std::optional<CapabilityId> capability_id_from_string(std::string_view id)
{
    for (std::size_t index = 0; index < capability_names.size(); ++index) {
        if (capability_names[index] == id) {
            return static_cast<CapabilityId>(index);
        }
    }
    return std::nullopt;
}

enum class DiscoveryProjectId : std::uint8_t {
    PotteryExperiment,
    Count
};

inline constexpr auto discovery_project_count =
    static_cast<std::size_t>(DiscoveryProjectId::Count);

constexpr std::array<std::string_view, discovery_project_count> discovery_project_names{
    "pottery_experiment"
};

constexpr std::string_view discovery_project_name(DiscoveryProjectId project)
{
    return discovery_project_names[static_cast<std::size_t>(project)];
}

inline std::optional<DiscoveryProjectId> discovery_project_id_from_string(std::string_view id)
{
    for (std::size_t index = 0; index < discovery_project_names.size(); ++index) {
        if (discovery_project_names[index] == id) {
            return static_cast<DiscoveryProjectId>(index);
        }
    }
    return std::nullopt;
}

}
