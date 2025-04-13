#include "xoverlayoutput.h"

#include "cm_ctors.h"
#include "colors_mgr.h"
#include "drawables.h"
#include "opaque_ptr.h"

#include <X11/X.h>
#include <X11/Xatom.h>
#include <X11/Xft/Xft.h>
#include <X11/Xlib.h>
#include <X11/Xos.h>
#include <X11/Xutil.h>
#include <X11/extensions/Xcomposite.h>
#include <X11/extensions/Xfixes.h>
#include <X11/extensions/Xrender.h>
#include <X11/extensions/shape.h>
#include <X11/extensions/shapeconst.h>

#include <fontconfig/fontconfig.h>
#include <memory.h>
#ifdef WITH_CAIRO
    #include <cairo.h>

    #include <cairo-xlib.h>
#endif

#include <cassert>
#include <cstdint>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <vector>

namespace {
// Events for normal windows
constexpr static long BASIC_EVENT_MASK = StructureNotifyMask | ExposureMask | PropertyChangeMask
                                         | EnterWindowMask | LeaveWindowMask | KeyPressMask
                                         | KeyReleaseMask | KeymapStateMask;

constexpr static long NOT_PROPAGATE_MASK = KeyPressMask | KeyReleaseMask | ButtonPressMask
                                           | ButtonReleaseMask | PointerMotionMask
                                           | ButtonMotionMask;

enum class ETextDecor {
    NoRectangle,
    Rectangle,
};

/// @brief Keeps window strings.
/// @note String [buffer] must exist while allocated hint exists.
class CWindowClass
{
  public:
    CWindowClass() = delete;
    NO_COPYMOVE(CWindowClass);

    explicit CWindowClass(const std::string &window_class) :
        window_class(window_class),
        classHint(AllocateOpaque<XClassHint>(&XFree, &XAllocClassHint))
    {
        classHint->res_class = const_cast<char *>(window_class.c_str()); // NOLINT
        classHint->res_name = const_cast<char *>(window_class.c_str());  // NOLINT
    }
    void Set(Display *display, Window w) const
    {
        XSetClassHint(display, w, classHint);
    }

    ~CWindowClass()
    {
        classHint.reset();
        window_class.clear();
    };

  private:
    std::string window_class;
    opaque_ptr<XClassHint> classHint;
};

} // namespace

/// @brief Real tasks to do on X server, it is private, so can be replaced by Wayland.
class XPrivateAccess
{
  private:
    // Make sure it is 1st field, so it is cleared last in dtor.
    struct TXInitFreeCaller;
    std::unique_ptr<TXInitFreeCaller> initializer;

    // I think, text should existis while window exists.
    CWindowClass window_class;

  public:
    template <typename T, typename taDeAllocator, typename taAllocator, typename... taAllocArgs>
    auto Allocate(taDeAllocator aDeallocate, taAllocator aAllocate, taAllocArgs &&...aArgs) const
    {
        return AllocateOpaque<T>(
          [this, dealloc = std::forward<taDeAllocator>(aDeallocate)](auto *ptr) {
              if (ptr)
              {
                  dealloc(g_display, ptr);
                  flush();
              }
          },
          std::forward<taAllocator>(aAllocate), std::forward<taAllocArgs>(aArgs)...);
    }

    XPrivateAccess() = delete;
    NO_COPYMOVE(XPrivateAccess);

    // NOLINTNEXTLINE
    XPrivateAccess(const std::string &window_class, int window_xpos, int window_ypos,
                   int window_width, int window_height) :
        initializer(std::make_unique<TXInitFreeCaller>()),
        window_class(window_class),
        window_xpos(window_xpos),
        window_ypos(window_ypos),
        window_width(window_width),
        window_height(window_height)
    {
        XftInit(nullptr);

        openDisplay();
        createShapedWindow();

        single_gc = allocGlobGC();
        colors = std::make_shared<MyXOverlayColorMap>(g_display, getAttributes());
    }

    ~XPrivateAccess()
    {
        loadedFonts.clear();
        colors.reset();
        single_gc.reset();
        g_display.reset();

        // This must be last because it closes X's threads.
        initializer.reset();
    }

    void cleanGC(GC gc) const
    {
        if (gc)
        {
            const auto &white = colors->get("solid_white");
            const auto &transparent = colors->get("transparent");
            XSetBackground(g_display, gc, white.pixel);
            XSetForeground(g_display, gc, transparent.pixel);
            XFillRectangle(g_display, g_win, gc, 0, 0, window_width, window_height);
        }
    }

    void cleanGC() const
    {
        cleanGC(single_gc);
    }

