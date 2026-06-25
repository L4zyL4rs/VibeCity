#pragma once

namespace vibecity::client {

enum class ClientMode {
    Select,
    PlacePath,
    Build
};

[[nodiscard]] const char* mode_name(ClientMode mode);

}
