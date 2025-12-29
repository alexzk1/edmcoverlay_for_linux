#pragma once

#include "font_path_or_family.hpp"

#include <linux/limits.h>
#include <lunasvg.h>
#include <unistd.h>

#include <array>
#include <cstddef>
#include <filesystem>
#include <iostream>
#include <mutex>
#include <string>
#include <unordered_set>
#include <variant>
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
#ifndef NDEBUG
            std::cout << path << " font was loaded.\n";
#endif
            return true;
        }
        return false;
    }
    return true;
}

/// @returns path where this binary is located.
inline std::filesystem::path ExecutableDir()
{
    std::array<char, PATH_MAX> buf{0};
    const std::size_t len = readlink("/proc/self/exe", buf.data(), buf.size() - 1);
    if (len <= 0)
    {
        return {};
    }
    return std::filesystem::path(buf.data()).parent_path();
}

/// @returns path to font downloaded by build script.
inline const std::filesystem::path &GetCustomDownloadedFont()
{
    static const std::filesystem::path font_path = ExecutableDir() / "AppleColorEmoji.ttf";
    return font_path;
}

/// @brief fonts are used to try to render <image> tag out of emoji (our custom renderer).
inline const std::vector<FontPathOrFamily> &GetEmojiFonts()
{
    static const std::vector<FontPathOrFamily> fonts = {
      GetCustomDownloadedFont(),
      std::string{"Segoe UI Emoji"},
      std::string{"Symbols Nerd Font Mono"},
      std::string{"Apple Color Emoji"},
      std::string{"FreeMono"},
      std::string{"Liberation Mono"},
    };
    return fonts;
}

/// @brief fonts are used to measure and draw <text> tags.
/// @note Use of non-mono space can make visual wrong aligment with emojies.
inline const std::vector<FontPathOrFamily> &GetTextFonts()
{
    static const std::vector<FontPathOrFamily> fonts = {
      std::string{"Liberation Mono"}, std::string{"DejaVu Sans Mono"},
      std::string{"Unifont"},         std::string{"Symbols Nerd Font Mono"},
      std::string{"FreeMono"},
    };
    return fonts;
}
