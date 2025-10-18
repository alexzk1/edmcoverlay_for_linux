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
    SvgBuilder(const int screenWidth, const int screenHeight, const TIndependantFont &font,
               const draw_task::drawitem_t &drawTask);
    std::string toSvgString() const;

  protected:
    void begin(int width, int height);
    void end();

    /// @brief Makes SVG text multiline.
    void makeSvgTextMultiline(const draw_task::drawitem_t &drawTask, const TIndependantFont &font);

  private:
    std::ostringstream out;
};
