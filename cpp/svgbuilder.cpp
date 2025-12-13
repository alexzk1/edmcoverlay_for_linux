#include "svgbuilder.h"

#include "drawables.h"
#include "strutils.h"

#include <algorithm>
#include <cassert>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

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

void makeSvgTextMultiline(std::ostringstream &svgOutStream, const draw_task::drawitem_t &drawTask,
                          const TIndependantFont &font)
{
    assert(drawTask.drawmode == draw_task::drawmode_t::text);
    font.updateSvgStream(svgOutStream);

    svgOutStream << "<text"
                 << " x='" << drawTask.x << "'"
                 << " y='" << drawTask.y + drawTask.text.getFinalFontSize() << "'"
                 << " font-family='" << font.fontFamily << "'"
                 << " font-size='" << drawTask.text.getFinalFontSize() << "'"
                 << " fill='" << drawTask.color << "'"
                 << " xml:space='preserve'>";

    // Ensure at least one glyph for lunasvg: empty text is represented by NBSP
    static const std::string nbsp = "\xC2\xA0";
    const auto &src_text = drawTask.text.text.empty() ? nbsp : drawTask.text.text;

    std::istringstream iss(src_text);
    std::string line;
    bool first = true;
    while (std::getline(iss, line))
    {
        svgOutStream << "<tspan x='" << drawTask.x << "'";
        if (first)
        {
            svgOutStream << " dy='0em'";
            first = false;
        }
        else
        {
            svgOutStream << " dy='1.05em'";
        }
        svgOutStream << ">"
                     << escape_for_svg(utility::replace_tabs_with_spaces(line, kTabSizeInSpaces))
                     << "</tspan>";
    }
    svgOutStream << "</text>";
}

void makeSvgShape(std::ostringstream &svgOutStream, const draw_task::drawitem_t &drawTask,
                  const TIndependantFont &font)
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
            makeSvgTextMultiline(svgOutStream, textTask, font);
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

SvgBuilder::SvgBuilder(const int windowWidth, const int windowHeight, TIndependantFont font,
                       draw_task::drawitem_t drawTask) :
    windowWidth(windowWidth),
    windowHeight(windowHeight),
    font(std::move(font)),
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
        svgTextStream << "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"" << width
                      << "\" height=\"" << height << "\" overflow='visible' >";

        svgTextStream << "<g transform='translate(" << -minX << "," << -minY << ")'>";
    }
    else
    {
        // This is not a vector task, so we had valid drawTask.x/y as corner for 0;0 of SVG.
        svgTextStream << "<svg xmlns=\"http://www.w3.org/2000/svg\" overflow='visible' >";
        svgTextStream << "<g transform='translate(" << -drawTask.x << "," << -drawTask.y << ")'>";
    }

    switch (drawTask.drawmode)
    {
        case draw_task::drawmode_t::text:
            makeSvgTextMultiline(svgTextStream, drawTask, font);
            break;
        case draw_task::drawmode_t::shape:
            makeSvgShape(svgTextStream, drawTask, font);
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

    return res;
}
