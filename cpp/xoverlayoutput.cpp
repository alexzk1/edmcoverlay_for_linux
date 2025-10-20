#include "xoverlayoutput.h"

#include "cm_ctors.h"
#include "drawables.h"
#include "luna_default_fonts.h"
#include "managed_id.hpp"
#include "opaque_ptr.h"
#include "svgbuilder.h"
#include "x11_colors_mgr.h"

#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/Xfixes.h>
#include <X11/extensions/Xrender.h>
#include <X11/extensions/shape.h>
#include <X11/extensions/shapeconst.h>

#include <lunasvg.h>

#include <cassert>
#include <cstdint>
#include <cstring>
#include <functional>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>

namespace {

using TManagedPixmap = TManagedId<Pixmap, None>;

// Events for normal windows
// NOLINTNEXTLINE
constexpr long BASIC_EVENT_MASK = StructureNotifyMask | ExposureMask | PropertyChangeMask
                                  | EnterWindowMask | LeaveWindowMask | KeyPressMask
                                  | KeyReleaseMask | KeymapStateMask;

// NOLINTNEXTLINE
constexpr long NOT_PROPAGATE_MASK = KeyPressMask | KeyReleaseMask | ButtonPressMask
                                    | ButtonReleaseMask | PointerMotionMask | ButtonMotionMask;

enum class ETextDecor : std::uint8_t {
    NoRectangle,
    Rectangle,
};

int XMyDestroyImage(XImage *ptr)
{
    // This is macro..and we need a function.
    return XDestroyImage(ptr);
}

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
// Yes, do not include class XPrivateAccess because it is mentioned in the header.
} // namespace

/// @brief Real tasks to do on X server, it is private, so can be replaced by Wayland.
class XPrivateAccess
{
  private:
    // Make sure it is 1st field, so it is cleared last in dtor.
    struct TXInitFreeCaller;
    std::unique_ptr<TXInitFreeCaller> initializer;

  public:
    const int window_xpos;
    const int window_ypos;
    const int window_width;
    const int window_height;

  private:
    // I think, text should existis while window exists.
    CWindowClass window_class;

    XVisualInfo g_vinfo{allocCType<XVisualInfo>()};

    std::shared_ptr<MyXOverlayColorMap> colors{nullptr};

    opaque_ptr<Display> g_display{nullptr};
    Window g_root{0};
    int g_screen{0};
    Window g_win{0};
    opaque_ptr<_XGC> single_gc{nullptr};
    TManagedId<Picture, None> g_windowOpaqueDestination;

  public:
    ///@brief Allocates RAII style memory.
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

    ///@brief Does not allocate memory, but returns some ID which should be RAII styled.
    template <typename taIdType, typename taDeAllocator, typename taAllocator,
              typename... taAllocArgs>
    TManagedId<taIdType, None> AllocateId(taDeAllocator aDeallocate, taAllocator aAllocate,
                                          taAllocArgs &&...aArgs) const
    {
        static_assert(std::is_trivial_v<taIdType>, "Only simple ID types are allowed.");
        return TManagedId<taIdType, None>{aAllocate(std::forward<taAllocArgs>(aArgs)...),
                                          [aDeallocate, this](taIdType id) {
                                              aDeallocate(g_display, id);
                                          }};
    }

    XPrivateAccess() = delete;
    NO_COPYMOVE(XPrivateAccess);

    // NOLINTNEXTLINE
    XPrivateAccess(const std::string &window_class, int window_xpos, int window_ypos,
                   int window_width, int window_height) :
        initializer(std::make_unique<TXInitFreeCaller>()),
        window_xpos(window_xpos),
        window_ypos(window_ypos),
        window_width(window_width),
        window_height(window_height),
        window_class(window_class)
    {
        openDisplay();
        createShapedWindow();

        single_gc = Allocate<_XGC>(XFreeGC, XCreateGC, g_display, g_win, 0, nullptr);
        colors = std::make_shared<MyXOverlayColorMap>(g_display, getAttributes());
    }

