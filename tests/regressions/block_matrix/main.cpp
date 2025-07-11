// Copyright (c) 2013 Erik Schnetter
//
// SPDX-License-Identifier: BSL-1.0
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <hpx/config.hpp>
#if !defined(HPX_COMPUTE_DEVICE_CODE)
#include <hpx/hpx.hpp>

#include "defs.hpp"
#include "tests.hpp"

#include <hpx/hpx.hpp>
#include <hpx/hpx_init.hpp>

#include <iostream>
#include <string>
#include <vector>

void output_hpx_info()
{
    std::cout << "HPX information:" << std::endl;

    int nlocs = static_cast<int>(hpx::get_num_localities().get());
    std::cout << "There are " << nlocs << " localities" << std::endl;
    hpx::id_type here = hpx::find_here();
    std::cout << "   here=" << here << std::endl;
    hpx::id_type root = hpx::find_root_locality();
    std::cout << "   root=" << root << std::endl;
    std::vector<hpx::id_type> locs = hpx::find_all_localities();
    std::cout << "   locs=" << mkstr(locs) << std::endl;

    int nthreads = static_cast<int>(hpx::get_num_worker_threads());
    std::cout << "There are " << nthreads << " threads overall" << std::endl;
    hpx::threads::thread_id_type self = hpx::threads::get_self_id();
    std::string name = hpx::get_thread_name();
    std::cout << "   self=" << self << " name=" << name << std::endl;

    std::cout << std::endl;
}

HPX_PLAIN_ACTION(test_dense)
HPX_PLAIN_ACTION(test_blocked)

int hpx_main(hpx::program_options::variables_map&)
{
    std::cout << "Block-structured DGEMM with HPX" << std::endl
              << "2013-09-11 Erik Schnetter <eschnetter@perimeterinstitute.ca>"
              << std::endl
              << std::endl;

    output_hpx_info();

#if 0
  // Direct function calls
  test_dense();
  test_blocked();
#elif 0
    // Async function calls
    hpx::future<void> f1 = hpx::async(test_dense);
    hpx::future<void> f2 = hpx::async(test_blocked);
    hpx::wait_all(f1, f2);
#elif 0
    // Async action calls
    test_dense_action act_test_dense;
    test_blocked_action act_test_blocked;
    hpx::id_type here = hpx::find_here();
    hpx::future<void> f1 = hpx::async(act_test_dense, here);
    hpx::future<void> f2 = hpx::async(act_test_blocked, here);
    hpx::wait_all(f1, f2);
#elif 1
    // Async action calls on remote localities
    test_dense_action act_test_dense;
    test_blocked_action act_test_blocked;
    int nlocs = static_cast<int>(hpx::get_num_localities().get());
    std::vector<hpx::id_type> locs = hpx::find_all_localities();
    hpx::id_type loc1 = locs[1 % nlocs];
    hpx::id_type loc2 = locs[2 % nlocs];
    hpx::future<void> f1 = hpx::async(act_test_dense, loc1);
    hpx::future<void> f2 = hpx::async(act_test_blocked, loc2);
    hpx::wait_all(f1, f2);
#endif

    std::cout << "Done." << std::endl;

    hpx::finalize();
    return report_errors();
}

int main(int argc, char** argv)
{
    return hpx::init(argc, argv);
}
#endif
