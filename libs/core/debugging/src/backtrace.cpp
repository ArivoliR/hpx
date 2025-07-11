//  Copyright (c) 2011 Bryce Lelbach
//  Copyright (c) 2011-2025 Hartmut Kaiser
//  Copyright (c) 2010 Artyom Beilis (Tonkikh)
//
//  SPDX-License-Identifier: BSL-1.0
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

#include <hpx/config.hpp>

#if defined(HPX_HAVE_STACKTRACES)

#include <hpx/debugging/backtrace/backtrace.hpp>

#if (defined(__linux) || defined(__APPLE__) || defined(__sun)) &&              \
    (!defined(__ANDROID__) || !defined(ANDROID))
#if defined(__GLIBC__)
#define HPX_HAVE_EXECINFO
#endif
#define HPX_HAVE_DLFCN
#if defined(__GNUC__) && !defined(__clang__)
#define HPX_HAVE_UNWIND
#endif
#endif

#if defined(__GNUC__) && !defined(__bgq__)
#define HPX_HAVE_ABI_CXA_DEMANGLE
#endif

#ifdef HPX_HAVE_EXECINFO
#include <execinfo.h>
#endif

#ifdef HPX_HAVE_ABI_CXA_DEMANGLE
#include <cxxabi.h>
#endif

#ifdef HPX_HAVE_DLFCN
#include <dlfcn.h>
#endif
#ifdef HPX_HAVE_UNWIND
#include <unwind.h>
#endif

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <ostream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#if defined(HPX_MSVC)
#include <windows.h>

#include <dbghelp.h>
#endif

namespace hpx::util::stack_trace {

#if defined(HPX_HAVE_EXECINFO) && defined(HPX_HAVE_UNWIND)
    struct trace_data
    {
        constexpr trace_data(void** array, std::size_t size) noexcept
          : array_(array)
          , size_(size)
          , cfa_(0)
          , count_(std::size_t(-1))
        {
        }

        void** array_;         // storage for the stack trace
        std::size_t size_;     // number of frames
        std::uint64_t cfa_;    // canonical frame address
        std::size_t count_;
    };

    [[nodiscard]] _Unwind_Reason_Code trace_callback(
        _Unwind_Context* ctx, void* ptr)
    {
        if (!ptr)
            return _URC_NO_REASON;

        trace_data& d = *(reinterpret_cast<trace_data*>(ptr));

        // First call.
        if (std::size_t(-1) != d.count_)
        {
            // Get the instruction pointer for this frame.
            d.array_[d.count_] = reinterpret_cast<void*>(_Unwind_GetIP(ctx));

            // Get the CFA.
            std::uint64_t cfa = _Unwind_GetCFA(ctx);

            // Check if we're at the end of the stack.
            if ((0 < d.count_) &&
                (d.array_[d.count_ - 1] == d.array_[d.count_]) &&
                (cfa == d.cfa_))
            {
                return _URC_END_OF_STACK;
            }

            d.cfa_ = cfa;
        }

        if (++d.count_ == d.size_)
            return _URC_END_OF_STACK;

        return _URC_NO_REASON;
    }

    [[nodiscard]] std::size_t trace(void** array, std::size_t n)
    {
        trace_data d(array, n);

        if (1 <= n)
            _Unwind_Backtrace(trace_callback, reinterpret_cast<void*>(&d));

        if ((1 < d.count_) && d.array_[d.count_ - 1])
            --d.count_;

        return (std::size_t(-1) != d.count_) ? d.count_ : 0;
    }

#elif defined(HPX_HAVE_EXECINFO)

    [[nodiscard]] std::size_t trace(void** array, std::size_t n)
    {
        return ::backtrace(array, static_cast<int>(n));
    }

#elif defined(HPX_MSVC)

    [[nodiscard]] std::size_t trace(void** array, std::size_t n)
    {
#if _WIN32_WINNT < 0x0600
        // for Windows XP/Windows Server 2003
        if (n >= 63)
            n = 62;
#endif
        return RtlCaptureStackBackTrace(
            static_cast<ULONG>(0), static_cast<ULONG>(n), array, nullptr);
    }

#else

