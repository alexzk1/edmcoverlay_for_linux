#pragma once

#include <limits.h>
#include <linux/limits.h>
#include <lunasvg.h>
#include <unistd.h>

#include <array>
#include <filesystem>
#include <iostream>
#include <mutex>
#include <string>
#include <unordered_set>

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

inline fs::path executable_dir()
{
    std::array<char, PATH_MAX> buf;
    ssize_t len = readlink("/proc/self/exe", buf.data(), buf.size() - 1);
    if (len <= 0)
    {
        return {};
    }

    buf[len] = '\0';
    return fs::path(buf.data()).parent_path();
}

inline bool InstallEmojiFont()
{
    const fs::path font = executable_dir() / "AppleColorEmoji.ttf";
    std::error_code ec;
    if (!fs::is_regular_file(font, ec) || ec)
    {
        std::cerr << "ERROR: Emoji font not found: " << font.string() << '\n';
        return false;
    }
    return InstallNormalFontFileToLuna(font);
}
