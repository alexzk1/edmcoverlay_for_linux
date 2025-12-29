#include "emoji_renderer.hpp"

#include "freetype/fttypes.h"
#include "lambda_visitors.hpp"
#include "opaque_ptr.h"
#include "unicode_splitter.hpp"

#include <fontconfig/fontconfig.h>
#include <freetype/freetype.h>
#include <freetype/ftimage.h>
#include <freetype/tttables.h>
#include <png.h>
#include <pngconf.h>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <iterator>
#include <map>
#include <mutex>
#include <string>
#include <type_traits>
#include <variant>
#include <vector>

#include <freetype/config/integer-types.h>

namespace {

const std::string kBase64Chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

inline std::string encodeBase64(const std::vector<unsigned char> &data)
{
    if (data.empty())
    {
        return {};
    }
    std::string out;
    int val = 0, valb = -6;
    for (const auto c : data)
    {
        val = (val << 8) + c;
        valb += 8;
        while (valb >= 0)
        {
            out.push_back(kBase64Chars[(val >> valb) & 0x3F]);
            valb -= 6;
        }
    }
    if (valb > -6)
    {
        out.push_back(kBase64Chars[((val << 8) >> (valb + 8)) & 0x3F]);
    }
    while (out.size() % 4)
    {
        out.push_back('=');
    }
    return out;
}

struct Bitmap
{
    Bitmap(unsigned int w, unsigned int h) : // NOLINT
        width(w),
        height(h),
        pixels{}
    {
        pixels.resize(static_cast<std::size_t>(width) * static_cast<std::size_t>(height)
                      * static_cast<std::size_t>(4u));
    }

