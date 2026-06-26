#pragma once

namespace vibecity::client {

enum class ClientMode {
    Select,
    PlacePath,
    Demolish,
    Build
};

[[nodiscard]] const char* mode_name(ClientMode mode);

}
