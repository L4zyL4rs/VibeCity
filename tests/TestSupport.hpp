#pragma once

#include <cstdlib>
#include <iostream>

namespace vibecity::test {

inline void check(bool condition, const char* expression, const char* file, int line)
{
    if (condition) {
        return;
    }

    std::cerr << file << ":" << line << ": check failed: " << expression << "\n";
    std::abort();
}

}

#define VIBECITY_CHECK(expression) \
    ::vibecity::test::check(static_cast<bool>(expression), #expression, __FILE__, __LINE__)
