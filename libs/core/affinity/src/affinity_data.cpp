//  Copyright (c) 2007-2025 Hartmut Kaiser
//
//  SPDX-License-Identifier: BSL-1.0
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <hpx/affinity/affinity_data.hpp>
#include <hpx/affinity/parse_affinity_options.hpp>
#include <hpx/assert.hpp>
#include <hpx/modules/errors.hpp>
#include <hpx/topology/cpu_mask.hpp>
#include <hpx/topology/topology.hpp>

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <string>
#include <utility>
#include <vector>

namespace hpx::threads::policies::detail {

    namespace {

        inline std::size_t count_initialized(
            std::vector<mask_type> const& masks) noexcept
        {
            std::size_t count = 0;
            for (mask_cref_type m : masks)
            {
                if (threads::any(m))
                    ++count;
            }
            return count;
        }
    }    // namespace

    affinity_data::affinity_data()
      : num_threads_(0)
      , pu_offset_(static_cast<std::size_t>(-1))
      , pu_step_(1)
      , used_cores_(0)
      , affinity_domain_("pu")
      , no_affinity_()
      , disable_affinities_(false)
      , use_process_mask_(false)
      , num_pus_needed_(0)
    {
        threads::resize(
            no_affinity_, static_cast<std::size_t>(hardware_concurrency()));
    }

    affinity_data::affinity_data(affinity_data const&) = default;
    affinity_data::affinity_data(affinity_data&&) noexcept = default;
    affinity_data& affinity_data::operator=(affinity_data const&) = default;
    affinity_data& affinity_data::operator=(affinity_data&&) noexcept = default;

    affinity_data::~affinity_data()
    {
        --instance_number_counter_;
    }

    void affinity_data::init(std::size_t const num_threads,
        std::size_t const max_cores, std::size_t const pu_offset,
        std::size_t const pu_step, std::size_t const used_cores,
        std::string affinity_domain,    // -V813
        std::string const& affinity_description, bool use_process_mask)
    {
#if defined(__APPLE__)
        use_process_mask = false;
        disable_affinities_ = true;
#endif

        use_process_mask_ = use_process_mask;
        num_threads_ = num_threads;
        std::size_t const num_system_pus =
            static_cast<std::size_t>(hardware_concurrency());

        if (pu_offset == static_cast<std::size_t>(-1))
        {
            pu_offset_ = 0;
        }
        else
        {
            pu_offset_ = pu_offset;
        }

        if (num_system_pus > 1)
        {
            pu_step_ = pu_step % num_system_pus;
        }

        affinity_domain_ = HPX_MOVE(affinity_domain);
        pu_nums_.clear();

        init_cached_pu_nums(num_system_pus);

        auto const& topo = threads::create_topology();

        if (affinity_description == "none")
        {
            // don't use any affinity for any of the os-threads
            threads::resize(no_affinity_, num_system_pus);
            for (std::size_t i = 0; i != num_threads_; ++i)
                threads::set(no_affinity_, get_pu_num(i));
            disable_affinities_ = true;
        }
        else if (!affinity_description.empty())
        {
            affinity_masks_.clear();
            affinity_masks_.resize(num_threads_, mask_type{});

            for (std::size_t i = 0; i != num_threads_; ++i)
                threads::resize(affinity_masks_[i], num_system_pus);

            parse_affinity_options(affinity_description, affinity_masks_,
                used_cores, max_cores, num_threads_, pu_nums_,
                use_process_mask_);

            if (std::size_t const num_initialized =
                    count_initialized(affinity_masks_);
                num_initialized != num_threads_)
            {
                HPX_THROW_EXCEPTION(hpx::error::bad_parameter,
                    "affinity_data::affinity_data",
                    "The number of OS threads requested ({1}) does not match "
                    "the number of threads to bind ({2})",
                    num_threads_, num_initialized);
            }
        }
        else if (pu_offset == static_cast<std::size_t>(-1))
        {
            // calculate the pu offset based on the used cores, but only if it's
            // not explicitly specified
            for (std::size_t num_core = 0; num_core != used_cores; ++num_core)
            {
                pu_offset_ += topo.get_number_of_core_pus(num_core);
            }
        }

        // correct used_cores from config data if appropriate
        if (used_cores_ == 0)
        {
            used_cores_ = used_cores;
        }

        pu_offset_ %= num_system_pus;

        std::vector<std::size_t> cores;
        cores.reserve(num_threads_);
        for (std::size_t i = 0; i != num_threads_; ++i)
        {
            std::size_t const add_me = topo.get_core_number(get_pu_num(i));
            cores.push_back(add_me);
        }

        std::sort(cores.begin(), cores.end());
        auto const it = std::unique(cores.begin(), cores.end());
        cores.erase(it, cores.end());

        std::size_t const num_unique_cores = cores.size();
        num_pus_needed_ = (std::max) (num_unique_cores, max_cores);
    }