    ~XPrivateAccess()
    {
        colors.reset();
        g_windowOpaqueDestination.reset();
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

    ///@returns PID of the current process with window which has focus.
    ///@note X11 does not support 64 bit pids.
    std::uint32_t getFocusedWindowPid() const
    {
        Window focused = 0;
        int revert_to = 0;
        XGetInputFocus(g_display, &focused, &revert_to);
        return getWindowPropertyInt<std::uint32_t>("_NET_WM_PID", focused);
    }

    ///@brief Draws SVG file on the screen.
    void drawAsSvg(const draw_task::drawitem_t &drawitem)
    {
        assert(drawitem.drawmode == draw_task::drawmode_t::svg);
        InstallNormalFontFileToLuna(drawitem.svg.fontFile);

        if (!drawitem.svg.render)
        {
            auto pixmap = RenderXPixmapFromSvgText(drawitem.svg.svg, drawitem.svg.css);
            if (!std::get<0>(pixmap).IsInitialized())
            {
                return;
            }
            auto renderer = [this,
                             shared_pixmap = std::make_shared<TPixmapWithDims>(std::move(pixmap)),
                             &drawitem]() {
                const auto &[pixmap_id, pixmap_width, pixmap_height] = *shared_pixmap;
                XRenderPictFormat *pictFormat = XRenderFindVisualFormat(g_display, g_vinfo.visual);
                auto srcPict = AllocateId<Picture>(XRenderFreePicture, XRenderCreatePicture,
                                                   g_display, pixmap_id, pictFormat, 0, nullptr);
                if (!g_windowOpaqueDestination.IsInitialized())
                {
                    g_windowOpaqueDestination =
                      AllocateId<Picture>(XRenderFreePicture, XRenderCreatePicture, g_display,
                                          g_win, pictFormat, 0, nullptr);
                }
                XRenderComposite(g_display, PictOpOver, srcPict, None, g_windowOpaqueDestination, 0,
                                 0, 0, 0, drawitem.x, drawitem.y, pixmap_width, pixmap_height);
            };
            drawitem.svg.render = std::move(renderer);
        }
        assert(drawitem.svg.render);
        if (!drawitem.svg.render)
        {
            std::cerr << "SVG renderer was not set. It should not happen.\n";
            return;
        }
        drawitem.svg.render();
    }

  private:
    using TPixmapWithDims = std::tuple<TManagedPixmap, int, int>;
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

    TPixmapWithDims RenderXPixmapFromSvgText(const std::string &svg, const std::string &css) const
    {
        try
        {
            using namespace lunasvg;
            if (svg.empty())
            {
                throw std::runtime_error("Empty SVG was provided.");
            }

            auto document = Document::loadFromData(svg);
            if (!css.empty())
            {
                document->applyStyleSheet(css);
            }

            auto bitmap = document->renderToBitmap();
            if (bitmap.isNull())
            {
                std::cerr << "Failed to render SVG." << std::endl;
                return std::make_tuple(TManagedPixmap{}, 0, 0);
            }

            constexpr int kBitnessWithAlpha = 32;
            auto pixmap = AllocateId<Pixmap>(XFreePixmap, XCreatePixmap, g_display, g_win,
                                             bitmap.width(), bitmap.height(), kBitnessWithAlpha);
            auto ximage = AllocateOpaque<XImage>(
              XMyDestroyImage, XCreateImage, g_display, g_vinfo.visual, kBitnessWithAlpha, ZPixmap,
              0, reinterpret_cast<char *>(bitmap.data()), bitmap.width(), bitmap.height(),
              kBitnessWithAlpha, 0);
            if (!ximage)
            {
                std::cerr << "Failed to create XImage during SVG render." << std::endl;
                return std::make_tuple(TManagedPixmap{}, 0, 0);
            }

            XPutImage(g_display, pixmap, single_gc, ximage, 0, 0, 0, 0, bitmap.width(),
                      bitmap.height());
            // Important:stopping freeing lunasvg memory.
            ximage->data = nullptr;

            return std::make_tuple(std::move(pixmap), bitmap.width(), bitmap.height());
        }
        catch (std::exception &e)
        {
            std::cerr << e.what() << "\n" << svg << std::endl;
        }
        catch (...)
        {
            std::cerr << "Something unknown happened while rendering SVG:\n" << svg << std::endl;
        }
        return TPixmapWithDims{TManagedPixmap{}, 0, 0};
    }

    void openDisplay()
    {
        // Member method Allocate() uses this g_display, so we do it direct here.
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
        XMatchVisualInfo(g_display, g_screen, 32, TrueColor, &g_vinfo);

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
        attr.colormap = XCreateColormap(g_display, g_root, g_vinfo.visual, AllocNone);

        // NOLINTNEXTLINE
        const unsigned long mask = CWColormap | CWBorderPixel | CWBackPixel | CWEventMask
                                   | CWWinGravity | CWBitGravity | CWSaveUnder | CWDontPropagate
                                   | CWOverrideRedirect;

        g_win =
          XCreateWindow(g_display, g_root, window_xpos, window_ypos, window_width, window_height, 0,
                        g_vinfo.depth, InputOutput, g_vinfo.visual, mask, &attr);
        window_class.Set(g_display, g_win);
        std::cout << "WMID: " << g_win << std::endl;

        XShapeCombineMask(g_display, g_win, ShapeInput, 0, 0, None, ShapeSet);

        // We want shape-changed event too
        XShapeSelectInput(g_display, g_win, ShapeNotifyMask);

        // Tell the Window Manager not to draw window borders (frame) or title.
        auto wattr = allocCType<XSetWindowAttributes>();
        wattr.override_redirect = 1;
        XChangeWindowAttributes(g_display, g_win, CWOverrideRedirect, &wattr);

        // Pass through input.
        const XserverRegion region = XFixesCreateRegion(g_display, nullptr, 0);
        // XFixesSetWindowShapeRegion (g_display, w, ShapeBounding, 0, 0, 0);
        XFixesSetWindowShapeRegion(g_display, g_win, ShapeInput, 0, 0, region);
        XFixesDestroyRegion(g_display, region);

        // Show the window
        XMapWindow(g_display, g_win);
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
            std::memcpy(&tmp, ptr, sizeof(tmp));
        }

        return tmp;
    }

    std::string getWindowPropertyString(const char *const aPropertyName, const Window aWindow) const
    {
        const auto ptr = getWindowPropertyAny(aPropertyName, aWindow);
        // NOLINTNEXTLINE
        return {reinterpret_cast<const char *const>(ptr)};
    }
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
    draw_task::drawitem_t task;
    task.drawmode = draw_task::drawmode_t::text;
    task.color = color;
    task.text.fontSize = 16;
    task.text.text = version;
    task.x = 10;
    task.y = 10;
    xserv->drawAsSvg(
      SvgBuilder(xserv->window_width, xserv->window_height, {}, task).BuildSvgTask());
}

void XOverlayOutput::draw(const draw_task::drawitem_t &drawitem)
{
    switch (drawitem.drawmode)
    {
        case draw_task::drawmode_t::svg:
            xserv->drawAsSvg(drawitem);
            break;
        case draw_task::drawmode_t::idk:
            break;
        default:
            assert(false && "Unhandled case.");
            break;
    }
}

std::string XOverlayOutput::getFocusedWindowBinaryPath() const
{
    const auto pid = xserv->getFocusedWindowPid();
    if (0 == pid)
    {
        return {};
    }
    return getBinaryPathForPid(pid);
}
