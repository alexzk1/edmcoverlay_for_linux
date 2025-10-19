#pragma once

#include "drawables.h"

#include <filesystem>
#include <optional>
#include <sstream>
#include <string>

/// @brief Describes font to use by SVG renderer later.
struct TIndependantFont
{
    std::string fontFamily{"Liberation Mono"};
    std::optional<std::filesystem::path> fontFile{std::nullopt};
};

/// @brief Converts historical drawables to SVG format.
class SvgBuilder
{
  public:
    SvgBuilder(const int windowWidth, const int windowHeight, TIndependantFont font,
               draw_task::drawitem_t drawTask);

    draw_task::drawitem_t BuildSvgTask() const;

  private:
    int windowWidth;
    int windowHeight;
    TIndependantFont font;
    draw_task::drawitem_t drawTask;
};