    void flush() const
    {
        XFlush(g_display);
    }

    const opaque_ptr<XftFont> &getFont(int size)
    {
        if (loadedFonts.find(size) == loadedFonts.end())
        {
            loadedFonts[size] = allocFont(size);
        }
        return loadedFonts.at(size);
    }

    const opaque_ptr<XftFont> &getFont(const std::string &sizeText)
    {
        // large = normal + kDeltaFontDifference
        constexpr int kDeltaFontDifference = 4;
        constexpr int kNormalFontSize = 16;

        // FYI: I set those big numbers for my eyes with glasses. Somebody may want lower / bigger.
        // From the other side, existing plugins send fixed distance between strings.
        // This one looks okish for Canon's.

        return sizeText == "large" ? getFont(kNormalFontSize + kDeltaFontDifference)
                                   : getFont(kNormalFontSize);
    }

    /// @returns true if transparency is avail in system now.
    bool isTransparencyAvail() const
    {
        if (!g_display)
        {
            return false;
        }

        const Atom cmAtom = XInternAtom(g_display, "_NET_WM_CM_S0", 1);
        if (cmAtom == None)
        {
            return false;
        }

        const Window cmOwner = XGetSelectionOwner(g_display, cmAtom);
        return cmOwner != None;
    }

  private:
    struct TXInitFreeCaller
    {
        NO_COPYMOVE(TXInitFreeCaller);
        TXInitFreeCaller()
        {
            XInitThreads();
        }
        ~TXInitFreeCaller()
        {
            XFreeThreads();
        }
    };
    std::unordered_map<int, opaque_ptr<XftFont>> loadedFonts;

    opaque_ptr<_XGC> allocGlobGC() const
    {
        return Allocate<_XGC>(XFreeGC, XCreateGC, g_display, g_win, 0, nullptr);
    }

    /// @brief Allocates and caches fonts. It tries to pick font family which is installed in
    /// system, ordered according to my personal preferences.
    /// @note configurable font's families below.
    opaque_ptr<XftFont> allocFont(const int aFontSize) const
    {
        const auto fontString = [&aFontSize](const std::string &family) {
            std::stringstream ss;
            ss << family;
            ss << ":size=" << aFontSize;
            ss << ":antialias=true";
            return ss.str();
        };

        // Font families to try.
        static const std::vector<std::string> kFontsToTry = {
          "Liberation Mono", "DejaVu Sans Mono", "Source Code Pro", "Ubuntu Mono",     "Monospace",
          "Sans Serif",      "Liberation Serif", "Serif",           "Liberation Sans", "Open Sans",
        };

        opaque_ptr<XftFont> font;

        for (const auto &family : kFontsToTry)
        {
            const auto fontStr = fontString(family);
            font = Allocate<XftFont>(XftFontClose, XftFontOpenName, g_display, g_screen,
                                     fontStr.c_str());

            if (font)
            {
                std::cout << "Overlay allocated font: " << fontStr << std::endl;
                break;
            }
        }

        if (!font)
        {
            throw std::runtime_error("Overlay could not load any font.");
        }

        return font;
    }

