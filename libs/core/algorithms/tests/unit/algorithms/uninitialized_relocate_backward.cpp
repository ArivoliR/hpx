//  Copyright (c) 2023 Isidoros Tsaousis-Seiras
//  Copyright (c) 2026 Arivoli Ramamoorthy
//
//  SPDX-License-Identifier: BSL-1.0
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <hpx/init.hpp>
#include <hpx/modules/algorithms.hpp>
#include <hpx/modules/executors.hpp>
#include <hpx/modules/testing.hpp>
#include <hpx/modules/type_support.hpp>

#include <atomic>
#include <random>
#include <set>
#include <utility>

constexpr int N = 500;    // number of objects to construct
constexpr int M = 200;    // number of objects to relocate
constexpr int K = 100;    // number of objects to relocate before throwing

static_assert(N > M);
static_assert(M > K);

using hpx::experimental::is_trivially_relocatable_v;
using hpx::experimental::uninitialized_relocate_backward;

std::mutex m;

template <typename F>
void simple_mutex_operation(F&& f)
{
    std::lock_guard<std::mutex> lk(m);
    f();
}

// enum for the different types of objects
enum relocation_category
{
    trivially_relocatable,
    non_trivially_relocatable,
    non_trivially_relocatable_throwing
};

template <relocation_category c, bool overlapping_test = false>
struct counted_struct
{
    static std::set<counted_struct<c, overlapping_test>*> made;
    static std::atomic<int> moved;
    static std::atomic<int> destroyed;
    int data;

    explicit counted_struct(int data)
      : data(data)
    {
        // Check that we are not constructing an object on top of another
        simple_mutex_operation([&]() {
            HPX_TEST(!made.count(this));
            made.insert(this);
        });
    }

    counted_struct(counted_struct&& other) noexcept(
        c != non_trivially_relocatable_throwing)
      : data(other.data)
    {
        if constexpr (c == non_trivially_relocatable_throwing)
        {
            if (moved++ == K - 1)
            {
                throw 42;
            }
        }
        else
        {
            moved++;
        }

        // Check that we are not constructing an object on top of another
        simple_mutex_operation([&]() {
            // Unless we are testing overlapping relocation
            // we should not be move-constructing an object on top of another
            if constexpr (!overlapping_test)
            {
                HPX_TEST(!made.count(this));
            }
            made.insert(this);
        });
    }

    ~counted_struct()
    {
        destroyed++;

        // Check that the object was constructed
        // and not already destroyed
        simple_mutex_operation([&]() {
            HPX_TEST(made.count(this));
            made.erase(this);
        });
    }

    // making sure the address is never directly accessed
    friend void operator&(counted_struct) = delete;
};

template <relocation_category c, bool overlapping_test>
std::set<counted_struct<c, overlapping_test>*>
    counted_struct<c, overlapping_test>::made;

template <relocation_category c, bool overlapping_test>
std::atomic<int> counted_struct<c, overlapping_test>::moved = 0;

template <relocation_category c, bool overlapping_test>
std::atomic<int> counted_struct<c, overlapping_test>::destroyed = 0;

// Non overlapping relocation testing mechanisms
using trivially_relocatable_struct = counted_struct<trivially_relocatable>;
HPX_DECLARE_TRIVIALLY_RELOCATABLE(trivially_relocatable_struct);

using non_trivially_relocatable_struct =
    counted_struct<non_trivially_relocatable>;

using non_trivially_relocatable_struct_throwing =
    counted_struct<non_trivially_relocatable_throwing>;

// Overlapping relocation testing mechanisms
using trivially_relocatable_struct_overlapping =
    counted_struct<trivially_relocatable, true>;
HPX_DECLARE_TRIVIALLY_RELOCATABLE(trivially_relocatable_struct_overlapping);

using non_trivially_relocatable_struct_overlapping =
    counted_struct<non_trivially_relocatable, true>;

using non_trivially_relocatable_struct_throwing_overlapping =
    counted_struct<non_trivially_relocatable_throwing, true>;

