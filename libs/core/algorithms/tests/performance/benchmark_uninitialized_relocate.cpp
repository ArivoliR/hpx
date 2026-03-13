///////////////////////////////////////////////////////////////////////////////
//  Copyright (c) 2026 Arivoli Ramamoorthy
//
//  SPDX-License-Identifier: BSL-1.0
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
///////////////////////////////////////////////////////////////////////////////

// Benchmarks for hpx::experimental::uninitialized_relocate and
// hpx::experimental::uninitialized_relocate_backward.
//
// Measures:
//   1. Non-overlapping sequential vs parallel (trivially + non-trivially relocatable)
//   2. Left-shift overlapping sequential vs parallel (forward algorithm)
//   3. Right-shift overlapping sequential vs parallel (backward algorithm)

#include <hpx/algorithm.hpp>
#include <hpx/chrono.hpp>
#include <hpx/format.hpp>
#include <hpx/init.hpp>
#include <hpx/modules/testing.hpp>
#include <hpx/modules/type_support.hpp>
#include <hpx/program_options.hpp>

#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <iostream>

using hpx::experimental::uninitialized_relocate;
using hpx::experimental::uninitialized_relocate_backward;

///////////////////////////////////////////////////////////////////////////////
// Trivially relocatable type: cheap to relocate (just a memcpy)
struct trivially_relocatable_int
{
    int value;
    explicit trivially_relocatable_int(int v = 0) noexcept
      : value(v)
    {
    }
    trivially_relocatable_int(trivially_relocatable_int&&) = default;
    ~trivially_relocatable_int() = default;
};
HPX_DECLARE_TRIVIALLY_RELOCATABLE(trivially_relocatable_int);

static_assert(
    hpx::experimental::is_trivially_relocatable_v<trivially_relocatable_int>);

// Non-trivially relocatable type: has a user-defined move constructor
// that does real work (simulates e.g. a type with a self-pointer).
struct non_trivially_relocatable_string
{
    static constexpr std::size_t buf_size = 64;
    char buf[buf_size];
    std::size_t len;

    explicit non_trivially_relocatable_string(int seed = 0) noexcept
      : len(static_cast<std::size_t>(seed % buf_size))
    {
        for (std::size_t i = 0; i < len; ++i)
            buf[i] = static_cast<char>('a' + (seed + i) % 26);
        buf[len] = '\0';
    }

    non_trivially_relocatable_string(
        non_trivially_relocatable_string&& o) noexcept
      : len(o.len)
    {
        std::memcpy(buf, o.buf, len + 1);
        o.len = 0;
        o.buf[0] = '\0';
    }

    ~non_trivially_relocatable_string() = default;
};

static_assert(!hpx::experimental::is_trivially_relocatable_v<
    non_trivially_relocatable_string>);

///////////////////////////////////////////////////////////////////////////////
// Helpers

template <typename T>
T* alloc_uninit(std::size_t n)
{
    return static_cast<T*>(std::malloc(n * sizeof(T)));
}

template <typename T>
void construct_range(T* ptr, std::size_t n)
{
    for (std::size_t i = 0; i < n; ++i)
        ::new (static_cast<void*>(ptr + i)) T(static_cast<int>(i));
}

template <typename T>
void destroy_range(T* ptr, std::size_t n)
{
    std::destroy_n(ptr, n);
}

///////////////////////////////////////////////////////////////////////////////
// Benchmark runners

template <typename T, typename ExPolicy>
double bench_non_overlapping_forward(
    std::size_t n, ExPolicy policy, int iterations)
{
    T* src = alloc_uninit<T>(n);
    T* dst = alloc_uninit<T>(n);
    double total = 0.0;

    for (int it = 0; it < iterations; ++it)
    {
        construct_range(src, n);
        auto start = hpx::chrono::high_resolution_clock::now();
        uninitialized_relocate(policy, src, src + n, dst);
        auto end = hpx::chrono::high_resolution_clock::now();
        // now() returns uint64_t nanoseconds; convert to seconds
        total += static_cast<double>(end - start) * 1e-9;
        destroy_range(dst, n);
    }

    std::free(src);
    std::free(dst);
    return total / iterations;
}

template <typename T, typename ExPolicy>
double bench_overlapping_forward(
    std::size_t n, std::size_t overlap, ExPolicy policy, int iterations)
{
    // Left-shift overlap: src = [ptr+overlap, ptr+n+overlap), dst = ptr
    T* buf = alloc_uninit<T>(n + overlap);
    double total = 0.0;

    for (int it = 0; it < iterations; ++it)
    {
        construct_range(buf + overlap, n);
        auto start = hpx::chrono::high_resolution_clock::now();
        uninitialized_relocate(policy, buf + overlap, buf + n + overlap, buf);
        auto end = hpx::chrono::high_resolution_clock::now();
        total += static_cast<double>(end - start) * 1e-9;
        destroy_range(buf, n);
    }

    std::free(buf);
    return total / iterations;
}

