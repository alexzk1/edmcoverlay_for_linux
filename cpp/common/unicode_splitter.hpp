#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

/// @brief Those below allows to iterate unicode text string and split it in chains by different
/// symbol categories. Some categories can be rendered by lunasvg, some require custom renderer
/// (emoji).

enum class GlyphClass : std::uint8_t {
    Latin1,  // 0x0000–0x00FF (1 byte UTF-8)
    Dingbat, //  >= 0x2700 && <= 0x27BF
    BMP,     // 0x0100–0xFFFF (2–3 bytes UTF-8)
    Astral,  // >= 0x10000 (4 bytes UTF-8)
    NotSet,
};

enum class SpanPosition : std::uint8_t {
    FirstSpan,
    InsideStringSpan,
    NotSet,
};

/// @brief Describes where 1 or more symbols begin and end in byte UTF8 string.
/// 1 SpanRange should have symbols of the same GlyphClass only.
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

    /// @returns true if object was properly set.
    [[nodiscard]]
    bool isValid() const
    {
        return end > begin;
    }

    /// @returns true if lunasvg cannot render such glyph in <text> tag.
    [[nodiscard]]
    bool needsCustomRender() const
    {
        return needsCustomRender(cls);
    }

    [[nodiscard]]
    static bool needsCustomRender(GlyphClass cls)
    {
        return cls == GlyphClass::Astral || cls == GlyphClass::Dingbat;
    }
};

/// @brief Per symbol string iterator. Symbols can be 1-4 bytes in UTF-8 form.
class UnicodeSymbolsIterator
{
  public:
    ///@param src is UTF-8 string (where symbols has variable byte length) which should be iterated
    /// per symbol.
    explicit UnicodeSymbolsIterator(const std::string &src) :
        src(src)
    {
    }

    /// @brief position this object to given @p range 1st symbol.
    /// @returns true if it was positioned ok.
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
            return GlyphClass::Dingbat;
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
/// Breaks UTF8 string into chains, where each chain has symbols of the same GlyphClass.
/// GlyphClass'es which cannot be rendered by lunasvg are set 1 symbol per 1 class.
inline std::vector<SpanRange> makeSpans(const std::string &text)
{
    std::vector<SpanRange> spans;
    spans.reserve(5);
    bool first = true;

    SpanRange current_span;
    for (UnicodeSymbolsIterator iter(text); iter.next();)
    {
        const auto cls = iter.classify();
        if (first)
        {
            current_span = {iter.getStartIndex(), 0u, cls, SpanPosition::FirstSpan};
            first = false;
            continue;
        }

        if (cls != current_span.cls)
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
