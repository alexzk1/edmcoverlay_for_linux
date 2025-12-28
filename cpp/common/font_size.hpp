#pragma once

#include <cstdint>
#include <type_traits>

namespace font_size {
struct TagFontPixelSize
{
};

struct TagFontPointSize
{
};

template <typename taTag>
struct FontSize
{
    std::uint32_t size;

    bool operator==(const FontSize<taTag> &other) const
    {
        return size == other.size;
    }

    bool operator!=(const FontSize<taTag> &other) const
    {
        return !(*this == other);
    }

    bool operator<(const FontSize<taTag> &other) const
    {
        return size < other.size;
    }
};

using FontPixelSize = FontSize<TagFontPixelSize>;
using FontPointSize = FontSize<TagFontPointSize>;

template <typename taTo, typename taFrom>
taTo convert(const taFrom &src)
{
    if constexpr (std::is_same_v<taTo, taFrom>)
    {
        return src;
    }
    // 96px = 72pt
    if constexpr (std::is_same_v<taFrom, FontPixelSize>)
    {
        static constexpr float px2pt = 72.0f / 96.0f;
        return FontPointSize{
          static_cast<std::uint32_t>(static_cast<float>(src.size /*px*/) * px2pt)};
    }
    else
    {
        static constexpr float pt2px = 96.0f / 72.0f;
        return FontPixelSize{
          static_cast<std::uint32_t>(static_cast<float>(src.size /*pt*/) * pt2px)};
    }
}
} // namespace font_size