    void affinity_data::set_num_threads(size_t const num_threads) noexcept
    {
        num_threads_ = num_threads;
    }

    void affinity_data::set_affinity_masks(
        std::vector<threads::mask_type> const& affinity_masks)
    {
        affinity_masks_ = affinity_masks;
    }

    void affinity_data::set_affinity_masks(
        std::vector<threads::mask_type>&& affinity_masks) noexcept
    {
        affinity_masks_ = HPX_MOVE(affinity_masks);
    }

    mask_type affinity_data::get_pu_mask(threads::topology const& topo,
        std::size_t const global_thread_num) const
    {
        // --hpx:bind=none disables all affinity
        if (threads::test(no_affinity_, global_thread_num))
        {
            auto m = mask_type();
            threads::resize(
                m, static_cast<std::size_t>(hardware_concurrency()));
            threads::set(m, get_pu_num(global_thread_num));
            return m;
        }

        // if we have individual, predefined affinity masks, return those
        if (!affinity_masks_.empty())
            return affinity_masks_[global_thread_num];

        // otherwise return mask based on affinity domain
        std::size_t const pu_num = get_pu_num(global_thread_num);
        if (0 == std::string("pu").find(affinity_domain_))
        {
            // The affinity domain is 'processing unit', just convert the
            // pu-number into a bit-mask.
            return topo.get_thread_affinity_mask(pu_num);
        }
        if (0 == std::string("core").find(affinity_domain_))
        {
            // The affinity domain is 'core', return a bit mask corresponding to
            // all processing units of the core containing the given pu_num.
            return topo.get_core_affinity_mask(pu_num);
        }
        if (0 == std::string("numa").find(affinity_domain_))
        {
            // The affinity domain is 'numa', return a bit mask corresponding to
            // all processing units of the NUMA domain containing the given
            // pu_num.
            return topo.get_numa_node_affinity_mask(pu_num);
        }

        // The affinity domain is 'machine', return a bit mask corresponding
        // to all processing units of the machine.
        HPX_ASSERT(0 == std::string("machine").find(affinity_domain_));
        return topo.get_machine_affinity_mask();
    }

    mask_type affinity_data::get_used_pus_mask(
        threads::topology const& topo, std::size_t const pu_num) const
    {
        auto const overall_threads =
            static_cast<std::size_t>(hardware_concurrency());

        auto ret = mask_type();
        threads::resize(ret, overall_threads);

        // --hpx:bind=none disables all affinity
        if (threads::test(no_affinity_, pu_num))
        {
            threads::set(ret, pu_num);
            return ret;
        }

        // clang-format off
        for (std::size_t thread_num = 0; thread_num != num_threads_;
            ++thread_num)
        // clang-format on
        {
            auto const thread_mask = get_pu_mask(topo, thread_num);
            for (std::size_t i = 0; i != overall_threads; ++i)
            {
                if (threads::test(thread_mask, i))
                {
                    threads::set(ret, i);
                }
            }
        }

        return ret;
    }

