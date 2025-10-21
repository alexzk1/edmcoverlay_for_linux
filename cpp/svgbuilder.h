#pragma once

#include "drawables.h"

#include <filesystem>
#include <optional>
#include <ostream>
#include <string>

/// @brief Describes font to use by SVG renderer later.
struct TIndependantFont
{
    std::string fontFamily{"Liberation Mono"};
    std::optional<std::filesystem::path> fontFile{std::nullopt};

    /// @brief Appends SVG style if this object has font file defined.
    void updateSvgStream(std::ostream &svgOutStream) const
    {
        if (fontFile.has_value() && !fontFile->empty())
        {
            svgOutStream << "<style>@font-face { font-family: '" << fontFamily << "'; src: url('"
                         << *fontFile << "'); }</style>\n";
        }
    }
};

/// @brief Converts historical drawables to SVG format.
class SvgBuilder
{
  public:
    SvgBuilder(const int windowWidth, const int windowHeight, TIndependantFont font,
               draw_task::drawitem_t drawTask);

    /// @brief Builds final
    draw_task::drawitem_t BuildSvgTask() const;

  private:
    int windowWidth;
    int windowHeight;
    TIndependantFont font;
    draw_task::drawitem_t drawTask;
};
