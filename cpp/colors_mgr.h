#pragma once

#include "cm_ctors.h"
#include "opaque_ptr.h"

#include <X11/Xft/Xft.h>
#include <X11/Xlib.h>
#include <X11/extensions/Xrender.h>

#include <stdint.h>

#include <algorithm>
#include <cstdint>
#include <map>
#include <stdexcept>
#include <string>
#include <tuple>

/// @brief Allocates and caches colors as XColor and XftColor in 2 separated lists.
/// It understands some names like "red" and hex codes.
/// @details This class is used to manage colors for an overlay in a graphical application.
/// It uses the X11 library to allocate and cache colors as both XColor and XftColor objects.
/// The class provides methods to retrieve colors by name, either as an XColor or as an XftColor.
/// @note The class assumes that the display and window attributes are valid for the lifetime of the
/// object.
class MyXOverlayColorMap
{
  private:
    // FYI: Configurable value for the colors.
    inline static constexpr std::uint8_t kAlpha = 240;

    const opaque_ptr<Display> &g_display;
    const XWindowAttributes g_attrs;

    std::map<std::string, XColor> known_xcolors;
    std::map<std::string, opaque_ptr<XftColor>> known_fontcolors;

  public:
    /// @brief A simple struct to represent an RGBA color.
    struct TRGBAColor
    {
        std::uint8_t red;
        std::uint8_t green;
        std::uint8_t blue;
        std::uint8_t alpha;

        TRGBAColor() = delete;

        /// @brief Converts an 8-bit color value to a 16-bit color value.
        static std::uint16_t upScale(std::uint8_t color)
        {
            return (static_cast<uint32_t>(color) * 0xFFFFu) / 0xFFu;
        }

        /// @returns an XRenderColor object representing the RGBA color.
        [[nodiscard]]
        XRenderColor toRenderColor() const
        {
            return {upScale(red), upScale(green), upScale(blue), upScale(alpha)};
        }

        using cairo_color_t = std::tuple<double, double, double, double>;

        /// @returns a packed color tuple representing the RGBA color usable with Cairo.
        [[nodiscard]]
        cairo_color_t toPackedColorDoubles() const
        {
            static constexpr double div = 256.0;
            static_assert(sizeof(decltype(red)) == 1);
            static_assert(sizeof(decltype(green)) == 1);
            static_assert(sizeof(decltype(blue)) == 1);
            static_assert(sizeof(decltype(alpha)) == 1);
            return {red / div, green / div, blue / div, alpha / div};
        }
    };

  public:
    NO_COPYMOVE(MyXOverlayColorMap);
    ~MyXOverlayColorMap()
    {
        known_fontcolors.clear();
        known_xcolors.clear();
    }
    MyXOverlayColorMap() = delete;

    /// @note This class is not copyable or movable.  It assumes that the display and window
    /// attributes are valid for the lifetime of the object.
    /// @param g_display The X11 display to use for color allocation.
    /// @param g_attrs The window attributes to use for color allocation.
    MyXOverlayColorMap(const opaque_ptr<Display> &g_display, XWindowAttributes g_attrs) :
        g_display(g_display),
        g_attrs(g_attrs)
    {
    }

    /// @brief Retrieves a color by name or hexcode as an XColor.
    const XColor &get(std::string name)
    {
        // making string lowercased as we use names in lowercase everywhere here
        std::transform(name.begin(), name.end(), name.begin(), [](auto c) {
            return std::tolower(c);
        });

        // if we know that color just return it
        const auto it = known_xcolors.find(name);
        if (it != known_xcolors.end())
        {
            return it->second;
        }

        // otherwise request creation and return
        known_xcolors[name] = colorFromName(name);
        return known_xcolors[name];
    }

