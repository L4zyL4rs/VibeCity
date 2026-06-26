#include "client/ClientMode.hpp"

namespace vibecity::client {

const char* mode_name(ClientMode mode)
{
    switch (mode) {
    case ClientMode::Select:
        return "select";
    case ClientMode::PlacePath:
        return "path";
    case ClientMode::Demolish:
        return "demolish";
    case ClientMode::Build:
        return "build";
    }
    return "unknown";
}

}