    [[nodiscard]] std::size_t trace(void** /*array*/, std::size_t /*n*/)
    {
        return 0;
    }

#endif

#if defined(HPX_HAVE_EXECINFO)
    [[nodiscard]] std::string get_symbol_exec_info(void* address)
    {
        char** ptr = backtrace_symbols(&address, 1);
        try
        {
            if (ptr == nullptr)
                return std::string("???");
            std::string res = ptr[0];
            // NOLINTNEXTLINE(bugprone-multi-level-implicit-pointer-conversion)
            free(ptr);
            ptr = nullptr;
            return res;
        }
        catch (...)
        {
            // NOLINTNEXTLINE(bugprone-multi-level-implicit-pointer-conversion)
            free(ptr);
            throw;
        }
    }
#endif

#if defined(HPX_HAVE_DLFCN) && defined(HPX_HAVE_ABI_CXA_DEMANGLE)
    [[nodiscard]] std::string get_symbol(void* ptr)
    {
        if (!ptr)
            return {};

        bool need_offset = true;
        std::ostringstream res;
        res.imbue(std::locale::classic());
        res << std::left << std::setw(sizeof(void*) * 2) << std::setfill(' ')
            << ptr << ": ";
        Dl_info info = {nullptr, nullptr, nullptr, nullptr};
        if (dladdr(ptr, &info) == 0)
        {
#if !defined(HPX_HAVE_EXECINFO)
            res << "???";
#else
            res << get_symbol_exec_info(ptr);
            need_offset = false;
#endif
        }
        else
        {
            if (info.dli_sname)
            {
#if defined(HPX_HAVE_STACKTRACES_DEMANGLE_SYMBOLS)
                int status = 0;
                char* demangled = abi::__cxa_demangle(
                    info.dli_sname, nullptr, nullptr, &status);
                if (demangled)
                {
                    res << demangled;
                    free(demangled);
                }
                else
                {
                    res << info.dli_sname;
                }
#else
                res << info.dli_sname;
#endif
            }
            else
            {
#if !defined(HPX_HAVE_EXECINFO)
                res << "???";
#else
                res << get_symbol_exec_info(ptr);
                need_offset = false;
#endif
            }

            std::ptrdiff_t offset = reinterpret_cast<char*>(ptr) -
                reinterpret_cast<char*>(info.dli_saddr);
            if (need_offset)
            {
                res << std::hex << " [0x" << offset << "]";
            }

            if (info.dli_fname)
            {
                res << " in " << info.dli_fname;
            }
        }
        return res.str();
    }

    [[nodiscard]] std::string get_symbols(
        void* const* addresses, std::size_t size)
    {
        // the first two stack frames are from the back tracing facility itself
        if (size > 2)
        {
            addresses += 2;
            size -= 2;
        }

        std::string res =
            std::to_string(size) + ((1 == size) ? " frame:" : " frames:");
        for (std::size_t i = 0; i < size; i++)
        {
            std::string tmp = get_symbol(addresses[i]);
            if (!tmp.empty())
            {
                res += '\n';
                res += tmp;
            }
        }
        return res;
    }

    void write_symbols(
        void* const* addresses, std::size_t size, std::ostream& out)
    {
        out << size << ((1 == size) ? " frame:" : " frames:");
        for (std::size_t i = 0; i < size; i++)
        {
            std::string tmp = get_symbol(addresses[i]);
            if (!tmp.empty())
            {
                out << '\n' << tmp;
            }
        }
        out << std::flush;
    }

#elif defined(HPX_HAVE_EXECINFO)

    [[nodiscard]] std::string get_symbol(void* address)
    {
        return get_symbol_exec_info(address);
    }

    [[nodiscard]] std::string get_symbols(
        void* const* address, std::size_t size)
    {
        // the first two stack frames are from the back tracing facility itself
        if (size > 2)
        {
            addresses += 2;
            size -= 2;
        }

        char** ptr = backtrace_symbols(address, size);
        try
        {
            if (ptr == nullptr)
                return {};
            std::string res =
                std::to_string(size) + ((1 == size) ? " frame:" : " frames:");
            for (std::size_t i = 0; i < size; i++)
            {
                res += '\n';
                res += ptr[i];
            }
            free(ptr);
            ptr = nullptr;
            return res;
        }
        catch (...)
        {
            free(ptr);
            throw;
        }
    }

