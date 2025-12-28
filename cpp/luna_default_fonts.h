#pragma once

#include <linux/limits.h>
#include <lunasvg.h>
#include <unistd.h>

#include <array>
#include <cstddef>
#include <filesystem>
#include <iostream>
#include <mutex>
#include <string>
#include <system_error>
#include <unordered_set>
#include <vector>

/// @brief Tries to add font path to lunasvg once. It is up-to caller to ensure it is exists.
inline bool InstallNormalFontFileToLuna(const std::string &path)
{
    if (path.empty())
    {
        return false;
    }

    static std::mutex mut;
    const std::lock_guard grd(mut);
    static std::unordered_set<std::string> installed;
    if (installed.count(path) == 0)
    {
        if (lunasvg_add_font_face_from_file("", false, false, path.c_str()))
        {
            installed.insert(path);
#ifndef _NDEBUG
            std::cout << path << " font was loaded.\n";
#endif
            return true;
        }
        return false;
    }
    return true;
}

namespace fs = std::filesystem;

/// @returns path where this binary is located.
inline fs::path ExecutableDir()
{
    std::array<char, PATH_MAX> buf{0};
    const std::size_t len = readlink("/proc/self/exe", buf.data(), buf.size() - 1);
    if (len <= 0)
    {
        return {};
    }
    return fs::path(buf.data()).parent_path();
}

/// @returns path to font downloaded by build script.
inline const auto GetCustomDownloadedFont()
{
    return ExecutableDir() / "AppleColorEmoji.ttf";
}

/// @brief fonts are used to try to render <image> tag out of emoji (our custom renderer).
inline const std::vector<std::string> &GetEmojiFonts()
{
    static const std::vector<std::string> fonts = {
      GetCustomDownloadedFont(), "Segoe UI Emoji", "Symbols Nerd Font Mono", "Noto Color Emoji",
      "Apple Color Emoji",       "FreeMono",       "Liberation Mono"};
    return fonts;
}

/// @brief 1st font is used to enforce render of <text> tag by lunasvg, other fonts are used  to try
/// to find BMP symbols and compute their width by our custom renderer/measurer (not lunasvg).
inline const std::vector<std::string> &GetTextFonts()
{
    static const std::vector<std::string> fonts = {
      "Liberation Mono",  "Segoe UI Emoji", "FreeMono",
      "DejaVu Sans Mono", "Unifont",        "Adwaita Sans",
      "Carlito",          "Unifont Upper",  "Symbols Nerd Font Mono",
      "Noto Color Emoji",
    };
    return fonts;
}