    void openDisplay()
    {
        g_display = std::shared_ptr<Display>(XOpenDisplay(nullptr), [](Display *p) {
            if (p)
            {
                XCloseDisplay(p);
            }
        });

        if (!g_display)
        {
            throw std::runtime_error("Failed to open X display");
        }

        if (!isTransparencyAvail())
        {
            std::cerr << "Composite manager is absent." << std::endl;
            std::cerr << "Please check instructions: "
                         "https://wiki.archlinux.org/index.php/Xcompmgr"
                      << std::endl;
            throw std::runtime_error("Transparance is impossible.");
        }
        g_screen = DefaultScreen(g_display);

        // Has shape extions?
        int shape_event_base = 0;
        int shape_error_base = 0;

        if (!XShapeQueryExtension(g_display, &shape_event_base, &shape_error_base))
        {
            throw std::runtime_error("NO shape extension in your system !");
        }

        g_root = DefaultRootWindow(g_display);
    }
    // Create a window
    void createShapedWindow()
    {
        auto vinfo = allocCType<XVisualInfo>();
        XMatchVisualInfo(g_display, g_screen, 32, TrueColor, &vinfo);

        auto attr = allocCType<XSetWindowAttributes>();
        attr.background_pixmap = None;
        attr.background_pixel = 0;
        attr.border_pixel = 0;
        attr.win_gravity = NorthWestGravity;
        attr.bit_gravity = ForgetGravity;
        attr.save_under = 1;
        attr.event_mask = BASIC_EVENT_MASK;
        attr.do_not_propagate_mask = NOT_PROPAGATE_MASK;
        attr.override_redirect = 1; // OpenGL > 0
        attr.colormap = XCreateColormap(g_display, g_root, vinfo.visual, AllocNone);

        // unsigned long mask =
        // CWBackPixel|CWBorderPixel|CWWinGravity|CWBitGravity|CWSaveUnder|CWEventMask|CWDontPropagate|CWOverrideRedirect;
        const unsigned long mask = CWColormap | CWBorderPixel | CWBackPixel | CWEventMask
                                   | CWWinGravity | CWBitGravity | CWSaveUnder | CWDontPropagate
                                   | CWOverrideRedirect;

        g_win =
          XCreateWindow(g_display, g_root, window_xpos, window_ypos, window_width, window_height, 0,
                        vinfo.depth, InputOutput, vinfo.visual, mask, &attr);
        window_class.Set(g_display, g_win);
        std::cout << "WMID: " << g_win << std::endl;

        /* g_bitmap = XCreateBitmapFromData (g_display, RootWindow(g_display, g_screen), (char
         * *)myshape_bits, myshape_width, myshape_height); */

        // XShapeCombineMask(g_display, g_win, ShapeBounding, 900, 500, g_bitmap, ShapeSet);
        XShapeCombineMask(g_display, g_win, ShapeInput, 0, 0, None, ShapeSet);

        // We want shape-changed event too
        XShapeSelectInput(g_display, g_win, ShapeNotifyMask);

        // Tell the Window Manager not to draw window borders (frame) or title.
        auto wattr = allocCType<XSetWindowAttributes>();
        wattr.override_redirect = 1;
        XChangeWindowAttributes(g_display, g_win, CWOverrideRedirect, &wattr);

        // pass through input
        const XserverRegion region = XFixesCreateRegion(g_display, nullptr, 0);
        // XFixesSetWindowShapeRegion (g_display, w, ShapeBounding, 0, 0, 0);
        XFixesSetWindowShapeRegion(g_display, g_win, ShapeInput, 0, 0, region);
        XFixesDestroyRegion(g_display, region);

        // Show the window
        XMapWindow(g_display, g_win);

#ifdef WITH_CAIRO
        cairo_surface = AllocateOpaque<cairo_surface_t>(cairo_surface_destroy,
                                                        cairo_xlib_surface_create, g_display, g_win,
                                                        vinfo.visual, window_width, window_height);
        if (cairo_surface)
        {
            cairo = AllocateOpaque<cairo_t>(cairo_destroy, cairo_create, cairo_surface);
            if (!cairo)
            {
                std::cerr << "Failed to allocate drawing cairo context." << std::endl;
            }
        }
        else
        {
            std::cerr << "Failed to create cairo surface." << std::endl;
        }
#endif
    }

    XWindowAttributes getAttributes() const
    {
        auto attrs = allocCType<XWindowAttributes>();
        XGetWindowAttributes(g_display, g_win, &attrs);
        return attrs;
    }

    const unsigned char *getWindowPropertyAny(const char *const aPropertyName,
                                              const Window aWindow) const
    {
        constexpr int kMaximumReturnedCountOf32Bits = 1024;
        if (aWindow == 0)
        {
            return nullptr;
        }
        Atom actual_type = 0;
        int actual_format = 0;
        unsigned long nitems = 0, bytes_after = 0; // NOLINT
        unsigned char *prop = nullptr;

        const auto filter_atom = XInternAtom(g_display, aPropertyName, True);
        const auto status = XGetWindowProperty(
          g_display, aWindow, filter_atom, 0, kMaximumReturnedCountOf32Bits, False, AnyPropertyType,
          &actual_type, &actual_format, &nitems, &bytes_after, &prop);

        if (status != Success)
        {
            std::cerr << "XGetWindowProperty failed with status: " << status << "." << std::endl;
        }

        return prop;
    }

    template <typename ExpectedType,
              typename = std::enable_if_t<std::is_integral<ExpectedType>::value>> // NOLINT
    ExpectedType getWindowPropertyInt(const char *const aPropertyName, const Window aWindow) const
    {
        constexpr auto kSize = sizeof(ExpectedType);
        static_assert(kSize == 1 || kSize == 2 || kSize == 4,
                      "Only 8/16/32 bits are supported by XGetWindowProperty()");

        ExpectedType tmp{0};
        const auto ptr = getWindowPropertyAny(aPropertyName, aWindow);
        if (ptr)
        {
            memcpy(&tmp, ptr, sizeof(tmp));
        }

        return tmp;
    }