    unsigned int width;
    unsigned int height;
    std::vector<unsigned char> pixels{}; // RGBA
};

Bitmap scaleBitmapToFitHeight(const Bitmap &bmp, unsigned char desiredHeight)
{
    if (bmp.height == 0 || bmp.width == 0 || bmp.pixels.empty())
    {
        return {0, 0};
    }

    const auto scale = static_cast<float>(desiredHeight) / static_cast<float>(bmp.height);
    const auto newWidth = static_cast<unsigned int>(static_cast<float>(bmp.width) * scale);
    const auto newHeight = desiredHeight;

    Bitmap result(newWidth, newHeight);
    for (unsigned int y = 0; y < newHeight; ++y)
    {
        const float srcY = static_cast<float>(y) / scale;
        const auto y0 = static_cast<unsigned int>(srcY);
        const auto y1 = std::min(y0 + 1, bmp.height - 1);
        const float fy = srcY - static_cast<float>(y0);

        for (unsigned int x = 0; x < newWidth; ++x)
        {
            const float srcX = static_cast<float>(x) / scale;
            const auto x0 = static_cast<unsigned int>(srcX);
            const auto x1 = std::min(x0 + 1, bmp.width - 1);
            const float fx = srcX - static_cast<float>(x0);

            for (int c = 0; c < 4; ++c)
            {
                const float v00 = bmp.pixels[(y0 * bmp.width + x0) * 4 + c];
                const float v10 = bmp.pixels[(y0 * bmp.width + x1) * 4 + c];
                const float v01 = bmp.pixels[(y1 * bmp.width + x0) * 4 + c];
                const float v11 = bmp.pixels[(y1 * bmp.width + x1) * 4 + c];

                const float val = (1 - fx) * (1 - fy) * v00 + fx * (1 - fy) * v10
                                  + (1 - fx) * fy * v01 + fx * fy * v11;

                result.pixels[(y * newWidth + x) * 4 + c] = static_cast<unsigned char>(val);
            }
        }
    }

    return result;
}

std::vector<unsigned char> encodePngRGBA(const Bitmap &bmp)
{
    std::vector<unsigned char> pngData;
    png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
    png_infop info = png_create_info_struct(png);
    if (!png || !info)
    {
        return {};
    }

    png_set_IHDR(png, info, bmp.width, bmp.height, 8, PNG_COLOR_TYPE_RGBA, PNG_INTERLACE_NONE,
                 PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);

    struct PngMemWriter
    {
        std::vector<unsigned char> &data;
    };
    PngMemWriter memWriter{pngData};

    auto writeFunc = [](png_structp png_ptr, png_bytep data, png_size_t length) {
        auto *w = reinterpret_cast<PngMemWriter *>(png_get_io_ptr(png_ptr)); // NOLINT
        w->data.insert(w->data.end(), data, data + length);
    };

    png_set_write_fn(png, &memWriter, writeFunc, nullptr);

    std::vector<png_bytep> rows(bmp.height);
    for (unsigned int y = 0; y < bmp.height; ++y)
    {
        rows[y] = const_cast<png_bytep>(&bmp.pixels[y * bmp.width * 4]); // NOLINT
    }

    png_set_rows(png, info, rows.data());
    png_write_png(png, info, PNG_TRANSFORM_IDENTITY, nullptr);

    png_destroy_write_struct(&png, &info);
    return pngData;
}

std::string findFontFile(const std::string &familyName)
{
    static std::once_flag init_once;
    std::call_once(init_once, []() {
        FcInit();
    });

    auto pat = AllocateOpaque<FcPattern>(&FcPatternDestroy, &FcPatternCreate);
    FcPatternAddString(pat, FC_FAMILY,
                       reinterpret_cast<const FcChar8 *>(familyName.c_str())); // NOLINT

    FcConfigSubstitute(nullptr, pat, FcMatchPattern);
    FcDefaultSubstitute(pat);

    auto match = AllocateOpaque<FcPattern>(
      &FcPatternDestroy,
      [](FcPattern *p) {
          FcResult result{FcResult::FcResultNoMatch};
          return FcFontMatch(nullptr, p, &result);
      },
      pat);

    std::string file;
    if (match)
    {
        FcChar8 *filename = nullptr;
        if (FcPatternGetString(match, FC_FILE, 0, &filename) == FcResultMatch)
        {
            file = reinterpret_cast<char *>(filename); // NOLINT
        }
    }
    return file;
}

std::string toKey(const FontPathOrFamily &key)
{
    static const LambdaVisitor visitor{
      [](const std::string &s) {
          return s;
      },
      [](const std::filesystem::path &p) {
          return p.string();
      },
    };
    return std::visit(visitor, key);
}

} // namespace

namespace emoji {
class EmojiRenderer::FtLibrary
{
  private:
    using FT_Face_Type = std::remove_pointer_t<FT_Face>;
    using FT_Library_Type = std::remove_pointer_t<FT_Library>;
    opaque_ptr<FT_Library_Type> libraryHandle = loadLibrary();
    std::map<std::string, opaque_ptr<FT_Face_Type>> faces;

    [[nodiscard]]
    static opaque_ptr<FT_Library_Type> loadLibrary()
    {
        return AllocateOpaque<FT_Library_Type>(&FT_Done_FreeType, []() -> FT_Library {
            FT_Library lib{nullptr};
            if (0 == FT_Init_FreeType(&lib))
            {
                return lib;
            }
            return nullptr;
        });
    }

    [[nodiscard]]
    opaque_ptr<FT_Face_Type> loadFace(const FontPathOrFamily &pathOrName) const
    {
        return AllocateOpaque<FT_Face_Type>(FT_Done_Face, [&pathOrName, this]() -> FT_Face {
            if (!libraryHandle)
            {
                return nullptr;
            }
            static const LambdaVisitor findFilePathVisitor = {
              [](const std::filesystem::path &pth) {
                  return pth.string();
              },
              [](const std::string &fam) {
                  return findFontFile(fam);
              },
            };
            const auto file = std::visit(findFilePathVisitor, pathOrName);

            FT_Face face{nullptr};
            if (FT_New_Face(libraryHandle, file.c_str(), 0, &face) == 0)
            {
                return face;
            }
            return nullptr;
        });
    }