    std::size_t affinity_data::get_thread_occupancy(
        threads::topology const& topo, std::size_t const pu_num) const
    {
        std::size_t count = 0;
        if (threads::test(no_affinity_, pu_num))
        {
            ++count;
        }
        else
        {
            auto pu_mask = mask_type();

            threads::resize(
                pu_mask, static_cast<std::size_t>(hardware_concurrency()));
            threads::set(pu_mask, pu_num);

            // clang-format off
            for (std::size_t num_thread = 0; num_thread != num_threads_;
                ++num_thread)
            // clang-format on
            {
                mask_cref_type affinity_mask = get_pu_mask(topo, num_thread);
                if (threads::any(pu_mask & affinity_mask))
                    ++count;
            }
        }
        return count;
    }

    std::size_t affinity_data::get_pu_num(
        std::size_t const num_thread) const noexcept
    {
        HPX_ASSERT(num_thread < pu_nums_.size());
        return pu_nums_[num_thread];
    }

    void affinity_data::set_pu_nums(std::vector<std::size_t> const& pu_nums)
    {
        pu_nums_ = pu_nums;
    }

    void affinity_data::set_pu_nums(std::vector<std::size_t>&& pu_nums) noexcept
    {
        pu_nums_ = HPX_MOVE(pu_nums);
    }

    // means of adding a processing unit after initialization
    void affinity_data::add_punit(
        std::size_t const virt_core, std::size_t const thread_num)
    {
        std::size_t const num_system_pus =
            static_cast<std::size_t>(hardware_concurrency());

        // initialize affinity_masks and set the mask for the given virt_core
        if (affinity_masks_.empty())
        {
            affinity_masks_.resize(num_threads_);
            for (std::size_t i = 0; i != num_threads_; ++i)
                threads::resize(affinity_masks_[i], num_system_pus);
        }
        threads::set(affinity_masks_[virt_core], thread_num);

        // find first used pu, which is then stored as the pu_offset
        std::size_t first_pu = static_cast<std::size_t>(-1);
        for (std::size_t i = 0; i != num_threads_; ++i)
        {
            std::size_t first = threads::find_first(affinity_masks_[i]);
            first_pu = (std::min) (first_pu, first);
        }
        if (first_pu != static_cast<std::size_t>(-1))
            pu_offset_ = first_pu;

        init_cached_pu_nums(num_system_pus);
    }

    void affinity_data::init_cached_pu_nums(
        std::size_t const hardware_concurrency)
    {
        if (pu_nums_.empty())
        {
            pu_nums_.resize(num_threads_);
            for (std::size_t i = 0; i != num_threads_; ++i)
            {
                pu_nums_[i] = get_pu_num(i, hardware_concurrency);
            }
        }
    }

    std::size_t affinity_data::get_pu_num(std::size_t const num_thread,
        std::size_t const hardware_concurrency) const
    {
        // The offset shouldn't be larger than the number of available
        // processing units.
        HPX_ASSERT(pu_offset_ < hardware_concurrency);

        // The distance between assigned processing units shouldn't be zero
        HPX_ASSERT(pu_step_ > 0 && pu_step_ <= hardware_concurrency);

        // We 'scale' the thread number to compute the corresponding processing
        // unit number.
        //
        // The baseline processing unit number is computed from the given
        // pu-offset and pu-step.
        std::size_t const num_pu = pu_offset_ + pu_step_ * num_thread;

        // We add an offset, which allows to 'roll over' if the pu number would
        // get larger than the number of available processing units. Note that
        // it does not make sense to 'roll over' farther than the given pu-step.
        std::size_t const offset = (num_pu / hardware_concurrency) % pu_step_;

        // The resulting pu number has to be smaller than the available
        // number of processing units.
        return (num_pu + offset) % hardware_concurrency;
    }

    std::atomic<int> affinity_data::instance_number_counter_(-1);
}    // namespace hpx::threads::policies::detail
