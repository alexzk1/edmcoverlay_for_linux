#include "svgbuilder.h"

#include "drawables.h"
#include "strutils.h"

#include <cassert>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>

namespace {

constexpr int kTabSizeInSpaces = 2;

void beginSvg(std::ostringstream &out, int width, int height)
{
    out << "<svg xmlns=\"http://www.w3.org/2000/svg\" "
           "width=\""
        << width << "\" height=\"" << height << "\">\n";
}

void endSvg(std::ostringstream &out)
{
    out << "</svg>";
}

void makeSvgTextMultiline(std::ostringstream &svgOutStream, const draw_task::drawitem_t &drawTask,
                          const TIndependantFont &font)
{
    assert(drawTask.drawmode == draw_task::drawmode_t::text);
    font.updateSvgStream(svgOutStream);

    svgOutStream << "<text"
                 << " x='" << drawTask.x << "'"
                 << " y='" << drawTask.y << "'"
                 << " font-family='" << font.fontFamily << "'"
                 << " font-size='" << drawTask.text.getFinalFontSize() << "'"
                 << " fill='" << drawTask.color << "'"
                 << " xml:space='preserve'>";

    std::istringstream iss(drawTask.text.text);
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
            svgOutStream << " dy='1.2em'";
        }
        svgOutStream << ">" << utility::replace_tabs_with_spaces(line, kTabSizeInSpaces)
                     << "</tspan>";
    }
    svgOutStream << "</text>";
}

void makeSvgShape(std::ostringstream &svgOutStream, const draw_task::drawitem_t &drawTask,
                  const TIndependantFont &font)
{
    static constexpr int kStrokeWidth = 1;
    static constexpr int kMarkerHalfSize = 4;
    static constexpr int kTextOffsetX = 8;
    static constexpr int kTextOffsetY = 4;

    assert(drawTask.drawmode == draw_task::drawmode_t::shape);

    const auto drawLine = [&](int x1, int y1, int x2, int y2) {
        svgOutStream << "<line "
                     << "x1='" << x1 << "' "
                     << "y1='" << y1 << "' "
                     << "x2='" << x2 << "' "
                     << "y2='" << y2 << "' "
                     << "stroke='" << drawTask.color << "' "
                     << "stroke-width='" << kStrokeWidth << "' "
                     << "/>\n";
    };

    const auto drawMarker = [&](const draw_task::TMarkerInVectorInShape &marker,
                                int vector_font_size) {
        if (marker.IsCircle())
        {
            svgOutStream << "<circle cx=\"" << marker.x << "\" cy=\"" << marker.y << "\" r=\""
                         << kMarkerHalfSize << "\" fill=\"none\""
                         << " stroke=\"" << drawTask.color << "\""
                         << " stroke-width=\"" << kStrokeWidth << "\""
                         << " />";
        }
        else if (marker.IsCross())
        {
            svgOutStream << "<line x1=\"" << marker.x - kMarkerHalfSize << "\" y1=\""
                         << marker.y - kMarkerHalfSize << "\" x2=\"" << marker.x + kMarkerHalfSize
                         << "\" y2=\"" << marker.y + kMarkerHalfSize << "\" stroke=\""
                         << drawTask.color << "\" stroke-width=\"" << kStrokeWidth << "\" />"
                         << "<line x1=\"" << marker.x - kMarkerHalfSize << "\" y1=\""
                         << marker.y + kMarkerHalfSize << "\" x2=\"" << marker.x + kMarkerHalfSize
                         << "\" y2=\"" << marker.y - kMarkerHalfSize << "\" stroke=\""
                         << drawTask.color << "\" stroke-width=\"" << kStrokeWidth << "\" />";
        }

        if (marker.HasText())
        {
            draw_task::drawitem_t textTask;
            textTask.drawmode = draw_task::drawmode_t::text;
            textTask.x = marker.x + kMarkerHalfSize + kTextOffsetX;
            textTask.y = marker.y - kTextOffsetY;
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

    // Shapes and texts are relative to window and svg is sized as whole window.
    beginSvg(svgTextStream, windowWidth, windowHeight);
    switch (drawTask.drawmode)
    {
        case draw_task::drawmode_t::text:
            makeSvgTextMultiline(svgTextStream, drawTask, font);
            break;
        case draw_task::drawmode_t::shape:
            makeSvgShape(svgTextStream, drawTask, font);
            break;
        case draw_task::drawmode_t::idk:
            [[fallthrough]];
        case draw_task::drawmode_t::svg:
            return drawTask;
        default:
            throw std::runtime_error("Unhandled in code switch case.");
    }
    endSvg(svgTextStream);

    draw_task::drawitem_t res = drawTask;
    // We do not need to move new generated SVG itself, as it covers whole window.
    res.x = 0;
    res.y = 0;
    res.text = {};
    res.shape = {};
    res.svg.svg = svgTextStream.str();
    res.drawmode = draw_task::drawmode_t::svg;

    return res;
}
