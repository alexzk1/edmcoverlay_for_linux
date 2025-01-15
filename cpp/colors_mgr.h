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

class MyXOverlayColorMap
{
  private:
    // FYI: Configurable value for the colors.
    inline static constexpr std::uint8_t kAlpha = 240;

    const opaque_ptr<Display> &g_display;
    const XWindowAttributes g_attrs;

    std::map<std::string, XColor> known_xcolors;
    std::map<std::string, opaque_ptr<XftColor>> known_fontcolors;

    struct TRGBAColor
    {
        std::uint8_t red;
        std::uint8_t green;
        std::uint8_t blue;
        std::uint8_t alpha;

        TRGBAColor() = delete;

        static std::uint16_t upScale(std::uint8_t color)
        {
            return (static_cast<uint32_t>(color) * 0xFFFFu) / 0xFFu;
        }

        [[nodiscard]]
        XRenderColor toRenderColor() const
        {
            return {upScale(red), upScale(green), upScale(blue), upScale(alpha)};
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

    // we take reference here to object, so this map must be destroyed prior object destroyed
    MyXOverlayColorMap(const opaque_ptr<Display> &g_display, XWindowAttributes g_attrs) :
        g_display(g_display),
        g_attrs(g_attrs)
    {
    }

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

        if (name.rfind("#", 0) == 0)
        {
            // direct hex color code
            if (name.length() == 7)
            {
                unsigned int r, g, b;
                sscanf(name.c_str(), "#%02x%02x%02x", &r, &g, &b);
                curr = TRGBAColor{static_cast<uint8_t>(r), static_cast<uint8_t>(g),
                                  static_cast<uint8_t>(b), static_cast<uint8_t>(kAlpha)};
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