// Check that the correct types are trivially relocatable
static_assert(is_trivially_relocatable_v<trivially_relocatable_struct>);
static_assert(
    is_trivially_relocatable_v<trivially_relocatable_struct_overlapping>);

static_assert(!is_trivially_relocatable_v<non_trivially_relocatable_struct>);
static_assert(
    !is_trivially_relocatable_v<non_trivially_relocatable_struct_overlapping>);

static_assert(
    !is_trivially_relocatable_v<non_trivially_relocatable_struct_throwing>);
static_assert(!is_trivially_relocatable_v<
    non_trivially_relocatable_struct_throwing_overlapping>);

void clear()
{
    // Reset for the next test
    trivially_relocatable_struct::moved = 0;
    trivially_relocatable_struct::destroyed = 0;
    trivially_relocatable_struct::made.clear();

    trivially_relocatable_struct_overlapping::moved = 0;
    trivially_relocatable_struct_overlapping::destroyed = 0;
    trivially_relocatable_struct_overlapping::made.clear();

    non_trivially_relocatable_struct::moved = 0;
    non_trivially_relocatable_struct::destroyed = 0;
    non_trivially_relocatable_struct::made.clear();

    non_trivially_relocatable_struct_overlapping::moved = 0;
    non_trivially_relocatable_struct_overlapping::destroyed = 0;
    non_trivially_relocatable_struct_overlapping::made.clear();

    non_trivially_relocatable_struct_throwing::moved = 0;
    non_trivially_relocatable_struct_throwing::destroyed = 0;
    non_trivially_relocatable_struct_throwing::made.clear();

    non_trivially_relocatable_struct_throwing_overlapping::moved = 0;
    non_trivially_relocatable_struct_throwing_overlapping::destroyed = 0;
    non_trivially_relocatable_struct_throwing_overlapping::made.clear();
}

template <typename T>
std::pair<T*, T*> setup()
{
    clear();

    void* mem1 = std::malloc(N * sizeof(T));
    void* mem2 = std::malloc(N * sizeof(T));

    HPX_TEST(mem1 && mem2);

    T* ptr1 = static_cast<T*>(mem1);
    T* ptr2 = static_cast<T*>(mem2);

    HPX_TEST(T::made.size() == 0);
    HPX_TEST(T::moved == 0);
    HPX_TEST(T::destroyed == 0);

    for (int i = 0; i < N; i++)
    {
        hpx::construct_at(ptr1 + i, i);
    }

    // fill ptr2 with 0 after M
    std::fill(static_cast<std::byte*>(mem2) + M * sizeof(T),
        static_cast<std::byte*>(mem2) + N * sizeof(T), std::byte{0});

    // N objects constructed
    HPX_TEST(T::made.size() == N);

    return {ptr1, ptr2};
}

