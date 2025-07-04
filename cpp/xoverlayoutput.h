#pragma once

#include "cm_ctors.h"
#include "layer_out.h"

#include <memory>
#include <string>

// Have to use such a trick, so this H file does not have too many includes
class XPrivateAccess;

class XOverlayOutput : public OutputLayer
{
  public:
    XOverlayOutput(const std::string &window_class, int window_xpos, int window_ypos,
                   int window_width, int window_height);
    static OutputLayer &get(const std::string &window_class, int window_xpos, int window_ypos,
                            int window_width, int window_height)
    {
        return getStaticObject<XOverlayOutput>(window_class, window_xpos, window_ypos, window_width,
                                               window_height);
    }
    NO_COPYMOVE(XOverlayOutput);
    ~XOverlayOutput() override;

    bool isTransparencyAvail() const override;
    void cleanFrame() override;
    void flushFrame() override;

    void showVersionString(const std::string &version, const std::string &color) override;
    void draw(const draw_task::drawitem_t &drawitem) override;
    [[nodiscard]]
    std::string getFocusedWindowBinaryPath() const override;

  private:
    void drawAsText(const draw_task::drawitem_t &drawitem);
    void drawAsShape(const draw_task::drawitem_t &drawitem);

    std::shared_ptr<XPrivateAccess> xserv{nullptr};
};
