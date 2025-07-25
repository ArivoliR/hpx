//  Copyright (c) 2022 Srinivas Yadav
//
//  SPDX-License-Identifier: BSL-1.0
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include <hpx/config.hpp>

#if defined(HPX_HAVE_DATAPAR_EVE)

#include <hpx/assert.hpp>
#include <hpx/concepts/concepts.hpp>
#include <hpx/execution/traits/detail/simd/vector_pack_simd.hpp>
#include <hpx/execution/traits/vector_pack_alignment_size.hpp>

#include <cstddef>

namespace hpx::parallel::traits {

    ///////////////////////////////////////////////////////////////////////
    template <typename Vector, HPX_CONCEPT_REQUIRES_(is_vector_pack_v<Vector>)>
    HPX_HOST_DEVICE HPX_FORCEINLINE auto get(
        Vector& vec, std::size_t index) noexcept
    {
        return vec.get(index);
    }

    template <typename Scalar,
        HPX_CONCEPT_REQUIRES_(is_scalar_vector_pack_v<Scalar>)>
    HPX_HOST_DEVICE HPX_FORCEINLINE auto get(
        Scalar& sc, [[maybe_unused]] std::size_t index) noexcept
    {
        HPX_ASSERT(index == 0);
        return sc;
    }

    ///////////////////////////////////////////////////////////////////////
    template <typename Vector, typename T,
        HPX_CONCEPT_REQUIRES_(is_vector_pack_v<Vector>)>
    HPX_HOST_DEVICE HPX_FORCEINLINE auto set(
        Vector& vec, std::size_t index, T val) noexcept
    {
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wuninitialized"
#endif

        vec.set(index, val);

#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
    }

    template <typename Scalar, typename T,
        HPX_CONCEPT_REQUIRES_(is_scalar_vector_pack_v<Scalar>)>
    HPX_HOST_DEVICE HPX_FORCEINLINE auto set(
        Scalar& sc, [[maybe_unused]] std::size_t index, T val) noexcept
    {
        HPX_ASSERT(index == 0);
        sc = val;
    }
}    // namespace hpx::parallel::traits

#endif
