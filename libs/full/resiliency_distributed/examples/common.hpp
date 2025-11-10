//  Copyright (c) 2019 National Technology & Engineering Solutions of Sandia,
//                     LLC (NTESS).
//  Copyright (c) 2018-2019 Hartmut Kaiser
//  Copyright (c) 2018-2019 Adrian Serio
//  Copyright (c) 2019-2020 Nikunj Gupta
//  SPDX-License-Identifier: BSL-1.0
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include <hpx/config.hpp>
#if !defined(HPX_COMPUTE_DEVICE_CODE)

#include <hpx/actions_base/plain_action.hpp>
#include <hpx/include/runtime.hpp>

#include <algorithm>
#include <cstddef>
#include <stdexcept>
#include <vector>

namespace resiliency_example {

    int universal_ans(std::vector<hpx::id_type> f_locales, std::size_t size)
    {
        // Pretending to do some useful work
        std::size_t start = hpx::chrono::high_resolution_clock::now();

        while ((hpx::chrono::high_resolution_clock::now() - start) <
            (size * 100))
        {
        }

        // Check if the node is faulty
        for (const auto& locale : f_locales)
        {
            // Throw a runtime error in case the node is faulty
            if (locale == hpx::find_here())
                throw std::runtime_error("runtime error occurred.");
        }

        return 42;
    }

    bool validate(int ans)
    {
        return ans == 42;
    }

    void mark_faulty_nodes(std::vector<hpx::id_type> const& locales,
        std::size_t f_nodes, std::vector<hpx::id_type>& f_locales)
    {
        std::vector<std::size_t> visited;

        // Mark nodes as faulty
        for (std::size_t i = 0; i < f_nodes; ++i)
        {
            std::size_t num = std::rand() % locales.size();
            while (visited.end() !=
                std::find(visited.begin(), visited.end(), num))
            {
                num = std::rand() % locales.size();
            }

            f_locales.push_back(locales.at(num));
            visited.push_back(num);
        }
    }

}    // namespace resiliency_example

HPX_PLAIN_ACTION(resiliency_example::universal_ans, universal_action)

#endif
