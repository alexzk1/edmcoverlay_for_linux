#pragma once

#include <cstdint>
#include <iosfwd>
#include <iostream>
#include <fstream>
#include <string>

#include "drawables.h"
#include "cm_ctors.h"
#include "strutils.h"

//virtual base class (interface) for doing output
class OutputLayer
{
protected:
    OutputLayer() = default;
public:
    NO_COPYMOVE(OutputLayer); //we don't want devices be copyable
    virtual ~OutputLayer() = default; //making sure we can proper delete children

    virtual void cleanFrame() = 0;
    virtual void flushFrame() = 0;
    [[nodiscard]]
    virtual std::string getFocusedWindowBinaryPath() const = 0;
    virtual void showVersionString(const std::string& src, const std::string& color) = 0;
    virtual void draw(const draw_task::drawitem_t& drawitem) = 0;
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
        catch (std::ios_base::failure& e)
        {
            std::cerr << e.what() <<std::endl;
            return {};
        }
        return {fulls.c_str()};
    }
};

//https://habr.com/ru/post/242639/
template <typename T, typename... Args>
inline T& getStaticObject(Args&&... args)
{
    static T obj(std::forward<Args>(args)...);
    return obj;
}