    std::string getWindowPropertyString(const char *const aPropertyName, const Window aWindow) const
    {
        const auto ptr = getWindowPropertyAny(aPropertyName, aWindow);
        // NOLINTNEXTLINE
        return {reinterpret_cast<const char *const>(ptr)};
    }

  public:
    const int window_xpos;
    const int window_ypos;
    const int window_width;
    const int window_height;
    std::shared_ptr<MyXOverlayColorMap> colors{nullptr};

    opaque_ptr<Display> g_display{nullptr};
    Window g_root{0};
    int g_screen{0};
    Window g_win{0};
    opaque_ptr<_XGC> single_gc{nullptr};
#ifdef WITH_CAIRO
    // Order is important.
    opaque_ptr<cairo_surface_t> cairo_surface;
    opaque_ptr<cairo_t> cairo;
#endif
    static int utf8CharactersCount(const std::string &str)
    {
        int count = 0;
        for (const auto &c : str)
        {
            if ((c & 0x80) == 0 || (c & 0xc0) == 0xc0)
            {
                ++count;
            }
        }
        return count;
    }

    void drawUtf8String(const opaque_ptr<XftFont> &aFont, const std::string &aColor, int aX, int aY,
                        const std::string &aString, const ETextDecor aRectangle) const
    {
        // https://github.com/jsynacek/xft-example/blob/master/main.c

        const auto length = static_cast<int>(aString.length());
        // NOLINTNEXTLINE
        const auto *str = reinterpret_cast<const FcChar8 *>(aString.c_str());

        if (!aFont)
        {
            throw std::runtime_error("Tried to draw string without font!!!");
        }

        if (ETextDecor::Rectangle == aRectangle)
        {
            XGlyphInfo extents;
            XftTextExtentsUtf8(g_display, aFont, str, length, &extents);

            const auto &black = colors->get("black");
            XSetForeground(g_display, single_gc, black.pixel);
            XFillRectangle(g_display, g_win, single_gc, aX, aY, extents.width, extents.height);
        }

        auto color = colors->getFontColor(aColor);
        if (!color)
        {
            throw std::runtime_error("Tried to draw string without color!!!");
        }
        const auto attrs = getAttributes();
        auto draw = AllocateOpaque<XftDraw>(XftDrawDestroy, XftDrawCreate, g_display, g_win,
                                            attrs.visual, attrs.colormap);
        assert(XftDrawColormap(draw) == attrs.colormap);
        assert(XftDrawVisual(draw) == attrs.visual);

        // XftDrawRect(draw, color, 50, 50, 250, 250);
        XftDrawStringUtf8(draw, color, aFont, aX, aY + aFont->ascent, str, length);
    }

    // X11 does not support 64 bit pids.
    std::uint32_t getFocusedWindowPid() const
    {
        Window focused = 0;
        int revert_to = 0;
        XGetInputFocus(g_display, &focused, &revert_to);
        return getWindowPropertyInt<std::uint32_t>("_NET_WM_PID", focused);
    }

#ifdef WITH_CAIRO
    void cairoSetColor(const std::string &color) const
    {
        if (cairo)
        {
            auto [r, g, b, a] = colors->decodeRGBAColor(color).toPackedColorDoubles();
            cairo_set_source_rgba(cairo, r, g, b, a);
        }
    }
#endif
};

//**********************************************************************************************************************
//*****************************XOverlayOutput***************************************************************************
//**********************************************************************************************************************

XOverlayOutput::XOverlayOutput(const std::string &window_class, int window_xpos, int window_ypos,
                               int window_width, int window_height) :
    xserv(new XPrivateAccess(window_class, window_xpos, window_ypos, window_width, window_height))
{
    xserv->cleanGC();
}

XOverlayOutput::~XOverlayOutput()
{
    xserv.reset();
}

bool XOverlayOutput::isTransparencyAvail() const
{
    return xserv->isTransparencyAvail();
}

void XOverlayOutput::cleanFrame()
{
    xserv->cleanGC();
}

void XOverlayOutput::flushFrame()
{
    xserv->flush();
}

void XOverlayOutput::showVersionString(const std::string &version, const std::string &color)
{
    xserv->drawUtf8String(xserv->getFont(12), color, 0, 0, version, ETextDecor::Rectangle);
}

