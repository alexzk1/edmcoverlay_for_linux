#include "svgbuilder.h"

#include "drawables.h"
#include "emoji_renderer.hpp"
#include "luna_default_fonts.h"
#include "strfmt.h"
#include "strutils.h"
#include "unicode_splitter.hpp"

#include <algorithm>
#include <cassert>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#ifndef _NDEBUG
    #include <iostream>
#endif

namespace {
/*
 * The idea behind this is next: we convert all historical commands used on Windows (like draw text)
 * into SVG, and than we have only SVG renderer implemented.
 *
 * All incoming coordinates are in screen-space system. However, we cannot render full screen
 * size SVG for each text symbol changed because it is too slow.
 * So we want to translate incoming messages into "local" coordinate system, render smaller SVG,
 * than display that SVG shifted back to screen coordinates.
 */

constexpr int kTabSizeInSpaces = 2;
constexpr int kMarkerHalfSize = 4;
constexpr int kStrokeWidth = 1;
constexpr int kTextOffsetX = 1;
constexpr int kTextOffsetY = 0;

std::string escape_for_svg(std::string_view in)
{
    std::string out;
    out.reserve(in.size());

    for (char const c : in)
    {
        switch (c)
        {
            case '&':
                out += "&amp;";
                break;
            case '<':
                out += "&lt;";
                break;
            case '>':
                out += "&gt;";
                break;
            case '"':
                out += "&quot;";
                break;
            case '\'':
                out += "&apos;";
                break;
            default:
                out += c;
                break;
        }
    }
    return out;
}

/// @brief Convert given drawTask into svg <text>/<image> chain where <image> is used for
/// emodji.
class TextToSvgConverter
{
  public:
    explicit TextToSvgConverter(const draw_task::drawitem_t &drawTask) :
        drawTask(drawTask),
        state{}
    {
        assert(drawTask.drawmode == draw_task::drawmode_t::text);
        static const std::string nbsp = "\xC2\xA0";
        textToDraw = utility::replace_tabs_with_spaces(
          drawTask.text.text.empty() ? nbsp : drawTask.text.text, kTabSizeInSpaces);
    }

    /// @brief Does actual conversion and puts result into @p svgOutStream.
    void generateSvg(std::ostringstream &svgOutStream)
    {
        std::istringstream iss(textToDraw);
        std::string line;
        state.y = drawTask.y;
        while (std::getline(iss, line))
        {
            state.x = drawTask.x;
            processSingleLine(svgOutStream, line);
            state.y +=
              static_cast<int>(kYSpacing * static_cast<float>(drawTask.text.getFinalFontSize()));
        }
    }

  private:
    static constexpr float kXSpacing = 1.03f;
    static constexpr float kYSpacing = 1.05f;

    struct RenderState
    {
        int x{0};
        int y{0};
    };

    const draw_task::drawitem_t &drawTask;
    RenderState state;
    std::string textToDraw;

  protected:
    /// @brief process single line of the source text: split into possible <text> and <image>
    /// chains, then generate each separated.
    /// It tracks positions of the tags too.
    void processSingleLine(std::ostringstream &svgOutStream, const std::string &line)
    {
        const auto emojiSpans = makeSpans(line);
        for (const auto &span : emojiSpans)
        {
            if (span.needsCustomRender())
            {
                UnicodeSymbolsIterator iter(line);
                if (!iter.rewindTo(span))
                {
#ifndef _NDEBUG
                    std::cerr << "Something went wrong. Could not rewind to pos " << span.begin
                              << "\n";
#endif
                    continue;
                }
                // We can render 1 symbol at once, but span may have couple of the same class.
                do
                {
                    renderCusomSingleSymbolImageTag(svgOutStream, iter.symbol());
                }
                while (iter.next() && span.cls == iter.classify());
                continue;
            }
            makeTextTag(svgOutStream, line, span);
        }
    }

