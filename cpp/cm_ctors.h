#pragma once
#include <memory.h>

//test must be used OUTSIDE class definition like
/*
    class A{};
    TEST_MOVE_NOEX(A);
*/

//NOLINTNEXTLINE
#define TEST_MOVE_NOEX(TYPE) static_assert(std::is_nothrow_move_constructible_v<TYPE> && std::is_nothrow_move_assignable_v<TYPE>, " Should be noexcept Moves.")

//NOLINTNEXTLINE
#define NO_COPYMOVE(TYPE) TYPE(const TYPE&) = delete;TYPE(TYPE&&) = delete;TYPE& operator=(const TYPE&) = delete;TYPE& operator=(TYPE&&) = delete

//NOLINTNEXTLINE
#define DEFAULT_COPYMOVE(TYPE) TYPE(const TYPE&) = default;TYPE(TYPE&&) = default; \
    TYPE& operator=(const TYPE&) = default;TYPE& operator=(TYPE&&) = default //NOLINT

//NOLINTNEXTLINE
#define MOVEONLY_ALLOWED(TYPE) TYPE(const TYPE&) = delete;TYPE(TYPE&&) = default;TYPE& operator=(const TYPE&) = delete; \
    TYPE& operator=(TYPE&&) = default //NOLINT

//only stack allocation allowed
#define STACK_ONLY static void *operator new (size_t) = delete; \
    static void *operator new[] (size_t) = delete

//sort of extern constructor to keep C-structures initialized
template <class T>
T allocCType()
{
    T r;
    memset(&r, 0, sizeof(T));
    return r;
}
