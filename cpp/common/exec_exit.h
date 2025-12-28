#pragma once

#include <utility>

/// @brief Execs callable when goes out of scope.
template <typename taCallable>
class exec_onexit final
{
  public:
    exec_onexit() = delete;
    exec_onexit(const exec_onexit &) = delete;
    exec_onexit(exec_onexit &&) = default;
    exec_onexit &operator=(const exec_onexit &) = delete;
    exec_onexit &operator=(exec_onexit &&) = default;

    explicit exec_onexit(taCallable &&func) noexcept :
        func(std::move(func))
    {
    }

    ~exec_onexit()
    {
        func();
    }

  private:
    taCallable func;
};
