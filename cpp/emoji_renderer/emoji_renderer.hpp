#pragma once

#include "cm_ctors.h"
#include "font_size.hpp"

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace emoji {

struct EmojiFontRequirement
{
    font_size::FontPixelSize fontSize;
    std::vector<std::string> fontFaceOrPath;

    bool operator<(const EmojiFontRequirement &other) const
    {
        if (fontSize != other.fontSize)
        {
            return fontSize < other.fontSize;
        }
        return fontFaceOrPath < other.fontFaceOrPath; // lexicographical compare
    }
};

struct EmojiToRender
{
    char32_t emoji{0};
    EmojiFontRequirement font;
    std::uint32_t color = 0x000000FF; // ARGB color, black by default

    bool operator<(const EmojiToRender &other) const
    {
        if (emoji != other.emoji)
        {
            return emoji < other.emoji;
        }
        if (color != other.color)
        {
            return color < other.color;
        }
        return font < other.font;
    }
};

struct PngData
{
    unsigned int width{0u};
    unsigned int height{0u};
    std::string png_base64; // PNG/base64

    [[nodiscard]]
    bool isValid() const
    {
        return width > 0u && height > 0u && !png_base64.empty();
    }
};

/// @brief Does render of the single emoji as base64 encoded PNG.
class EmojiRenderer
{
  public:
    struct TextFontWidth
    {
        unsigned int computedWidth;
        // Font selected or empty if fallback was used.
        std::string fontUsedToMeasure;
    };

    NO_COPYMOVE(EmojiRenderer);

    /// @brief Renders emoji to bitmap if required system libraries were found.
    /// @returns "tofu" image if it could not render.
    const PngData &renderToPng(const EmojiToRender &what);
    ~EmojiRenderer();

    /// @returns computed pixel width of the text string with one of the fonts.
    TextFontWidth computeWidth(const EmojiFontRequirement &font, const std::vector<char32_t> &text);

    /// @returns reference to static thread_local instance.
    static EmojiRenderer &instance();

  private:
    EmojiRenderer();

    class FtLibrary;
    std::unique_ptr<FtLibrary> library;
    std::map<EmojiToRender, PngData> emojies;
};

} // namespace emoji