    /// @brief custom render of emoji symbols failed by lunasvg and add it as <image> tag per
    /// symbol.
    void renderCusomSingleSymbolImageTag(std::ostringstream &svgOutStream, const char32_t symbol)
    {
        using namespace format_helper;
        using namespace emoji;

        EmojiFontRequirement font{drawTask.text.getFinalFontSize(), GetEmojiFonts()};
        const auto &png = EmojiRenderer::instance().renderToPng({symbol, std::move(font)});
        if (!png.isValid())
        {
#ifndef _NDEBUG
            std::cerr << "Something went wrong. Could not draw emoji-png.\n";
#endif
            return;
        }
        svgOutStream << stringfmt(
          R"(<image x="%u" y="%u" width="%u" height="%u" href="data:image/png;base64,%s"/>)",
          state.x, state.y, png.width, png.height, png.png_base64);

        state.x += static_cast<int>(static_cast<float>(png.width) * kXSpacing);
    }

    /// @brief generates <text> tag which has no emoji symbols which are failed by lunasvg.
    void makeTextTag(std::ostringstream &svgOutStream, const std::string &line,
                     const SpanRange &range)
    {
        using namespace format_helper;
        const auto sub = line.substr(range.begin, range.end - range.begin);

        const std::string font_fam = range.cls == GlyphClass::Latin1
                                       ? stringfmt(R"(font-family="%s")", GetTextFonts().front())
                                       : "";

        svgOutStream << stringfmt(
          R"(<text x="%u" y="%u" font-size="%i" fill="%s" %s xml:space='preserve'>)", state.x,
          state.y + drawTask.text.getFinalFontSize(), drawTask.text.getFinalFontSize(),
          drawTask.color, font_fam)
                     << escape_for_svg(sub) << "</text>";
        state.x += static_cast<int>(static_cast<float>(measureWidhtOfText(sub)) * kXSpacing);
    }

    /// @brief tries to measure the width of text rendered by luasvg for <text> tag.
    /// @note we can set precise Latin font used and measure it, but for bitmap fonts we're doing
    /// guessings there. We find some font, but luasvg could find another.
    [[nodiscard]]
    unsigned int measureWidhtOfText(const std::string &text) const
    {
        std::vector<char32_t> txt;
        txt.reserve(text.size());
        for (UnicodeSymbolsIterator iter(text); iter.next();)
        {
            txt.emplace_back(iter.symbol());
        }

        const emoji::EmojiFontRequirement font{drawTask.text.getFinalFontSize(), GetTextFonts()};
        return emoji::EmojiRenderer::instance().computeWidth(font, txt);
    }
};

void makeSvgTextMultiline(std::ostringstream &svgOutStream, const draw_task::drawitem_t &drawTask)
{
    TextToSvgConverter converter(drawTask);
    converter.generateSvg(svgOutStream);
}

void makeSvgShape(std::ostringstream &svgOutStream, const draw_task::drawitem_t &drawTask)
{
    assert(drawTask.drawmode == draw_task::drawmode_t::shape);
    const auto drawLineWithColor = [&](int x1, int y1, int x2, int y2, const std::string &color) {
        svgOutStream << "<line "
                     << "x1='" << x1 << "' "
                     << "y1='" << y1 << "' "
                     << "x2='" << x2 << "' "
                     << "y2='" << y2 << "' "
                     << "stroke='" << color << "' "
                     << "stroke-width='" << kStrokeWidth << "'/>";
    };

    const auto drawLine = [&](int x1, int y1, int x2, int y2) {
        drawLineWithColor(x1, y1, x2, y2, drawTask.color);
    };

    const auto drawMarker = [&](const draw_task::TMarkerInVectorInShape &marker,
                                int vector_font_size) {
        if (marker.IsCircle())
        {
            svgOutStream << "<circle cx='" << marker.x << "' cy='" << marker.y << "' r='"
                         << kMarkerHalfSize << "' fill='none'"
                         << " stroke='" << marker.color << "'"
                         << " stroke-width='" << kStrokeWidth << "'"
                         << " />";
        }
        if (marker.IsCross())
        {
            drawLineWithColor(marker.x - kMarkerHalfSize, marker.y - kMarkerHalfSize,
                              marker.x + kMarkerHalfSize, marker.y + kMarkerHalfSize, marker.color);
            drawLineWithColor(marker.x - kMarkerHalfSize, marker.y + kMarkerHalfSize,
                              marker.x + kMarkerHalfSize, marker.y - kMarkerHalfSize, marker.color);
        }

        if (marker.HasText())
        {
            draw_task::drawitem_t textTask;
            textTask.drawmode = draw_task::drawmode_t::text;
            textTask.x = marker.x + kMarkerHalfSize + kTextOffsetX;
            textTask.y = marker.y - kTextOffsetY;
            textTask.color = marker.color;
            textTask.text.fontSize = vector_font_size;
            textTask.text.text = marker.text;
            makeSvgTextMultiline(svgOutStream, textTask);
        }
    };

    const bool had_vec = draw_task::ForEachVectorPointsPair(drawTask, drawLine, drawMarker);
    if (!had_vec && drawTask.shape.shape == "rect")
    {
        svgOutStream << "<rect x='" << drawTask.x << "' y='" << drawTask.y << "' width='"
                     << drawTask.shape.w << "' height='" << drawTask.shape.h
                     << "' fill='none' stroke='" << drawTask.color << "' stroke-width='"
                     << kStrokeWidth << "' />";
    }
}

} // namespace

