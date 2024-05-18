#include "xoverlayoutput.h"
#include "colors_mgr.h"
#include <X11/Xos.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/extensions/shape.h>
#include <X11/extensions/Xcomposite.h>
#include <X11/extensions/Xfixes.h>
#include <X11/Xft/Xft.h>

#include <cassert>
#include <memory.h>
#include <iostream>
#include <string>
#include <sstream>
#include <stdexcept>
#include <mutex>
#include <vector>
#include <unordered_map>


// Events for normal windows
constexpr static long BASIC_EVENT_MASK = StructureNotifyMask | ExposureMask | PropertyChangeMask |
    EnterWindowMask | LeaveWindowMask | KeyPressMask | KeyReleaseMask | KeymapStateMask;

constexpr static long NOT_PROPAGATE_MASK = KeyPressMask | KeyReleaseMask | ButtonPressMask |
    ButtonReleaseMask | PointerMotionMask | ButtonMotionMask;

constexpr static long event_mask = StructureNotifyMask | ExposureMask | PropertyChangeMask |
                                   EnterWindowMask |
                                   LeaveWindowMask | KeyRelease | ButtonPress | ButtonRelease | KeymapStateMask;

class XPrivateAccess
{
public:

    template <typename T, typename taDeAllocator, typename taAllocator, typename ...taAllocArgs>
    auto Allocate(taDeAllocator aDeallocate, taAllocator aAllocate, taAllocArgs&& ...aArgs) const
    {
        return AllocateOpaque<T>([this, dealloc = std::forward<taDeAllocator>(aDeallocate)](auto *ptr)
        {
            if (ptr)
            {
                dealloc(g_display, ptr);
                flush();
            }
        }, std::forward<taAllocator>(aAllocate), std::forward<taAllocArgs>(aArgs)...);
    }


    XPrivateAccess() = delete;
    XPrivateAccess(int window_xpos, int window_ypos, int window_width, int window_height):
        window_xpos(window_xpos), window_ypos(window_ypos),
        window_width(window_width), window_height(window_height)
    {
        XInitThreads();
        XftInit(nullptr);

        openDisplay();
        createShapedWindow();

        single_gc = allocGlobGC();

        colors.reset(new MyXOverlayColorMap(g_display, g_screen, getAttributes()));
    }

    ~XPrivateAccess()
    {
        loadedFonts.clear();
        colors.reset();
        single_gc.reset();
        g_display.reset();
    }

