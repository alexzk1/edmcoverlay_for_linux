#pragma once

#include <lunasvg.h>

#include <mutex>
#include <string>
#include <unordered_set>

/// Adds more default fonts to lunasvg.

///@brief Installs default fonts by paths which are absent in default lunasvg load.
/// Thos
inline void InstallMoreDefaultFontsToLuna()
{
    static std::once_flag once;
    std::call_once(once, []() {
        using namespace lunasvg;
        static const struct // NOLINT
        {
            const char *filename;
            const bool bold;
            const bool italic;
        } entries[] = {
          {"/usr/share/fonts/TTF/DejaVuSans.ttf", false, false},
          {"/usr/share/fonts/TTF/DejaVuSans-Bold.ttf", true, false},
          {"/usr/share/fonts/TTF/DejaVuSans-Oblique.ttf", false, true},
          {"/usr/share/fonts/TTF/DejaVuSans-BoldOblique.ttf", true, true},
        };
        for (const auto &entry : entries)
        {
            lunasvg_add_font_face_from_file("", entry.bold, entry.italic, entry.filename);
        }
    });
}

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
