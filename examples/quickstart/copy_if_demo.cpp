//  Copyright (c) 2026 Arivoli Ramamoorthy
//
//  SPDX-License-Identifier: BSL-1.0
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

// Parallel copy_if demo for Tracy profiling.

#include <hpx/algorithm.hpp>
#include <hpx/execution.hpp>
#include <hpx/init.hpp>

#if defined(HPX_HAVE_MODULE_TRACY)
#include <hpx/modules/tracy.hpp>
#endif

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <numeric>
#include <random>
#include <vector>

int hpx_main(hpx::program_options::variables_map& vm)
{
    std::size_t const n = vm["n"].as<std::size_t>();
    unsigned int seed = std::random_device{}();
    if (vm.count("seed"))
        seed = vm["seed"].as<unsigned int>();

    std::cout << "n=" << n << " seed=" << seed << "\n";

    std::vector<int> input(n);
    {
#if defined(HPX_HAVE_MODULE_TRACY)
        hpx::tracy::region z("fill_input", 0, 0);
#endif
        std::mt19937 gen(seed);
        std::uniform_int_distribution<int> dis(0, 99);
        std::generate(input.begin(), input.end(), [&] { return dis(gen); });
    }

    std::vector<int> output(n);

    {
#if defined(HPX_HAVE_MODULE_TRACY)
        hpx::tracy::frame_mark_start("copy_if");
        hpx::tracy::region z("copy_if_par", 0, 0);
#endif
        auto last = hpx::copy_if(hpx::execution::par, input.begin(),
            input.end(), output.begin(), [](int x) { return x % 2 == 0; });

        std::size_t const count =
            static_cast<std::size_t>(last - output.begin());
        std::cout << "copy_if kept " << count << " / " << n << " elements\n";
#if defined(HPX_HAVE_MODULE_TRACY)
        hpx::tracy::frame_mark_end("copy_if");
#endif
    }

    return hpx::local::finalize();
}

int main(int argc, char* argv[])
{
    using namespace hpx::program_options;
    options_description cmdline(
        "usage: " HPX_APPLICATION_STRING " [options]");
    // clang-format off
    cmdline.add_options()
        ("n",
        value<std::size_t>()->default_value(10'000'000),
        "Number of elements")
        ("seed,s",
        value<unsigned int>(),
        "Random seed");
    // clang-format on

    hpx::local::init_params init_args;
    init_args.desc_cmdline = cmdline;
    return hpx::local::init(hpx_main, argc, argv, init_args);
}