  public:
    [[nodiscard]]
    opaque_ptr<FT_Face_Type> getFace(const FontPathOrFamily &pathOrName)
    {
        const auto key = toKey(pathOrName);
        auto it = faces.find(key);
        if (it != faces.end())
        {
            return it->second;
        }
        auto ptr = loadFace(pathOrName);
        if (ptr)
        {
            faces[key] = ptr;
        }
        return ptr;
    }

    [[nodiscard]]
    bool isValid() const
    {
        return libraryHandle;
    }

    [[nodiscard]]
    bool isColorEmojiFont(const opaque_ptr<FT_Face_Type> &face) const
    {
        static const std::uint32_t tag = FT_MAKE_TAG('C', 'B', 'D', 'T');
        FT_ULong length = 0;
        FT_Load_Sfnt_Table(face, tag, 0, nullptr, &length);
        return length != 0;
    }
};

EmojiRenderer::EmojiRenderer() :
    library(new EmojiRenderer::FtLibrary())
{
}

const PngData &EmojiRenderer::renderToPng(const EmojiToRender &what)
{
    static const PngData kNoResult;
    if (what.emoji == 0 || !library || !library->isValid())
    {
        return kNoResult;
    }

    const auto existing_it = emojies.find(what);
    if (existing_it != emojies.end())
    {
        return existing_it->second;
    }

    for (const auto &font_path : what.font.fontFaceOrPath)
    {
        auto face = library->getFace(font_path);
        if (!face)
        {
            continue;
        }
        FT_Set_Transform(face, nullptr, nullptr);

        FT_Int32 options = FT_LOAD_RENDER;

        if (library->isColorEmojiFont(face))
        {
            options |= FT_LOAD_COLOR;
            if (face->num_fixed_sizes == 0)
            {
                continue;
            }
            int best_match = 0;
            int diff =
              std::abs(static_cast<int>(what.font.fontSize.size) - face->available_sizes[0].height);
            for (int i = 1; i < face->num_fixed_sizes; ++i)
            {
                const int ndiff = std::abs(static_cast<int>(what.font.fontSize.size)
                                           - face->available_sizes[i].height);
                if (ndiff < diff)
                {
                    best_match = i;
                    diff = ndiff;
                }
            }
            if (FT_Select_Size(face, best_match))
            {
                continue;
            }
        }
        else
        {
            FT_Set_Pixel_Sizes(face, 0, what.font.fontSize.size);
        }

        const auto glyph_index = FT_Get_Char_Index(face, what.emoji);
        if (glyph_index == 0)
        {
            continue;
        }

        if (FT_Load_Glyph(face, glyph_index, options) != 0)
        {
            continue;
        }

        const auto glyph = face->glyph;

        if (FT_Render_Glyph(glyph, FT_RENDER_MODE_MAX))
        {
            continue;
        }

        const auto w = glyph->bitmap.width;
        const auto h = glyph->bitmap.rows;

        if (w == 0 || h == 0)
        {
            continue;
        }

        auto &result = emojies[what];

        Bitmap bmp(w, h);
        const auto pixel_mode = glyph->bitmap.pixel_mode;
        const auto pitch = glyph->bitmap.pitch;
        switch (pixel_mode)
        {
            case FT_PIXEL_MODE_GRAY:
                for (unsigned int y = 0; y < h; ++y)
                {
                    for (unsigned int x = 0; x < w; ++x)
                    {
                        const unsigned char v = glyph->bitmap.buffer[y * pitch + x];
                        const std::size_t idx = (y * w + x) * 4;         // NOLINT
                        bmp.pixels[idx + 0] = (what.color >> 16) & 0xFF; // R
                        bmp.pixels[idx + 1] = (what.color >> 8) & 0xFF;  // G
                        bmp.pixels[idx + 2] = (what.color >> 0) & 0xFF;  // B
                        bmp.pixels[idx + 3] = v;                         // A
                    }
                }
                break;
            case FT_PIXEL_MODE_BGRA:
                for (int y = 0; y < h; ++y)
                {
                    const unsigned char *src = std::next(glyph->bitmap.buffer, y * pitch); // NOLINT
                    unsigned char *dst = std::next(bmp.pixels.data(), y * w * 4);          // NOLINT
                    for (int x = 0; x < w; ++x)
                    {
                        // FreeType: BGRA
                        dst[4 * x + 0] = src[4 * x + 2]; // R
                        dst[4 * x + 1] = src[4 * x + 1]; // G
                        dst[4 * x + 2] = src[4 * x + 0]; // B
                        dst[4 * x + 3] = src[4 * x + 3]; // A
                    }
                }
                break;
            default:
                continue;
        }

        bmp = scaleBitmapToFitHeight(bmp, what.font.fontSize.size);

        result.width = bmp.width;
        result.height = bmp.height;
        result.png_base64 = encodeBase64(encodePngRGBA(bmp));
        return result;
    }
    return kNoResult;
}

EmojiRenderer::~EmojiRenderer() = default;

EmojiRenderer::TextFontWidth EmojiRenderer::computeWidth(const EmojiFontRequirement &font,
                                                         const std::vector<char32_t> &text)
{
    for (const auto &font_path : font.fontFaceOrPath)
    {
        const auto &face = library->getFace(font_path);
        if (!face)
        {
            continue;
        }
        FT_Set_Transform(face, nullptr, nullptr);
        FT_Int32 options = FT_LOAD_NO_BITMAP;

        if (library->isColorEmojiFont(face))
        {
            options |= FT_LOAD_COLOR;
            if (face->num_fixed_sizes == 0)
            {
                continue;
            }
            int best_match = 0;
            int diff =
              std::abs(static_cast<int>(font.fontSize.size) - face->available_sizes[0].height);
            for (int i = 1; i < face->num_fixed_sizes; ++i)
            {
                const int ndiff =
                  std::abs(static_cast<int>(font.fontSize.size) - face->available_sizes[i].height);
                if (ndiff < diff)
                {
                    best_match = i;
                    diff = ndiff;
                }
            }
            if (FT_Select_Size(face, best_match))
            {
                continue;
            }
        }
        else
        {
            FT_Set_Pixel_Sizes(face, 0, font.fontSize.size);
        }

        FT_Pos pen_x = 0;
        FT_UInt prev = 0;

        bool all_ok = true;
        for (const char32_t ch : text)
        {
            assert(!SpanRange::needsCustomRender(UnicodeSymbolsIterator::classify(ch))
                   && "Glyphs for custom rendering should not come here.");
            const FT_UInt glyph_index = FT_Get_Char_Index(face, ch);
            if (glyph_index == 0)
            {
                all_ok = false;
                break;
            }

            if (FT_Load_Glyph(face, glyph_index, options) == 0)
            {
                pen_x += face->glyph->advance.x >> 6;
                if (prev)
                {
                    FT_Vector kern;
                    if (FT_Get_Kerning(face, prev, glyph_index, FT_KERNING_DEFAULT, &kern) == 0)
                    {
                        pen_x += kern.x >> 6;
                    }
                }
            }
            else
            {
                all_ok = false;
                break;
            }

            prev = glyph_index;
        }
        if (all_ok)
        {
            return {static_cast<unsigned int>(pen_x), font_path};
        }
    }

    // Work around if we could not find valid font.
    return {static_cast<unsigned int>(text.size()) * font.fontSize.size, std::string{}};
}

EmojiRenderer &EmojiRenderer::instance()
{
    static thread_local EmojiRenderer inst;
    return inst;
}

} // namespace emoji