template <typename Ex>
void test()
{
    {    // Non-overlapping trivially relocatable
        auto [ptr1, ptr2] = setup<trivially_relocatable_struct>();

        for (int i = 0; i < N; i++)
        {
            // Check that the objects were constructed in the first place
            HPX_TEST(trivially_relocatable_struct::made.count(ptr1 + i));
        }

        // relocate M objects to ptr2
        uninitialized_relocate_backward(Ex{}, ptr1, ptr1 + M, ptr2 + M);

        // bookkeeping
        // Artificially add and remove the objects to the set that are where
        // relocated because the relocation is trivial
        for (int i = 0; i < M; i++)
        {
            trivially_relocatable_struct::made.erase(ptr1 + i);
            trivially_relocatable_struct::made.insert(ptr2 + i);
        }

        // No move constructor or destructor should be called
        HPX_TEST(trivially_relocatable_struct::moved == 0);
        HPX_TEST(trivially_relocatable_struct::destroyed == 0);

        // Objects not touched
        for (int i = M; i < N; i++)
        {
            HPX_TEST(ptr1[i].data == i);
        }

        // Objects moved from ptr1 to ptr2
        for (int i = 0; i < M; i++)
        {
            HPX_TEST(ptr2[i].data == i);
        }

        // make sure the memory beyond M is untouched
        for (std::byte* p = reinterpret_cast<std::byte*>(ptr2 + M);
            p < reinterpret_cast<std::byte*>(ptr2 + N); p++)
        {
            HPX_TEST(*p == std::byte{0});
        }

        // From our perspective, the objects in ptr1 are destroyed
        std::destroy(ptr1 + M, ptr1 + N);
        std::destroy(ptr2, ptr2 + M);

        HPX_TEST(trivially_relocatable_struct::made.empty());

        std::free(ptr1);
        std::free(ptr2);
    }
    {    // Non-overlapping non-trivially relocatable
        auto [ptr1, ptr2] = setup<non_trivially_relocatable_struct>();

        // relocate them to ptr2
        uninitialized_relocate_backward(Ex{}, ptr1, ptr1 + M, ptr2 + M);

        // M move constructors were called and M destructors
        HPX_TEST(non_trivially_relocatable_struct::moved == M);
        HPX_TEST(non_trivially_relocatable_struct::destroyed == M);

        // Objects not touched
        for (int i = M; i < N; i++)
        {
            HPX_TEST(ptr1[i].data == i);
        }

        // Objects moved from ptr1 to ptr2
        for (int i = 0; i < M; i++)
        {
            HPX_TEST(ptr2[i].data == i);
        }

        // make sure the memory beyond M is untouched
        for (std::byte* p = reinterpret_cast<std::byte*>(ptr2 + M);
            p < reinterpret_cast<std::byte*>(ptr2 + N); p++)
        {
            HPX_TEST(*p == std::byte{0});
        }

        std::destroy(ptr1 + M, ptr1 + N);
        std::destroy(ptr2, ptr2 + M);

        HPX_TEST(non_trivially_relocatable_struct::made.empty());

        std::free(ptr1);
        std::free(ptr2);
    }
    {    // Non-overlapping non-trivially relocatable throwing
        auto [ptr1, ptr2] = setup<non_trivially_relocatable_struct_throwing>();

        // relocate M objects to ptr2
        try
        {
            uninitialized_relocate_backward(Ex{}, ptr1, ptr1 + M, ptr2 + M);
            HPX_UNREACHABLE;    // should have thrown
        }
        // NOLINTNEXTLINE(bugprone-empty-catch)
        catch (...)
        {
        }

        // If the order is sequenced we can guarantee that no moving
        // occurs after the exception is thrown
        if constexpr (std::is_same_v<Ex, hpx::execution::sequenced_policy>)
        {
            // K move constructors were called, and then the last one throws
            HPX_TEST(non_trivially_relocatable_struct_throwing::moved == K);

            // K - 1 objects where destroyed after being moved-from
            // and then M destructors were called: K on the old range and
            // M - K on the new range
            HPX_TEST(non_trivially_relocatable_struct_throwing::destroyed ==
                K - 1 + M);
        }

        // After the exception is caught, all the objects in the old and the new
        // range are destroyed, so M objects are destroyed on each range
        // In the case of parallel execution, the number of moves done before stopping
        // is arbitrary, since the exception in one thread is not guaranteed to stop
        // all the other threads.
        HPX_TEST(
            non_trivially_relocatable_struct_throwing::made.size() == N - M);

        // The objects in the ends of ptr1 are still valid
        for (int i = M; i < N; i++)
        {
            HPX_TEST(ptr1[i].data == i);
        }

        // make sure the memory beyond M is untouched
        for (std::byte* p = reinterpret_cast<std::byte*>(ptr2 + M);
            p < reinterpret_cast<std::byte*>(ptr2 + N); p++)
        {
            HPX_TEST(*p == std::byte{0});
        }

        std::destroy(ptr1 + M, ptr1 + N);

        HPX_TEST(non_trivially_relocatable_struct_throwing::made.empty());

        std::free(ptr1);
        std::free(ptr2);
    }
    clear();

    return;
}

