#pragma once

#include "drawables.h"

#include <filesystem>
#include <optional>
#include <ostream>
#include <string>
#include <vector>

/// @brief Converts historical drawables to SVG format.
class SvgBuilder
{
  public:
    SvgBuilder(const int windowWidth, const int windowHeight, draw_task::drawitem_t drawTask);

    /// @brief Builds final
    draw_task::drawitem_t BuildSvgTask() const;

  private:
    int windowWidth;
    int windowHeight;
    draw_task::drawitem_t drawTask;
};