    void write_symbols(
        void* const* addresses, std::size_t size, std::ostream& out)
    {
        char** ptr = backtrace_symbols(addresses, size);
        out << size << ((1 == size) ? " frame:" : " frames:");
        try
        {
            if (ptr == nullptr)
                return;
            for (std::size_t i = 0; i < size; i++)
                out << '\n' << ptr[i];
            free(ptr);
            ptr = nullptr;
            out << std::flush;
        }
        catch (...)
        {
            free(ptr);
            throw;
        }
    }

#elif defined(HPX_MSVC)

    namespace {

        HANDLE hProcess = nullptr;
        bool syms_ready = false;

        void init()
        {
            if (hProcess == nullptr)
            {
                hProcess = GetCurrentProcess();
                SymSetOptions(SYMOPT_DEFERRED_LOADS);

                if (SymInitialize(hProcess, nullptr, TRUE))
                {
                    syms_ready = true;
                }
            }
        }
    }    // namespace

    [[nodiscard]] std::string get_symbol(void* ptr)
    {
        if (ptr == nullptr)
            return {};

        init();
        std::ostringstream ss;
        ss << std::left << std::setw(sizeof(void*) * 2) << std::setfill(' ')
           << ptr;
        if (syms_ready)
        {
            DWORD64 dwDisplacement = 0;
            auto const dwAddress = reinterpret_cast<DWORD64>(ptr);

            std::vector<char> buffer(sizeof(SYMBOL_INFO) + MAX_SYM_NAME);
            auto const pSymbol =
                reinterpret_cast<PSYMBOL_INFO>(&buffer.front());

            pSymbol->SizeOfStruct = sizeof(SYMBOL_INFO);
            pSymbol->MaxNameLen = MAX_SYM_NAME;

            if (SymFromAddr(hProcess, dwAddress, &dwDisplacement, pSymbol))
            {
                ss << ": " << pSymbol->Name << std::hex << " +0x"
                   << dwDisplacement;
            }
            else
            {
                ss << ": ???";
            }
        }
        return ss.str();
    }

    [[nodiscard]] std::string get_symbols(
        void* const* addresses, std::size_t size)
    {
        // the first two stack frames are from the back tracing facility itself
        if (size > 2)
        {
            addresses += 2;
            size -= 2;
        }

        std::string res =
            std::to_string(size) + ((1 == size) ? " frame:" : " frames:");
        for (std::size_t i = 0; i < size; i++)
        {
            std::string tmp = get_symbol(addresses[i]);
            if (!tmp.empty())
            {
                res += '\n';
                res += tmp;
            }
        }
        return res;
    }

    void write_symbols(
        void* const* addresses, std::size_t size, std::ostream& out)
    {
        out << size << ((1 == size) ? " frame:" : " frames:");    //-V128
        for (std::size_t i = 0; i < size; i++)
        {
            std::string tmp = get_symbol(addresses[i]);
            if (!tmp.empty())
            {
                out << '\n' << tmp;
            }
        }
        out << std::flush;
    }

#else

    [[nodiscard]] std::string get_symbol(void* ptr)
    {
        if (!ptr)
            return {};
        std::ostringstream res;
        res.imbue(std::locale::classic());
        res << std::left << std::setw(sizeof(void*) * 2) << std::setfill(' ')
            << ptr;
        return res.str();
    }

    std::string get_symbols(void* const* ptrs, std::size_t size)
    {
        if (!ptrs)
            return {};

        // the first two stack frames are from the back tracing facility itself
        if (size > 2)
        {
            ptrs += 2;
            size -= 2;
        }

        std::ostringstream res;
        res.imbue(std::locale::classic());
        write_symbols(ptrs, size, res);
        return res.str();
    }

    void write_symbols(
        void* const* addresses, std::size_t size, std::ostream& out)
    {
        out << size << ((1 == size) ? " frame:" : " frames:");    //-V128
        for (std::size_t i = 0; i < size; i++)
        {
            if (addresses[i] != nullptr)
                out << '\n'
                    << std::left << std::setw(sizeof(void*) * 2)
                    << std::setfill(' ') << addresses[i];
        }
        out << std::flush;
    }

#endif
}    // namespace hpx::util::stack_trace

#endif
