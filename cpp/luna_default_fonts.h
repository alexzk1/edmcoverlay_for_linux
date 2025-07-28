#pragma once

#include <lunasvg.h>

#include <mutex>
#include <string>
#include <unordered_set>

/// @brief Tries to add font path to lunasvg once. It is up-to caller to ensure it is exists.
inline void InstallNormalFontFileToLuna(const std::string &path)
{
    if (path.empty())
    {
        return;
    }

    static std::mutex mut;
    const std::lock_guard grd(mut);
    static std::unordered_set<std::string> installed;
    if (installed.count(path) == 0)
    {
        installed.insert(path);
        lunasvg_add_font_face_from_file("", false, false, path.c_str());
    }
}