    void cleanGC(GC gc) const
    {
        if (gc)
        {
            const auto& white = colors->get("solid_white");
            const auto& transparent = colors->get("transparent");
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

    const opaque_ptr<XftFont>& getFont(int size)
    {
        if (loadedFonts.find(size) == loadedFonts.end())
        {
            loadedFonts[size] = allocFont(size);
        }
        return loadedFonts.at(size);
    }

    const opaque_ptr<XftFont>& getFont(const std::string& sizeText)
    {
        //large = normal + kDeltaFontDifference
        constexpr int kDeltaFontDifference = 4;
        constexpr int kNormalFontSize      = 16;

        //FYI: I set those big numbers for my eyes with glasses. Somebody may want lower / bigger.
        //From the other side, existing plugins send fixed distance between strings.
        //This one looks okish for Canon's.

        return sizeText == "large" ? getFont(kNormalFontSize + kDeltaFontDifference)
               : getFont(kNormalFontSize);
    }
private:
    std::unordered_map<int, opaque_ptr<XftFont>> loadedFonts;

    opaque_ptr<_XGC> allocGlobGC() const
    {
        return Allocate<_XGC>(XFreeGC, XCreateGC, g_display, g_win, 0, nullptr);
    }

    //FYI: configurable font's families below.
    opaque_ptr<XftFont> allocFont(const int aFontSize) const
    {
        const auto fontString = [&aFontSize](const std::string& family)
        {
            std::stringstream ss;
            ss << family;
            ss << ":size=" << aFontSize;
            ss << ":antialias=true";
            return ss.str();
        };

        static const std::vector<std::string> kFontsToTry =
        {
            "Liberation Mono",
            "DejaVu Sans Mono",
            "Source Code Pro",
            "Ubuntu Mono",
            "Monospace",
            "Sans Serif",
            "Liberation Serif",
            "Serif",
            "Liberation Sans",
            "Open Sans",
        };

        opaque_ptr<XftFont> font;

        for (const auto& family: kFontsToTry)
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
        g_display = std::shared_ptr<Display>(XOpenDisplay(0), [](Display * p)
        {
            if (p)
            {
                XCloseDisplay(p);
            }
        });

        if (!g_display)
        {
            std::cerr << "Failed to open X display" << std::endl;
            exit(-1);
        }

        Atom cmAtom = XInternAtom(g_display, "_NET_WM_CM_S0", 0);
        Window cmOwner = XGetSelectionOwner(g_display, cmAtom);
        if (!cmOwner)
        {
            std::cerr << "Composite manager is absent." << std::endl;
            std::cerr << "Please check instructions: https://wiki.archlinux.org/index.php/Xcompmgr" <<
                      std::endl;
            exit(-1);
        }
        g_screen    = DefaultScreen(g_display);

        // Has shape extions?
        int     shape_event_base;
        int     shape_error_base;

        if (!XShapeQueryExtension (g_display, &shape_event_base, &shape_error_base))
        {
            std::cerr << "NO shape extension in your system !" << std::endl;
            exit (-1);
        }

        g_root    = DefaultRootWindow(g_display);
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

        //unsigned long mask = CWBackPixel|CWBorderPixel|CWWinGravity|CWBitGravity|CWSaveUnder|CWEventMask|CWDontPropagate|CWOverrideRedirect;
        unsigned long mask = CWColormap | CWBorderPixel | CWBackPixel | CWEventMask | CWWinGravity |
                             CWBitGravity | CWSaveUnder | CWDontPropagate | CWOverrideRedirect;

        g_win = XCreateWindow(g_display, g_root, window_xpos, window_ypos, window_width, window_height, 0,
                              vinfo.depth, InputOutput, vinfo.visual, mask, &attr);

        /* g_bitmap = XCreateBitmapFromData (g_display, RootWindow(g_display, g_screen), (char *)myshape_bits, myshape_width, myshape_height); */

        //XShapeCombineMask(g_display, g_win, ShapeBounding, 900, 500, g_bitmap, ShapeSet);
        XShapeCombineMask(g_display, g_win, ShapeInput, 0, 0, None, ShapeSet );

        // We want shape-changed event too
        XShapeSelectInput (g_display, g_win, ShapeNotifyMask);

        // Tell the Window Manager not to draw window borders (frame) or title.
        auto wattr = allocCType<XSetWindowAttributes>();
        wattr.override_redirect = 1;
        XChangeWindowAttributes(g_display, g_win, CWOverrideRedirect, &wattr);


        //pass through input
        XserverRegion region = XFixesCreateRegion (g_display, NULL, 0);
        //XFixesSetWindowShapeRegion (g_display, w, ShapeBounding, 0, 0, 0);
        XFixesSetWindowShapeRegion (g_display, g_win, ShapeInput, 0, 0, region);
        XFixesDestroyRegion (g_display, region);

        // Show the window
        XMapWindow(g_display, g_win);
    }

    XWindowAttributes getAttributes() const
    {
        auto attrs = allocCType<XWindowAttributes>();
        XGetWindowAttributes(g_display, g_win, &attrs);
        return attrs;
    }

    const unsigned char* const getWindowPropertyAny(const char* const aPropertyName,
            const Window aWindow) const
    {
        constexpr int kMaximumReturnedCountOf32Bits = 1024;
        if (aWindow == 0)
        {
            return nullptr;
        }
        Atom actual_type;
        int actual_format;
        unsigned long nitems, bytes_after;
        unsigned char *prop = nullptr;

        const auto filter_atom = XInternAtom(g_display, aPropertyName, True);
        const auto status = XGetWindowProperty(g_display, aWindow, filter_atom, 0,
                                               kMaximumReturnedCountOf32Bits, False,
                                               AnyPropertyType,
                                               &actual_type, &actual_format, &nitems, &bytes_after, &prop);

        if (status != Success)
        {
            std::cerr << "XGetWindowProperty failed with status: " << status <<"."<< std::endl;
        }

        return prop;
    }

    template <typename ExpectedType, typename =
              typename std::enable_if<std::is_integral<ExpectedType>::value>::type>
    ExpectedType getWindowPropertyInt(const char* const aPropertyName,
                                      const Window aWindow) const
    {
        constexpr auto kSize = sizeof(ExpectedType);
        static_assert(kSize == 1 || kSize == 2
                      || kSize == 4, "Only 8/16/32 bits are supported by XGetWindowProperty()");

        ExpectedType tmp{0};
        const auto ptr = getWindowPropertyAny(aPropertyName, aWindow);
        if (ptr)
        {
            memcpy(&tmp, ptr, sizeof(tmp));
        }

        return tmp;
    }

    std::string getWindowPropertyString(const char* const aPropertyName,
                                        const Window aWindow) const
    {
        const auto ptr = getWindowPropertyAny(aPropertyName, aWindow);
        return std::string(reinterpret_cast<const char* const>(ptr));
    }
public:
    const int window_xpos;
    const int window_ypos;
    const int window_width;
    const int window_height;
    std::shared_ptr<MyXOverlayColorMap> colors{nullptr};

    opaque_ptr<Display> g_display{nullptr};
    Window              g_root{0};
    int                 g_screen{0};
    Window              g_win{0};
    opaque_ptr<_XGC>    single_gc{nullptr};

    static int utf8CharactersCount(const std::string& str)
    {
        int count = 0;
        for (const auto& c : str)
        {
            if ((c & 0x80) == 0 || (c & 0xc0) == 0xc0)
            {
                ++count;
            }
        }
        return count;
    }

    void drawUtf8String(const opaque_ptr<XftFont>& aFont, const std::string& aColor,
                        int aX, int aY,
                        const std::string& aString, bool aRectangle) const
    {
        //https://github.com/jsynacek/xft-example/blob/master/main.c

        const int length = aString.length();
        const FcChar8* str = reinterpret_cast<const FcChar8*>(aString.c_str());

        if (!aFont)
        {
            throw std::runtime_error("Tried to draw string without font!!!");
        }

        if (aRectangle)
        {
            XGlyphInfo extents;
            XftTextExtentsUtf8 (g_display,
                                aFont,
                                str,
                                length,
                                &extents);

            const auto& black = colors->get("black");
            XSetForeground(g_display, single_gc, black.pixel);
            XFillRectangle(g_display, g_win, single_gc, aX, aY,
                           extents.width, extents.height);
        }

        auto color = colors->getFontColor(aColor);
        if (!color)
        {
            throw std::runtime_error("Tried to draw string without color!!!");
        }
        const auto attrs = getAttributes();
        auto draw = AllocateOpaque<XftDraw>(XftDrawDestroy, XftDrawCreate, g_display, g_win,
                                            attrs.visual,
                                            attrs.colormap);
        assert(XftDrawColormap(draw) == attrs.colormap);
        assert(XftDrawVisual(draw)   == attrs.visual);

        //XftDrawRect(draw, color, 50, 50, 250, 250);
        XftDrawStringUtf8(draw, color, aFont, aX, aY + aFont->ascent, str, length);
    }

    //X11 does not support 64 bit pids.
    std::uint32_t getFocusedWindowPid() const
    {
        Window focused = 0;
        int revert_to = 0;
        XGetInputFocus(g_display, &focused, &revert_to);
        return getWindowPropertyInt<std::uint32_t>("_NET_WM_PID", focused);
    }
};

//**********************************************************************************************************************
//*****************************XOverlayOutput***************************************************************************
//**********************************************************************************************************************

XOverlayOutput::XOverlayOutput(int window_xpos, int window_ypos, int window_width,
                               int window_height):
    xserv(new XPrivateAccess(window_xpos, window_ypos, window_width, window_height))
{
    xserv->cleanGC();
}

XOverlayOutput::~XOverlayOutput()
{
    xserv.reset();
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
    xserv->drawUtf8String(xserv->getFont(12), color, 0, 0, version, true);
}

void XOverlayOutput::draw(const draw_task::drawitem_t &drawitem)
{
    const auto& gc = xserv->single_gc;
    const auto& g_display = xserv->g_display;
    const auto& g_win = xserv->g_win;

    if (drawitem.drawmode == draw_task::drawmode_t::text)
    {
        const auto& font = drawitem.text.fontSize ? xserv->getFont(*drawitem.text.fontSize)
                           : xserv->getFont(drawitem.text.size);
        xserv->drawUtf8String(font, drawitem.color, drawitem.x, drawitem.y, drawitem.text.text, false);
    }
    else
    {
        XSetForeground(g_display, gc, xserv->colors->get(drawitem.color).pixel);

        const bool had_vec = draw_task::ForEachVectorPointsPair(drawitem, [&](int x1, int y1, int x2,
                             int y2)
        {
            XDrawLine(g_display, g_win, gc, x1, y1, x2, y2);
        });

        if (!had_vec && drawitem.shape.shape == "rect")
        {
            // TODO distinct fill/edge colour
            XDrawRectangle(g_display, g_win, gc, drawitem.x,
                           drawitem.y, drawitem.shape.w, drawitem.shape.h);
        }
    }
}

std::string XOverlayOutput::getFocusedWindowBinaryPath() const
{
    const auto pid = xserv->getFocusedWindowPid();
    //std::cout << "Focused window: " << pid << std::endl;
    if (0 == pid)
    {
        return {};
    }
    return getBinaryPathForPid(pid);
}