template <typename Ex>
void test_overlapping()
{
    // using Ex = hpx::execution::sequenced_policy;
    // static_assert(std::is_same_v<Ex, hpx::execution::sequenced_policy>);

    constexpr int offset = 4;

    // relocating M objects `offset` positions forward
    static_assert(M + offset <= N);

    {    // Overlapping trivially-relocatable
        auto [ptr, ___] = setup<trivially_relocatable_struct_overlapping>();

        // Destroy the objects that will be overwritten for bookkeeping
        std::destroy(ptr + M, ptr + M + offset);
        HPX_TEST(trivially_relocatable_struct_overlapping::destroyed == offset);

        // relocate M objects `offset` positions forwards
        uninitialized_relocate_backward(Ex{}, ptr, ptr + M, ptr + M + offset);

        // Artificially remove the objects from the set for bookkeeping
        for (int i = 0; i < M; i++)
        {
            trivially_relocatable_struct_overlapping::made.erase(ptr + i);
        }    // and add the objects that were relocated
        for (int i = offset; i < M + offset; i++)
        {
            trivially_relocatable_struct_overlapping::made.insert(ptr + i);
        }

        // No move constructor or destructor should be called
        // because the objects are trivially relocatable
        HPX_TEST(trivially_relocatable_struct_overlapping::moved == 0);
        HPX_TEST(trivially_relocatable_struct_overlapping::destroyed == offset);

        // Objects relocated forwards
        for (int i = offset; i < M + offset; i++)
        {
            HPX_TEST(ptr[i].data == i - offset);
        }

        // Objects not touched
        for (int i = M + offset; i < N; i++)
        {
            HPX_TEST(ptr[i].data == i);
        }

        // Destroy objects within their lifetime
        // from our perspective objects in the range [M, M + offset) are destroyed
        std::destroy(ptr + offset, ptr + N);

        HPX_TEST(trivially_relocatable_struct_overlapping::made.empty());

        std::free(ptr);
    }
    {    // Overlapping non-trivially relocatable
        auto [ptr, ___] = setup<non_trivially_relocatable_struct_overlapping>();

        // Destroy the objects that will be overwritten for bookkeeping purposes
        std::destroy(ptr + M, ptr + M + offset);
        HPX_TEST(
            non_trivially_relocatable_struct_overlapping::destroyed == offset);

        // relocate backwards them to ptr2
        uninitialized_relocate_backward(Ex{}, ptr, ptr + M, ptr + M + offset);

        // M move constructors were called and M destructors + prior destructors
        HPX_TEST(non_trivially_relocatable_struct_overlapping::moved == M);
        HPX_TEST(non_trivially_relocatable_struct_overlapping::destroyed ==
            M + offset);

        // Objects relocated forwards
        for (int i = offset; i < M + offset; i++)
        {
            HPX_TEST(ptr[i].data == i - offset);
        }

        // Objects not touched
        for (int i = M + offset; i < N; i++)
        {
            HPX_TEST(ptr[i].data == i);
        }

        // Destroy objects within their lifetime
        // objects in the range [M, M + offset) are destroyed
        std::destroy(ptr + offset, ptr + N);

        HPX_TEST(non_trivially_relocatable_struct_overlapping::made.empty());

        std::free(ptr);
    }
    {    // Overlapping non-trivially relocatable throwing
        auto [ptr, ___] =
            setup<non_trivially_relocatable_struct_throwing_overlapping>();

        // Destroy the objects that will be overwritten for bookkeeping purposes
        std::destroy(ptr + M, ptr + M + offset);
        HPX_TEST(
            non_trivially_relocatable_struct_throwing_overlapping::destroyed ==
            offset);

        // relocate them backwards
        try
        {
            uninitialized_relocate_backward(
                Ex{}, ptr, ptr + M, ptr + M + offset);
            HPX_UNREACHABLE;    // should have thrown
        }
        // NOLINTNEXTLINE(bugprone-empty-catch)
        catch (...)
        {
        }

        // Because we know the execution is sequenced:

        // K move constructors were called, and then the last one throws
        HPX_TEST(
            non_trivially_relocatable_struct_throwing_overlapping::moved == K);

        // K - 1 objects where destroyed after being moved-from
        // and then M destructors were called: K on the new range and
        // M - K on the old range
        // + offset from prior the relocation
        HPX_TEST(
            non_trivially_relocatable_struct_throwing_overlapping::destroyed ==
            K - 1 + M + offset);

        // The objects in the end of ptr1 are still valid
        for (int i = M + offset; i < N; i++)
        {
            HPX_TEST(ptr[i].data == i);
        }

        // Destroy objects within their lifetime
        std::destroy(ptr + M + offset, ptr + N);

        HPX_TEST(non_trivially_relocatable_struct_throwing_overlapping::made
                .empty());

        std::free(ptr);
    }
    clear();

    return;
}

