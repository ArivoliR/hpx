//  Copyright (c) 2026 Hartmut Kaiser
//
//  SPDX-License-Identifier: BSL-1.0
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <hpx/config.hpp>

#if defined(HPX_HAVE_MODULE_TRACY)
#include <hpx/tracy/tracy.hpp>

#include <string>

#include <tracy/Tracy.hpp>

namespace hpx::tracy {

    void set_thread_name(char const* name) noexcept
    {
        ::tracy::SetThreadName(name);
    }

    namespace {
        // Guards against TRACY_ON_DEMAND race: if the profiler connects after
        // enter_fiber (which was a no-op), leave_fiber must also be a no-op.
        // Without this, Tracy receives FiberLeave with no matching FiberEnter.
        thread_local bool fiber_active = false;
    }    // namespace

    // Expose Tracy fibers support
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

    // Create a new plot in Tracy
    void create_counter(std::string const& name) noexcept
    {
        ::TracyPlotConfig(
            name.c_str(), ::tracy::PlotFormatType::Number, true, false, 0);
    }

    // Pass a plot value to Tracy
    void sample_value(std::string const& name, double const value) noexcept
    {
        ::TracyPlot(name.c_str(), value);
    }
}    // namespace hpx::tracy

#endif