    /// @brief Retrieves a color by name or hexcode as an XftColor.
    opaque_ptr<XftColor> getFontColor(std::string name)
    {
        std::transform(name.begin(), name.end(), name.begin(), [](auto c) {
            return std::tolower(c);
        });

        // if we know that color just return it
        const auto it = known_fontcolors.find(name);
        if (it != known_fontcolors.end())
        {
            return it->second;
        }

        // otherwise request creation and return
        known_fontcolors[name] = createFontColor(name);
        return known_fontcolors[name];
    }

    /// @brief This function is used to decode a color name or hexcode into an RGB(A) color.
    /// @param name The color name or hexcode to decode.  If the name starts with "#", it is assumed
    /// to be a hex code.  Otherwise, it is looked up in a map of known colors.
    /// @return A TRGBAColor object representing the decoded color.
    static TRGBAColor decodeRGBAColor(const std::string &name)
    {
        // todo: add more colors here which can be recognized by string-name
        const static std::map<std::string, TRGBAColor> named_colors = {
          // Those 2 colors used to clear the frame
          {"transparent", {0, 0, 0, 0}},      {"solid_white", {255, 255, 255, 255}},

          {"white", {255, 255, 255, kAlpha}}, {"black", {0, 0, 0, kAlpha}},
          {"blue", {0, 0, 255, kAlpha}},      {"yellow", {255, 255, 0, kAlpha}},
          {"green", {0, 255, 0, kAlpha}},     {"red", {255, 0, 0, kAlpha}},
        };

        TRGBAColor curr{0xFF, 0xFF, 0xFF, kAlpha};
        const auto nameLen = name.length();
        const bool nl7 = nameLen == 7;
        const bool nl9 = nameLen == 9;
        if ((nl7 || nl9) && name.rfind /*Last occurence*/ ("#", 0) == 0)
        {
            // direct hex color code
            if (nl7)
            {
                unsigned int r, g, b;
                sscanf(name.c_str(), "#%02x%02x%02x", &r, &g, &b);
                curr = TRGBAColor{static_cast<uint8_t>(r), static_cast<uint8_t>(g),
                                  static_cast<uint8_t>(b), static_cast<uint8_t>(kAlpha)};
            }
            if (nl9)
            {
                unsigned int a, r, g, b;
                sscanf(name.c_str(), "#%02x%02x%02x%02x", &a, &r, &g, &b);
                curr = TRGBAColor{static_cast<uint8_t>(r), static_cast<uint8_t>(g),
                                  static_cast<uint8_t>(b), static_cast<uint8_t>(a)};
            }
        }
        else
        {
            const auto it = named_colors.find(name);
            if (it != named_colors.end())
            {
                curr = it->second;
            }
        }

        return curr;
    }

  private:
    XColor colorFromName(const std::string &name) const
    {
        return createXColorFromRGBA(decodeRGBAColor(name));
    }

    XColor createXColorFromRGBA(const TRGBAColor &aRGBA) const
    {
        auto color = allocCType<XColor>();

        auto c = aRGBA.toRenderColor();
        color.red = c.red;
        color.green = c.green;
        color.blue = c.blue;
        color.flags = DoRed | DoGreen | DoBlue;

        if (!XAllocColor(g_display, g_attrs.colormap, &color))
        {
            throw std::runtime_error("createXColorFromRGB: Cannot create color");
        }

        color.pixel = (color.pixel & 0x00ffffffu) | (aRGBA.alpha << 24);
        return color;
    }

    opaque_ptr<XftColor> createFontColor(const std::string &name) const
    {
        const auto renderColor = decodeRGBAColor(name).toRenderColor();

        auto onStack = allocCType<XftColor>();
        if (!XftColorAllocValue(g_display, g_attrs.visual, g_attrs.colormap, &renderColor,
                                &onStack))
        {
            throw std::runtime_error("Failed to allocate font color!");
        }
        return std::shared_ptr<XftColor>(new XftColor(onStack), [this](auto ptr) {
            if (ptr)
            {
                XftColorFree(g_display, g_attrs.visual, g_attrs.colormap, ptr);
                delete ptr;
            }
        });
    }
};
