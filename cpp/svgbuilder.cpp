#include "svgbuilder.h"

#include "drawables.h"
#include "strutils.h"

#include <cassert>
#include <sstream>
#include <string>
#include <utility>

namespace {
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

void makeSvgTextMultiline(std::ostringstream &out, const draw_task::drawitem_t &drawTask,
                          const TIndependantFont &font)
{
    constexpr int kTabSizeInSpaces = 2;

    assert(drawTask.drawmode == draw_task::drawmode_t::text);
    if (font.fontFile.has_value() && !font.fontFile->empty())
    {
        out << "<style>@font-face { font-family: '" << font.fontFamily << "'; src: url('"
            << *font.fontFile << "'); }</style>";
    }

    out << "<text"
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
        out << "<tspan x='" << drawTask.x << "'";
        if (first)
        {
            out << " dy='0em'";
            first = false;
        }
        else
        {
            out << " dy='1.2em'";
        }
        out << ">" << utility::replace_tabs_with_spaces(line, kTabSizeInSpaces) << "</tspan>";
    }
    out << "</text>";
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
    draw_task::drawitem_t res = drawTask;
    std::ostringstream out;
    if (drawTask.drawmode == draw_task::drawmode_t::text)
    {
        beginSvg(out, windowWidth, windowHeight);
        makeSvgTextMultiline(out, drawTask, font);
        endSvg(out);

        res.x = 0;
        res.y = 0;
        res.text = {};
        res.svg.svg = out.str();
    }
    res.drawmode = draw_task::drawmode_t::svg;
    return res;
}
