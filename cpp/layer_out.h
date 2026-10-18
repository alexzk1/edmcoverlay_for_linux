#pragma once

#include "cm_ctors.h"
#include "drawables.h"
#include "strutils.h"

#include <cstdint>
#include <fstream>
#include <iosfwd>
#include <iostream>
#include <string>

// virtual base class (interface) for doing output
class OutputLayer
{
  protected:
    OutputLayer() = default;

  public:
    virtual ~OutputLayer() = default; // making sure we can proper delete children
    NO_COPYMOVE(OutputLayer);         // we don't want devices be copyable

    virtual void cleanFrame() = 0;
    virtual void flushFrame() = 0;
    [[nodiscard]]
    virtual std::string getFocusedWindowBinaryPath() const = 0;
    virtual void showVersionString(const std::string &src, const std::string &color) = 0;
    virtual void draw(const draw_task::drawitem_t &drawitem) = 0;
    [[nodiscard]]
    virtual bool isTransparencyAvail() const = 0;

    static std::string getBinaryPathForPid(const std::uint64_t pid)
    {
        std::string fulls;
        std::fstream io(utility::string_sprintf("/proc/%zu/cmdline", pid),
                        std::ios_base::in | std::ios_base::binary);
        try
        {
            fulls = utility::read_stream_into_container(io);
        }
        catch (std::ios_base::failure &e)
        {
            std::cerr << e.what() << std::endl;
            return {};
        }
        return {fulls.c_str()};
    }

    template <typename taType, typename... taArgs>
    static std::unique_ptr<OutputLayer> allocate(taArgs &&...args)
    {
        static_assert(std::is_base_of_v<OutputLayer, taType>,
                      "Children of OutputLayer are allowed.");
        return std::make_unique<taType>(std::forward<taArgs>(args)...);
    }
};
