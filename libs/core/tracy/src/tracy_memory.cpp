//  Copyright (c) 2026 Arivoli Ramamoorthy
//
//  SPDX-License-Identifier: BSL-1.0
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

// Global operator new/delete overrides for Tracy memory tracking.
// Only compiled when HPX_TRACY_WITH_MEMORY_TRACKING=ON.

#include <hpx/config.hpp>

#if defined(HPX_HAVE_MODULE_TRACY)

#include <cstdlib>
#include <new>

#include <tracy/Tracy.hpp>

void* operator new(std::size_t size)
{
    void* ptr = std::malloc(size);
    if (!ptr)
        throw std::bad_alloc{};
    TracyAlloc(ptr, size);
    return ptr;
}

void* operator new[](std::size_t size)
{
    void* ptr = std::malloc(size);
    if (!ptr)
        throw std::bad_alloc{};
    TracyAlloc(ptr, size);
    return ptr;
}

void operator delete(void* ptr) noexcept
{
    TracyFree(ptr);
    std::free(ptr);
}

void operator delete[](void* ptr) noexcept
{
    TracyFree(ptr);
    std::free(ptr);
}

void operator delete(void* ptr, std::size_t) noexcept
{
    TracyFree(ptr);
    std::free(ptr);
}

void operator delete[](void* ptr, std::size_t) noexcept
{
    TracyFree(ptr);
    std::free(ptr);
}

void* operator new(std::size_t size, std::nothrow_t const&) noexcept
{
    void* ptr = std::malloc(size);
    if (ptr)
        TracyAlloc(ptr, size);
    return ptr;
}

void* operator new[](std::size_t size, std::nothrow_t const&) noexcept
{
    void* ptr = std::malloc(size);
    if (ptr)
        TracyAlloc(ptr, size);
    return ptr;
}

void operator delete(void* ptr, std::nothrow_t const&) noexcept
{
    TracyFree(ptr);
    std::free(ptr);
}

void operator delete[](void* ptr, std::nothrow_t const&) noexcept
{
    TracyFree(ptr);
    std::free(ptr);
}

#endif
