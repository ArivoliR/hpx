//  Copyright (c) 2019 National Technology & Engineering Solutions of Sandia,
//                     LLC (NTESS).
//  Copyright (c) 2018-2019 Hartmut Kaiser
//  Copyright (c) 2018-2019 Adrian Serio
//  Copyright (c) 2019-2020 Nikunj Gupta
//  SPDX-License-Identifier: BSL-1.0
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <hpx/config.hpp>
#if !defined(HPX_COMPUTE_DEVICE_CODE)

#include <hpx/assert.hpp>
#include <hpx/hpx_init.hpp>
#include <hpx/include/runtime.hpp>
#include <hpx/modules/futures.hpp>
#include <hpx/modules/resiliency_distributed.hpp>
#include <hpx/modules/testing.hpp>

#include "common.hpp"

#include <cstddef>
#include <iostream>
#include <random>
#include <vector>

int vote(std::vector<int>&& results)
{
    return results.at(0);
}

int hpx_main(hpx::program_options::variables_map& vm)
{
    std::size_t f_nodes = vm["f-nodes"].as<std::size_t>();
    std::size_t size = vm["size"].as<std::size_t>();
    std::size_t num_tasks = vm["num-tasks"].as<std::size_t>();
    std::size_t num_replications = vm["num-replications"].as<std::size_t>();

    universal_action ac;
    std::vector<hpx::id_type> locales = hpx::find_all_localities();

    // Make sure that the number of faulty nodes are less than the number of
    // localities we work on.
    HPX_ASSERT(f_nodes < locales.size());

    // List of faulty nodes
    std::vector<hpx::id_type> f_locales;

    // Mark nodes as faulty
    resiliency_example::mark_faulty_nodes(locales, f_nodes, f_locales);

    {
        hpx::chrono::high_resolution_timer t;

        std::vector<hpx::future<int>> tasks;
        for (std::size_t i = 0; i < num_tasks; ++i)
        {
            std::vector<hpx::id_type> ids;
            ids.reserve(num_replications);
            if (num_replications < locales.size())
            {
                for (auto& locale : locales)
                    ids.push_back(locale);
            }
            else
            {
                for (auto& locale : locales)
                    ids.push_back(locale);

                for (std::size_t i = 0; i < num_replications - ids.size(); ++i)
                {
                    ids.emplace_back(locales.at(i));
                }
            }

            tasks.push_back(hpx::resiliency::experimental::async_replicate(
                ids, ac, f_locales, size));

            std::rotate(locales.begin(), locales.begin() + 1, locales.end());
        }

        hpx::wait_all(tasks);

        double elapsed = t.elapsed();
        std::cout << "Replicate: " << elapsed << std::endl;
    }

    {
        hpx::chrono::high_resolution_timer t;

        std::vector<hpx::future<int>> tasks;
        for (std::size_t i = 0; i < num_tasks; ++i)
        {
            std::vector<hpx::id_type> ids;
            ids.reserve(num_replications);
            if (num_replications < locales.size())
            {
                for (auto& locale : locales)
                    ids.push_back(locale);
            }
            else
            {
                for (auto& locale : locales)
                    ids.push_back(locale);

                for (std::size_t i = 0; i < num_replications - ids.size(); ++i)
                {
                    ids.emplace_back(locales.at(i));
                }
            }

            tasks.push_back(
                hpx::resiliency::experimental::async_replicate_validate(
                    ids, &resiliency_example::validate, ac, f_locales, size));

            std::rotate(locales.begin(), locales.begin() + 1, locales.end());
        }

        hpx::wait_all(tasks);

        double elapsed = t.elapsed();
        std::cout << "Replicate Validate: " << elapsed << std::endl;
    }

    {
        hpx::chrono::high_resolution_timer t;

        std::vector<hpx::future<int>> tasks;
        for (std::size_t i = 0; i < num_tasks; ++i)
        {
            std::vector<hpx::id_type> ids;
            ids.reserve(num_replications);
            if (num_replications < locales.size())
            {
                for (auto& locale : locales)
                    ids.push_back(locale);
            }
            else
            {
                for (auto& locale : locales)
                    ids.push_back(locale);

                for (std::size_t i = 0; i < num_replications - ids.size(); ++i)
                {
                    ids.emplace_back(locales.at(i));
                }
            }

            tasks.push_back(hpx::resiliency::experimental::async_replicate_vote(
                ids, &vote, ac, f_locales, size));

            std::rotate(locales.begin(), locales.begin() + 1, locales.end());
        }

        hpx::wait_all(tasks);

        double elapsed = t.elapsed();
        std::cout << "Replicate Vote: " << elapsed << std::endl;
    }

    {
        hpx::chrono::high_resolution_timer t;

        std::vector<hpx::future<int>> tasks;
        for (std::size_t i = 0; i < num_tasks; ++i)
        {
            std::vector<hpx::id_type> ids;
            ids.reserve(num_replications);
            if (num_replications < locales.size())
            {
                for (auto& locale : locales)
                    ids.push_back(locale);
            }
            else
            {
                for (auto& locale : locales)
                    ids.push_back(locale);

                for (std::size_t i = 0; i < num_replications - ids.size(); ++i)
                {
                    ids.emplace_back(locales.at(i));
                }
            }

            tasks.push_back(
                hpx::resiliency::experimental::async_replicate_vote_validate(
                    ids, &vote, &resiliency_example::validate, ac, f_locales,
                    size));

            std::rotate(locales.begin(), locales.begin() + 1, locales.end());
        }

        hpx::wait_all(tasks);

        double elapsed = t.elapsed();
        std::cout << "Replicate Vote Validate: " << elapsed << std::endl;
    }

    return hpx::finalize();
}

int main(int argc, char* argv[])
{
    // Configure application-specific options
    hpx::program_options::options_description desc_commandline;

    namespace po = hpx::program_options;

    // clang-format off
    desc_commandline.add_options()
        ("f-nodes", po::value<std::size_t>()->default_value(1),
            "Number of faulty nodes to be injected")
        ("size", po::value<std::size_t>()->default_value(200),
            "Grain size of a task")
        ("num-tasks", po::value<std::size_t>()->default_value(1000),
            "Number of tasks to invoke")
        ("num-replications", po::value<std::size_t>()->default_value(3),
            "Total number of replicates for a task (including the task itself)")
    ;
    // clang-format on

    // Initialize and run HPX
    hpx::init_params params;
    params.desc_cmdline = desc_commandline;
    return hpx::init(argc, argv, params);
}

#endif
