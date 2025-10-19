#pragma once

#include "cm_ctors.h"

#include <functional>
#include <utility>

/// @brief Usable to manage RAII X resources, like TManagedId<Picture>
template <typename taIdType, taIdType taNone>
class TManagedId
{
  public:
    using TIdType = taIdType;
    MOVEONLY_ALLOWED(TManagedId);
    TManagedId() = default;

    TManagedId(taIdType id_, std::function<void(taIdType)> free_) :
        id_(id_),
        free_(std::move(free_))
    {
    }

    ~TManagedId()
    {
        reset();
    }

    [[nodiscard]]
    bool IsInitialized() const
    {
        return id_ != None;
    }

    // NOLINTNEXTLINE
    operator taIdType() const
    {
        return id_;
    }

    void reset()
    {
        if (id_ != taNone && free_)
        {
            free_(id_);
            id_ = taNone;
        }
    }

  private:
    taIdType id_{taNone};
    std::function<void(taIdType)> free_;
};