template <typename T, typename ExPolicy>
double bench_overlapping_backward(
    std::size_t n, std::size_t overlap, ExPolicy policy, int iterations)
{
    // Right-shift overlap: src = [ptr, ptr+n), dst_last = ptr+n+overlap
    T* buf = alloc_uninit<T>(n + overlap);
    double total = 0.0;

    for (int it = 0; it < iterations; ++it)
    {
        construct_range(buf, n);
        // destroy the overlap zone that will be overwritten
        // (for bookkeeping - objects don't exist there yet)
        auto start = hpx::chrono::high_resolution_clock::now();
        uninitialized_relocate_backward(
            policy, buf, buf + n, buf + n + overlap);
        auto end = hpx::chrono::high_resolution_clock::now();
        total += static_cast<double>(end - start) * 1e-9;
        destroy_range(buf + overlap, n);
    }

    std::free(buf);
    return total / iterations;
}

///////////////////////////////////////////////////////////////////////////////
void print_result(const char* label, double sec, std::size_t n)
{
    double mb_per_sec = (static_cast<double>(n) * sizeof(int)) / (sec * 1e6);
    hpx::util::format_to(std::cout,
        "  {:50s}: {:9.3f} ms  ({:.1f} MiB/s)\n", label, sec * 1000.0,
        mb_per_sec);
}

///////////////////////////////////////////////////////////////////////////////
int hpx_main(hpx::program_options::variables_map& vm)
{
    std::size_t n = vm["n"].as<std::size_t>();
    std::size_t overlap = vm["overlap"].as<std::size_t>();
    int iterations = vm["iterations"].as<int>();

    std::cout << "=== uninitialized_relocate benchmark ===\n";
    std::cout << "  n=" << n << "  overlap=" << overlap
              << "  iterations=" << iterations << "\n\n";

    // ---- Non-overlapping: forward ----
    std::cout << "--- Non-overlapping (forward) ---\n";

    print_result("trivially_relocatable seq",
        bench_non_overlapping_forward<trivially_relocatable_int>(
            n, hpx::execution::seq, iterations),
        n);
    print_result("trivially_relocatable par",
        bench_non_overlapping_forward<trivially_relocatable_int>(
            n, hpx::execution::par, iterations),
        n);
    print_result("non_trivially_relocatable seq",
        bench_non_overlapping_forward<non_trivially_relocatable_string>(
            n, hpx::execution::seq, iterations),
        n);
    print_result("non_trivially_relocatable par",
        bench_non_overlapping_forward<non_trivially_relocatable_string>(
            n, hpx::execution::par, iterations),
        n);

    // ---- Left-shift overlapping: forward ----
    std::cout << "\n--- Left-shift overlapping (forward, overlap=" << overlap
              << ") ---\n";

    print_result("trivially_relocatable seq",
        bench_overlapping_forward<trivially_relocatable_int>(
            n, overlap, hpx::execution::seq, iterations),
        n);
    print_result("trivially_relocatable par",
        bench_overlapping_forward<trivially_relocatable_int>(
            n, overlap, hpx::execution::par, iterations),
        n);
    print_result("non_trivially_relocatable seq",
        bench_overlapping_forward<non_trivially_relocatable_string>(
            n, overlap, hpx::execution::seq, iterations),
        n);
    print_result("non_trivially_relocatable par",
        bench_overlapping_forward<non_trivially_relocatable_string>(
            n, overlap, hpx::execution::par, iterations),
        n);

    // ---- Right-shift overlapping: backward ----
    std::cout << "\n--- Right-shift overlapping (backward, overlap=" << overlap
              << ") ---\n";

    print_result("trivially_relocatable seq",
        bench_overlapping_backward<trivially_relocatable_int>(
            n, overlap, hpx::execution::seq, iterations),
        n);
    print_result("trivially_relocatable par",
        bench_overlapping_backward<trivially_relocatable_int>(
            n, overlap, hpx::execution::par, iterations),
        n);
    print_result("non_trivially_relocatable seq",
        bench_overlapping_backward<non_trivially_relocatable_string>(
            n, overlap, hpx::execution::seq, iterations),
        n);
    print_result("non_trivially_relocatable par",
        bench_overlapping_backward<non_trivially_relocatable_string>(
            n, overlap, hpx::execution::par, iterations),
        n);

    return hpx::local::finalize();
}

int main(int argc, char* argv[])
{
    using namespace hpx::program_options;
    options_description desc_commandline(
        "Usage: " HPX_APPLICATION_STRING " [options]");

    // clang-format off
    desc_commandline.add_options()
        ("n",
            value<std::size_t>()->default_value(1 << 20),
            "number of elements to relocate")
        ("overlap",
            value<std::size_t>()->default_value(64),
            "overlap size for overlapping range tests")
        ("iterations",
            value<int>()->default_value(10),
            "number of benchmark iterations");
    // clang-format on

    hpx::local::init_params init_args;
    init_args.desc_cmdline = desc_commandline;

    return hpx::local::init(hpx_main, argc, argv, init_args);
}