void XOverlayOutput::draw(const draw_task::drawitem_t &drawitem)
{
    const auto &gc = xserv->single_gc;
    const auto &g_display = xserv->g_display;
    const auto &g_win = xserv->g_win;

    if (drawitem.drawmode == draw_task::drawmode_t::text)
    {
        const auto &font = drawitem.text.fontSize ? xserv->getFont(*drawitem.text.fontSize)
                                                  : xserv->getFont(drawitem.text.size);
        xserv->drawUtf8String(font, drawitem.color, drawitem.x, drawitem.y, drawitem.text.text,
                              ETextDecor::NoRectangle);
    }
    else
    {
        const auto main_color = xserv->colors->get(drawitem.color);
        XSetForeground(g_display, gc, main_color.pixel);
#ifdef WITH_CAIRO
        if (xserv->cairo)
        {
            xserv->cairoSetColor(drawitem.color);
        }
#endif

        const auto drawLine = [&](int x1, int y1, int x2, int y2) {
#ifdef WITH_CAIRO
            if (xserv->cairo)
            {
                cairo_move_to(xserv->cairo, x1, y1);
                cairo_line_to(xserv->cairo, x2, y2);
                cairo_stroke(xserv->cairo);
                return;
            }
#endif
            XDrawLine(g_display, g_win, gc, x1, y1, x2, y2);
        };

        const auto drawMarker = [this, &g_display, &gc,
                                 &g_win](const draw_task::TMarkerInVectorInShape &marker,
                                         int vector_font_size) {
            static constexpr int kMarkerHalfSize = 4;
#ifdef WITH_CAIRO
            if (xserv->cairo)
            {
                xserv->cairoSetColor(marker.color);
                if (marker.IsCircle())
                {
                    cairo_arc(xserv->cairo, marker.x, marker.y, kMarkerHalfSize, 0, 2 * M_PI);
                    cairo_fill(xserv->cairo);
                }
                if (marker.IsCross())
                {
                    cairo_move_to(xserv->cairo, marker.x - kMarkerHalfSize,
                                  marker.y - kMarkerHalfSize);
                    cairo_line_to(xserv->cairo, marker.x + kMarkerHalfSize,
                                  marker.y + kMarkerHalfSize);

                    cairo_move_to(xserv->cairo, marker.x - kMarkerHalfSize,
                                  marker.y + kMarkerHalfSize);
                    cairo_line_to(xserv->cairo, marker.x + kMarkerHalfSize,
                                  marker.y - kMarkerHalfSize);

                    cairo_stroke(xserv->cairo);
                }
            }
            else
#endif
            {
                const auto marker_color = xserv->colors->get(marker.color);
                XSetForeground(g_display, gc, marker_color.pixel);
                if (marker.IsCircle())
                {
                    XDrawArc(g_display, g_win, gc, marker.x - kMarkerHalfSize,
                             marker.y - kMarkerHalfSize, 2 * kMarkerHalfSize, 2 * kMarkerHalfSize,
                             0, 360 * 64);
                }
                if (marker.IsCross())
                {
                    XDrawLine(g_display, g_win, gc, marker.x - kMarkerHalfSize,
                              marker.y - kMarkerHalfSize, marker.x + kMarkerHalfSize,
                              marker.y + kMarkerHalfSize);
                    XDrawLine(g_display, g_win, gc, marker.x - kMarkerHalfSize,
                              marker.y + kMarkerHalfSize, marker.x + kMarkerHalfSize,
                              marker.y - kMarkerHalfSize);
                }
            }
            // Common part with/without cairo.
            if (marker.HasText())
            {
                const auto font = vector_font_size > 0 ? xserv->getFont(vector_font_size)
                                                       : xserv->getFont("normal");
                xserv->drawUtf8String(font, marker.color, marker.x + 10, marker.y + 20, marker.text,
                                      ETextDecor::NoRectangle);
            }
        };

        const bool had_vec = draw_task::ForEachVectorPointsPair(drawitem, drawLine, drawMarker);
        if (!had_vec && drawitem.shape.shape == "rect")
        {
            // TODO: distinct fill/edge colour
#ifdef WITH_CAIRO
            if (xserv->cairo)
            {
                cairo_rectangle(xserv->cairo, drawitem.x, drawitem.y, drawitem.shape.w,
                                drawitem.shape.h);
                cairo_stroke(xserv->cairo);
                return;
            }
#endif
            XDrawRectangle(g_display, g_win, gc, drawitem.x, drawitem.y, drawitem.shape.w,
                           drawitem.shape.h);
        }
    }
}

std::string XOverlayOutput::getFocusedWindowBinaryPath() const
{
    const auto pid = xserv->getFocusedWindowPid();
    // std::cout << "Focused window: " << pid << std::endl;
    if (0 == pid)
    {
        return {};
    }
    return getBinaryPathForPid(pid);
}