template <typename T>
struct non_contiguous_iterator
{
    using value_type = T;
    using difference_type = std::ptrdiff_t;
    using pointer = T*;
    using reference = T&;
    using iterator_category = std::random_access_iterator_tag;

    T* ptr;

    explicit non_contiguous_iterator(T* p) noexcept
      : ptr(p)
    {
    }

    T& operator*() const noexcept { return *ptr; }
    T* operator->() const noexcept { return ptr; }

    non_contiguous_iterator& operator++() noexcept
    {
        ++ptr;
        return *this;
    }
    non_contiguous_iterator operator++(int) noexcept
    {
        auto tmp = *this;
        ++ptr;
        return tmp;
    }
    non_contiguous_iterator& operator--() noexcept
    {
        --ptr;
        return *this;
    }
    non_contiguous_iterator operator--(int) noexcept
    {
        auto tmp = *this;
        --ptr;
        return tmp;
    }
    non_contiguous_iterator& operator+=(difference_type n) noexcept
    {
        ptr += n;
        return *this;
    }
    non_contiguous_iterator& operator-=(difference_type n) noexcept
    {
        ptr -= n;
        return *this;
    }
    non_contiguous_iterator operator+(difference_type n) const noexcept
    {
        return non_contiguous_iterator{ptr + n};
    }
    non_contiguous_iterator operator-(difference_type n) const noexcept
    {
        return non_contiguous_iterator{ptr - n};
    }
    difference_type operator-(
        const non_contiguous_iterator& o) const noexcept
    {
        return ptr - o.ptr;
    }
    T& operator[](difference_type n) const noexcept { return ptr[n]; }
    bool operator==(const non_contiguous_iterator& o) const noexcept
    {
        return ptr == o.ptr;
    }
    bool operator!=(const non_contiguous_iterator& o) const noexcept
    {
        return ptr != o.ptr;
    }
    bool operator<(const non_contiguous_iterator& o) const noexcept
    {
        return ptr < o.ptr;
    }
    bool operator>(const non_contiguous_iterator& o) const noexcept
    {
        return ptr > o.ptr;
    }
    bool operator<=(const non_contiguous_iterator& o) const noexcept
    {
        return ptr <= o.ptr;
    }
    bool operator>=(const non_contiguous_iterator& o) const noexcept
    {
        return ptr >= o.ptr;
    }
    friend non_contiguous_iterator operator+(
        difference_type n, const non_contiguous_iterator& it) noexcept
    {
        return non_contiguous_iterator{it.ptr + n};
    }
};

static_assert(
    !hpx::traits::is_contiguous_iterator_v<
        non_contiguous_iterator<trivially_relocatable_struct>>,
    "non_contiguous_iterator must not be contiguous");

