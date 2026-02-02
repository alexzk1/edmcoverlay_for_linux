#pragma once

#include "drawables.h"
#include "runners.h"

#include <memory>
#include <mutex>
#include <utility>

/// @brief Ouput context usable by TCP Session to provide data to processing core.
class OutputContext
{
  public:
    OutputContext(std::shared_ptr<std::mutex> mut, draw_task::draw_items_t &allDraws) :
        mut(std::move(mut)),
        allDraws(allDraws)
    {
    }

    template <typename taCallable>
    void accessContext(const taCallable &callable)
    {
        const std::lock_guard grd(*mut);
        callable(allDraws);
    }

  private:
    std::shared_ptr<std::mutex> mut;
    draw_task::draw_items_t &allDraws;
};

/// @brief Logic context for TCP accept and TCP session, allows to parse incoming data properly and
/// stop processing fast when program closed.
struct LogicContext
{
    int window_width;
    int window_height;
    OutputContext outputContext;
    utility::runnerint_t shouldStop;

    /// @returns true if thread can continue, @returns false when all processing must be stoped now.
    [[nodiscard]]
    bool canContinue() const
    {
        return !(*shouldStop);
    }
};
