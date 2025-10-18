#include "svgbuilder.h"

#include "strutils.h"

#include <cassert>
#include <sstream>
#include <string>

SvgBuilder::SvgBuilder(const int screenWidth, const int screenHeight, const TIndependantFont &font,
                       const draw_task::drawitem_t &drawTask)
{
    using namespace draw_task;
    begin(screenWidth, screenHeight);
    if (drawTask.drawmode == drawmode_t::text)
    {
        makeSvgTextMultiline(drawTask, font);
    }
    end();
}

std::string SvgBuilder::toSvgString() const
{
    return out.str();
}

void SvgBuilder::begin(int width, int height)
{
    out << "<svg xmlns=\"http://www.w3.org/2000/svg\" "
           "width=\""
        << width << "\" height=\"" << height << "\">\n";
}

void SvgBuilder::end()
{
    out << "</svg>";
}

void SvgBuilder::makeSvgTextMultiline(const draw_task::drawitem_t &drawTask,
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
