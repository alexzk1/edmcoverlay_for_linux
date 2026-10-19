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
#include <set>
#include <shared_mutex>
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

    static std::shared_mutex mut;
    static std::unordered_set<std::string> installed;
    {
        std::shared_lock read_lock(mut);
        if (installed.find(path) != installed.end())
        {
            return true;
        }
    }

    std::unique_lock write_lock(mut);
    if (installed.find(path) == installed.end())
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
    // File was loaded by another client while we were awaiting the lock.
    return true;
}

using FontFilesUniques = std::set<std::filesystem::path>;

inline bool InstallNormalFontFileToLuna(const FontFilesUniques &fonts)
{
    for (const auto &font : fonts)
    {
        if (!InstallNormalFontFileToLuna(font.string()))
        {
            return false;
        }
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
    // FIXME: If we will ever add file path here instead family name, than
    // `InstallNormalFontFileToLuna()` (both overloads) must be update to use actual resolved family
    // name instead empty string.
    static const std::vector<FontPathOrFamily> fonts = {
      std::string{"Liberation Mono"}, std::string{"DejaVu Sans Mono"},
      std::string{"Unifont"},         std::string{"Symbols Nerd Font Mono"},
      std::string{"FreeMono"},
    };
    return fonts;
}