// NOLINTNEXTLINE
SvgBuilder::SvgBuilder(const int windowWidth, const int windowHeight,
                       draw_task::drawitem_t drawTask) :
    windowWidth(windowWidth),
    windowHeight(windowHeight),
    drawTask(std::move(drawTask))
{
}

draw_task::drawitem_t SvgBuilder::BuildSvgTask() const
{
    std::ostringstream svgTextStream;
    int minX = std::numeric_limits<int>::max();
    int minY = std::numeric_limits<int>::max();
    if (drawTask.isShapeVector())
    {
        // Vectors have invalid drawTask.x/y set, instead each point is absolute screen coordinate.
        // We need to find bounding corner of it so corner becomes 0;0 of SVG.
        // Than we change task.x/y to this corner, so screen output will use to position SVG.
        int maxX = std::numeric_limits<int>::min();
        int maxY = std::numeric_limits<int>::min();

        for (const auto &node_ : drawTask.shape.vect.items())
        {
            const auto &val = node_.value();
            const int x = val["x"].get<int>();
            const int y = val["y"].get<int>();

            minX = std::min(minX, x);
            minY = std::min(minY, y);
            maxX = std::max(maxX, x);
            maxY = std::max(maxY, y);
        }

        auto width = maxX - minX;
        auto height = maxY - minY;

        if (drawTask.shape.vect.size() == 1)
        {
            height = 2 * kMarkerHalfSize + 1 + drawTask.shape.getFinalFontSize() + kTextOffsetY;
            width = windowWidth / 4;
            minX -= kMarkerHalfSize + 1;
            minY -= kMarkerHalfSize + 1;
        }
        svgTextStream << R"(<svg xmlns="http://www.w3.org/2000/svg" width=")" << width
                      << R"(" height=")" << height << R"(" overflow='visible' >)";

        svgTextStream << "<g transform='translate(" << -minX << "," << -minY << ")'>";
    }
    else
    {
        // This is not a vector task, so we had valid drawTask.x/y as corner for 0;0 of SVG.
        svgTextStream << R"(<svg xmlns="http://www.w3.org/2000/svg" overflow='visible' >)";
        svgTextStream << "<g transform='translate(" << -drawTask.x << "," << -drawTask.y << ")'>";
    }

    switch (drawTask.drawmode)
    {
        case draw_task::drawmode_t::text:
            makeSvgTextMultiline(svgTextStream, drawTask);
            break;
        case draw_task::drawmode_t::shape:
            makeSvgShape(svgTextStream, drawTask);
            break;
        case draw_task::drawmode_t::idk:
            // If we got unknown drawing task, just return it as-is, it could be the command.
            [[fallthrough]];
        case draw_task::drawmode_t::svg:
            // If task is direct SVG it does not need to be converted.
            return drawTask;
        default:
            throw std::runtime_error("Unhandled in code switch case.");
    }
    svgTextStream << "</g></svg>";

    draw_task::drawitem_t res = drawTask;
    if (drawTask.isShapeVector())
    {
        res.x = minX;
        res.y = minY;
    }
    res.text = {};
    res.shape = {};
    res.svg.svg = svgTextStream.str();
    res.drawmode = draw_task::drawmode_t::svg;

    // #ifndef _NDEBUG
    //     std::cout << res.svg.svg << std::endl;
    // #endif

    return res;
}
