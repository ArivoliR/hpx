//  Copyright (c) 2026 Hartmut Kaiser
//
//  SPDX-License-Identifier: BSL-1.0
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <hpx/config.hpp>

#if defined(HPX_HAVE_MODULE_TRACY)
#include <hpx/tracy/tracy.hpp>

#include <atomic>
#include <cstdint>
#include <string>

#include <tracy/Tracy.hpp>

namespace hpx::tracy {

    namespace {
        constexpr std::uint32_t thread_palette[] = {
            0xe6194b, 0x3cb44b, 0x4363d8, 0xf58231,
            0x911eb4, 0x42d4f4, 0xf032e6, 0xbfef45,
            0xfabed4, 0x469990, 0xdcbeff, 0x9a6324,
        };
        constexpr int palette_size =
            static_cast<int>(sizeof(thread_palette) / sizeof(thread_palette[0]));

        std::atomic<int> thread_counter{0};
        thread_local int thread_color_index = -1;
    }    // namespace

    void set_thread_name(char const* name) noexcept
    {
        ::tracy::SetThreadName(name);
        if (thread_color_index < 0)
            thread_color_index = thread_counter.fetch_add(1) % palette_size;
        TracySetThreadColor(thread_palette[thread_color_index]);
    }

    namespace {
        // Guards against TRACY_ON_DEMAND race: if the profiler connects after
        // enter_fiber (which was a no-op), leave_fiber must also be a no-op.
        // Without this, Tracy receives FiberLeave with no matching FiberEnter.
        thread_local bool fiber_active = false;
    }    // namespace

    void enter_fiber(char const* name) noexcept
    {
        bool const connected = ::tracy::GetProfiler().IsConnected();
        if (connected)
            ::TracyFiberEnter(name);
        fiber_active = connected;
    }

    void leave_fiber() noexcept
    {
        if (fiber_active)
        {
            ::TracyFiberLeave;
            fiber_active = false;
        }
    }

    void frame_mark_start(char const* name) noexcept
    {
        ::tracy::Profiler::SendFrameMark(name, ::tracy::QueueType::FrameMarkMsgStart);
    }

    void frame_mark_end(char const* name) noexcept
    {
        ::tracy::Profiler::SendFrameMark(name, ::tracy::QueueType::FrameMarkMsgEnd);
    }

    void create_counter(std::string const& name) noexcept
    {
        ::TracyPlotConfig(
            name.c_str(), ::tracy::PlotFormatType::Number, true, false, 0);
    }

    void sample_value(std::string const& name, double const value) noexcept
    {
        ::TracyPlot(name.c_str(), value);
    }
}    // namespace hpx::tracy

#endif