// Test parallel execution with non-contiguous iterators (non-overlapping).
// Also verifies that the return value is dest_first (not dest_last).
template <typename Ex>
void test_non_contiguous()
{
    {    // Non-contiguous non-trivially relocatable
        auto [ptr1, ptr2] = setup<non_trivially_relocatable_struct>();

        using nc_iter = non_contiguous_iterator<non_trivially_relocatable_struct>;

        auto result = uninitialized_relocate_backward(
            Ex{}, nc_iter(ptr1), nc_iter(ptr1 + M), nc_iter(ptr2 + M));

        // Return value must be dest_first (ptr2), not dest_last (ptr2+M)
        HPX_TEST(result.ptr == ptr2);

        HPX_TEST(non_trivially_relocatable_struct::moved == M);
        HPX_TEST(non_trivially_relocatable_struct::destroyed == M);

        for (int i = 0; i < M; i++)
            HPX_TEST(ptr2[i].data == i);
        for (int i = M; i < N; i++)
            HPX_TEST(ptr1[i].data == i);

        std::destroy(ptr1 + M, ptr1 + N);
        std::destroy(ptr2, ptr2 + M);
        HPX_TEST(non_trivially_relocatable_struct::made.empty());

        std::free(ptr1);
        std::free(ptr2);
    }
    {    // Non-contiguous trivially relocatable
        auto [ptr1, ptr2] = setup<trivially_relocatable_struct>();

        using nc_iter = non_contiguous_iterator<trivially_relocatable_struct>;

        auto result = uninitialized_relocate_backward(
            Ex{}, nc_iter(ptr1), nc_iter(ptr1 + M), nc_iter(ptr2 + M));

        // Return value must be dest_first (ptr2)
        HPX_TEST(result.ptr == ptr2);

        for (int i = 0; i < M; i++)
        {
            trivially_relocatable_struct::made.erase(ptr1 + i);
            trivially_relocatable_struct::made.insert(ptr2 + i);
        }

        HPX_TEST(trivially_relocatable_struct::moved == 0);
        HPX_TEST(trivially_relocatable_struct::destroyed == 0);

        for (int i = 0; i < M; i++)
            HPX_TEST(ptr2[i].data == i);
        for (int i = M; i < N; i++)
            HPX_TEST(ptr1[i].data == i);

        std::destroy(ptr1 + M, ptr1 + N);
        std::destroy(ptr2, ptr2 + M);
        HPX_TEST(trivially_relocatable_struct::made.empty());

        std::free(ptr1);
        std::free(ptr2);
    }
    clear();
}

// Verify return value consistency: parallel and sequential must return
// the same value (dest_first, not dest_last).
void test_return_value()
{
    auto [ptr1, ptr2] = setup<non_trivially_relocatable_struct>();

    auto seq_result = uninitialized_relocate_backward(
        hpx::execution::seq, ptr1, ptr1 + M, ptr2 + M);
    HPX_TEST(seq_result == ptr2);

    // reset and redo with parallel policy
    std::destroy(ptr2, ptr2 + M);
    for (int i = 0; i < M; i++)
        hpx::construct_at(ptr1 + i, i);
    non_trivially_relocatable_struct::moved = 0;
    non_trivially_relocatable_struct::destroyed = 0;

    auto par_result = uninitialized_relocate_backward(
        hpx::execution::par, ptr1, ptr1 + M, ptr2 + M);
    HPX_TEST(par_result == ptr2);

    std::destroy(ptr1 + M, ptr1 + N);
    std::destroy(ptr2, ptr2 + M);
    non_trivially_relocatable_struct::made.clear();
    non_trivially_relocatable_struct::moved = 0;
    non_trivially_relocatable_struct::destroyed = 0;

    std::free(ptr1);
    std::free(ptr2);
}

int hpx_main()
{
    test<hpx::execution::sequenced_policy>();
    test<hpx::execution::parallel_policy>();

    test_overlapping<hpx::execution::sequenced_policy>();
    test_overlapping<hpx::execution::parallel_policy>();

    // Test parallel path with non-contiguous iterators
    test_non_contiguous<hpx::execution::parallel_policy>();

    // Verify return value is dest_first for both seq and par
    test_return_value();

    return hpx::local::finalize();
}

int main(int argc, char* argv[])
{
    hpx::local::init(hpx_main, argc, argv);
    return hpx::util::report_errors();
}
