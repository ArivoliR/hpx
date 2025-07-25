////////////////////////////////////////////////////////////////////////////////
//  Copyright (c) 2011 Bryce Adelstein-Lelbach
//
//  SPDX-License-Identifier: BSL-1.0
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
////////////////////////////////////////////////////////////////////////////////

#include <hpx/functional/serialization/serializable_function.hpp>
#include <hpx/modules/serialization.hpp>
#include <hpx/serialization/access.hpp>

#include <cstdint>
#include <iostream>

using hpx::function;

///////////////////////////////////////////////////////////////////////////////
struct small_object
{
private:
    std::uint64_t x_;

    friend class hpx::serialization::access;

    template <typename Archive>
    void serialize(Archive& ar, unsigned const)
    {
        ar & x_;
        std::cout << "small_object: serialize(" << x_ << ")\n";
    }

public:
    small_object()
      : x_(0)
    {
        std::cout << "small_object: default ctor\n";
    }

    small_object(std::uint64_t x)
      : x_(x)
    {
        std::cout << "small_object: ctor(" << x << ")\n";
    }

    small_object(small_object const& o)
      : x_(o.x_)
    {
        std::cout << "small_object: copy(" << o.x_ << ")\n";
    }

    small_object& operator=(small_object const& o)
    {
        x_ = o.x_;
        std::cout << "small_object: assign(" << o.x_ << ")\n";
        return *this;
    }

    ~small_object()
    {
        std::cout << "small_object: dtor(" << x_ << ")\n";
    }

    std::uint64_t operator()(std::uint64_t const& z_)
    {
        std::cout << "small_object: call(" << x_ << ", " << z_ << ")\n";
        return x_ + z_;
    }
};

///////////////////////////////////////////////////////////////////////////////
struct big_object
{
private:
    std::uint64_t x_;
    std::uint64_t y_;

    friend class hpx::serialization::access;

    template <typename Archive>
    void serialize(Archive& ar, unsigned const)
    {
        ar & x_;
        ar & y_;
        std::cout << "big_object: serialize(" << x_ << ", " << y_ << ")\n";
    }

public:
    big_object()
      : x_(0)
      , y_(0)
    {
        std::cout << "big_object: default ctor\n";
    }

    big_object(std::uint64_t x, std::uint64_t y)
      : x_(x)
      , y_(y)
    {
        std::cout << "big_object: ctor(" << x << ", " << y << ")\n";
    }

    big_object(big_object const& o)
      : x_(o.x_)
      , y_(o.y_)
    {
        std::cout << "big_object: copy(" << o.x_ << ", " << o.y_ << ")\n";
    }

    big_object& operator=(big_object const& o)
    {
        x_ = o.x_;
        y_ = o.y_;
        std::cout << "big_object: assign(" << o.x_ << ", " << o.y_ << ")\n";
        return *this;
    }

    ~big_object()
    {
        std::cout << "big_object: dtor(" << x_ << ", " << y_ << ")\n";
    }

    std::uint64_t operator()(std::uint64_t const& z_, std::uint64_t const& w_)
    {
        std::cout << "big_object: call(" << x_ << ", " << y_ << z_ << ", " << w_
                  << ")\n";
        return x_ + y_ + z_ + w_;
    }
};

struct foo
{
    void operator()() {}
};

template <typename Archive>
void serialize(Archive&, foo&, unsigned)
{
}

///////////////////////////////////////////////////////////////////////////////
int main()
{
    {
        if (sizeof(small_object) <= hpx::util::detail::function_storage_size)
            std::cout << "object is small\n";
        else
            std::cout << "object is large\n";

        small_object const f(17);

        function<std::uint64_t(std::uint64_t const&)> f0(f);

        function<std::uint64_t(std::uint64_t const&)> f1(f0);

        function<std::uint64_t(std::uint64_t const&)> f2;

        f2 = f0;

        f0(7);
        f1(9);
        f2(11);
    }

    {
        if (sizeof(big_object) <= hpx::util::detail::function_storage_size)
            std::cout << "object is small\n";
        else
            std::cout << "object is large\n";

        big_object const f(5, 12);

        function<std::uint64_t(std::uint64_t const&, std::uint64_t const&)> f0(
            f);

        function<std::uint64_t(std::uint64_t const&, std::uint64_t const&)> f1(
            f0);

        function<std::uint64_t(std::uint64_t const&, std::uint64_t const&)> f2;

        f2 = f0;

        f0(0, 1);
        f1(1, 0);
        f2(1, 1);
    }

    // non serializable version
    {
        if (sizeof(small_object) <= hpx::util::detail::function_storage_size)
            std::cout << "object is small\n";
        else
            std::cout << "object is large\n";

        small_object const f(17);

        function<std::uint64_t(std::uint64_t const&), false> f0(f);

        function<std::uint64_t(std::uint64_t const&), false> f1(f0);

        function<std::uint64_t(std::uint64_t const&), false> f2;

        f2 = f0;

        f0(2);
        f1(4);
        f2(6);
    }

    {
        if (sizeof(big_object) <= hpx::util::detail::function_storage_size)
            std::cout << "object is small\n";
        else
            std::cout << "object is large\n";

        big_object const f(5, 12);

        function<std::uint64_t(std::uint64_t const&, std::uint64_t const&),
            false>
            f0(f);

        function<std::uint64_t(std::uint64_t const&, std::uint64_t const&),
            false>
            f1(f0);

        function<std::uint64_t(std::uint64_t const&, std::uint64_t const&),
            false>
            f2;

        f2 = f0;

        f0(3, 4);
        f1(5, 6);
        f2(7, 8);
    }

    return 0;
}
