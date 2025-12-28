#include "svgbuilder.h"

#include "drawables.h"
#include "emoji_renderer.hpp"
#include "exec_exit.h"
#include "luna_default_fonts.h"
#include "strfmt.h"
#include "strutils.h"

#include <algorithm>
#include <cassert>
#include <cstddef>
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

enum class GlyphClass : std::uint8_t {
    Latin1, // 0x0000–0x00FF (1 byte UTF-8)
    BMP,    // 0x0100–0xFFFF (2–3 bytes UTF-8)
    Astral, // >= 0x10000 (4 bytes UTF-8)
    NotSet,
};

enum class SpanPosition : std::uint8_t {
    FirstSpan,
    InsideStringSpan,
    NotSet,
};

struct SpanRange
{
    std::size_t begin{0u};
    std::size_t end{0u};
    GlyphClass cls{GlyphClass::NotSet};
    SpanPosition position{SpanPosition::NotSet};

    SpanRange &setEnd(std::size_t e)
    {
        end = e;
        return *this;
    }

    [[nodiscard]]
    bool isValid() const
    {
        return end > begin;
    }

    [[nodiscard]]
    bool isEmoji() const
    {
        return cls == GlyphClass::Astral;
    }
};

/// @brief Per symbol string iterator. Symbols can be 1-4 bytes.
class Char32Iter
{
  public:
    explicit Char32Iter(const std::string &s) :
        src(s)
    {
    }

    bool rewindTo(const SpanRange &range)
    {
        next_byte_index = range.begin;
        seq_len = 0;
        cp = 0;

        return next() && classify() == range.cls;
    }

    /// @brief Position this object on next symbol in string.
    /// @returns false if end-of-string reached, object remains unchanged.
    bool next()
    {
        if (next_byte_index >= src.size())
        {
            return false;
        }

        const auto byte = static_cast<unsigned char>(src[next_byte_index]);
        if (byte < 0x80)
        {
            cp = byte;
            seq_len = 1;
        }
        else if ((byte & 0xE0) == 0xC0)
        {
            cp = byte & 0x1F;
            seq_len = 2;
        }
        else if ((byte & 0xF0) == 0xE0)
        {
            cp = byte & 0x0F;
            seq_len = 3;
        }
        else if ((byte & 0xF8) == 0xF0)
        {
            cp = byte & 0x07;
            seq_len = 4;
        }
        else
        {
            cp = 0xFFFD;
            seq_len = 1;
        }

        for (std::size_t j = 1; j < seq_len && next_byte_index + j < src.size(); ++j)
        {
            cp = (cp << 6) | (src[next_byte_index + j] & 0x3F);
        }

        next_byte_index += seq_len;
        return true;
    }

    ///@returns classification of the current positioned symbol.
    [[nodiscard]]
    GlyphClass classify() const
    {
        if (seq_len == 0u)
        {
            return GlyphClass::NotSet;
        }
        return classify(symbol());
    }

    ///@returns classification of the given symbol.
    [[nodiscard]]
    static GlyphClass classify(char32_t symbol)
    {
        if (symbol <= 0x00FF)
        {
            return GlyphClass::Latin1;
        }
        if (symbol >= 0x2700 && symbol <= 0x27BF)
        {
            return GlyphClass::Astral;
        }
        if (symbol <= 0xFFFF)
        {
            return GlyphClass::BMP;
        }
        return GlyphClass::Astral;
    }

    /// @returns current symbol, it should be used after next().
    [[nodiscard]]
    char32_t symbol() const
    {
        return cp;
    }

    /// @returns byte offset of the first byte of THIS symbol in the string.
    [[nodiscard]]
    std::size_t getStartIndex() const
    {
        return next_byte_index - seq_len;
    }

    /// @returns byte offset **immediately after this symbol** (start of next symbol).
    [[nodiscard]]
    std::size_t getEndIndex() const
    {
        return next_byte_index;
    }

  private:
    const std::string &src;
    std::size_t next_byte_index{0u};
    char32_t cp{0};
    std::size_t seq_len{0u};
};

/// @brief Detects chains of code pages in string.
std::vector<SpanRange> makeSpans(const std::string &text)
{
    std::vector<SpanRange> spans;
    spans.reserve(5);
    bool first = true;

    SpanRange current_span;
    for (Char32Iter iter(text); iter.next();)
    {
        const auto cls = iter.classify();
        if (first)
        {
            current_span = {iter.getStartIndex(), 0u, cls, SpanPosition::FirstSpan};
            first = false;
            continue;
        }

        // Astral characters must be 1 per span as we handle them differently.
        if (cls == GlyphClass::Astral || cls != current_span.cls)
        {
            spans.emplace_back(current_span.setEnd(iter.getStartIndex()));
            current_span = {iter.getStartIndex(), 0u, cls,
                            SpanPosition::InsideStringSpan}; // Pointing to next symbol.
        }
    }

    if (!first)
    {
        auto span = current_span.setEnd(text.size());
        if (span.isValid())
        {
            spans.emplace_back(span);
        }
    }

    return spans;
}

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

    void generateSvg(std::ostringstream &svgOutStream)
    {
        std::istringstream iss(textToDraw);
        std::string line;
        state.y = drawTask.y;
        while (std::getline(iss, line))
        {
            state.x = drawTask.x;
            processSingleLine(svgOutStream, line);
            state.y += kYSpacing * drawTask.text.getFinalFontSize();
        }
    }

  private:
    static constexpr double kXSpacing = 1.05;
    static constexpr double kYSpacing = 1.05;

    struct RenderState
    {
        int x{0};
        int y{0};
    };

    const draw_task::drawitem_t &drawTask;
    RenderState state;
    std::string textToDraw;

  protected:
    void processSingleLine(std::ostringstream &svgOutStream, const std::string &line)
    {
        const auto emojiSpans = makeSpans(line);
        for (const auto &span : emojiSpans)
        {
            if (span.isEmoji())
            {
                Char32Iter iter(line);
                if (!iter.rewindTo(span))
                {
#ifndef _NDEBUG
                    std::cerr << "Something went wrong. Could not rewind to pos " << span.begin
                              << "\n";
#endif
                    continue;
                }
                makeImage(svgOutStream, iter.symbol());
                continue;
            }
            makeText(svgOutStream, line, span);
        }
    }

    void makeImage(std::ostringstream &svgOutStream, const char32_t symbol)
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

        state.x += png.width;
    }

    void makeText(std::ostringstream &svgOutStream, const std::string &line, const SpanRange &range)
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
        state.x += measureWidhtOfText(sub);
    }

    [[nodiscard]]
    unsigned int measureWidhtOfText(const std::string &text) const
    {
        std::vector<char32_t> txt;
        txt.reserve(text.size());
        for (Char32Iter iter(text); iter.next();)
        {
            txt.emplace_back(iter.symbol());
        }

        const emoji::EmojiFontRequirement font{drawTask.text.getFinalFontSize(), GetTextFonts()};
        const auto computed = emoji::EmojiRenderer::instance().computeWidth(font, txt);

        // Work around if we could not find valid font.
        return 0u == computed && Char32Iter::classify(txt.front()) == GlyphClass::BMP
                 ? static_cast<unsigned int>(txt.size())
                     * static_cast<unsigned int>(
                       static_cast<double>(drawTask.text.getFinalFontSize()) * 0.85)
                 : computed;
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

#ifndef _NDEBUG
    std::cout << res.svg.svg << std::endl;
#endif

    return res;
}
