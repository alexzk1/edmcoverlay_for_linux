#pragma once
#include <cstdio>
#include <stdexcept>
#include <string>

// https://stackoverflow.com/questions/2342162/stdstring-formatting-like-sprintf

namespace format_helper {

template <class Src>
inline Src cast(Src v)
{
    return v;
}

inline const char *cast(const std::string &v)
{
    return v.c_str();
}
} // namespace format_helper

template <typename... Ts>
inline std::string stringfmt(const std::string &fmt, Ts &&...vs)
{
    using namespace format_helper;
    char b;

    // Not counting the terminating null character.
    size_t required = std::snprintf(&b, 0, fmt.c_str(), cast(vs)...);
    std::string result;
    result.resize(required, 0);
    std::snprintf(const_cast<char *>(result.data()), required + 1, fmt.c_str(),
                  cast(std::forward<Ts>(vs))...);

    return result;
}
