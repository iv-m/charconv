// Copyright 2020-2023 Daniel Lemire
// Copyright 2023 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

// If the architecture (e.g. ARM) does not have __int128 we need to emulate it

#ifndef BOOST_CHARCONV_DETAIL_EMULATED128_HPP
#define BOOST_CHARCONV_DETAIL_EMULATED128_HPP

#include <boost/charconv/detail/config.hpp>
#include <boost/charconv/config.hpp>
#include <boost/core/bit.hpp>
#include <type_traits>
#include <limits>
#include <cstdint>
#include <cassert>
#include <cmath>

namespace boost { namespace charconv { namespace detail {

// Compilers might support built-in 128-bit integer types. However, it seems that
// emulating them with a pair of 64-bit integers actually produces a better code,
// so we avoid using those built-ins. That said, they are still useful for
// implementing 64-bit x 64-bit -> 128-bit multiplication.

struct uint128
{
    std::uint64_t high;
    std::uint64_t low;

    // Constructors
    constexpr uint128() noexcept : high {}, low {} {}

    constexpr uint128(const uint128& v) noexcept = default;

    constexpr uint128(uint128&& v) noexcept = default;

    constexpr uint128(std::uint64_t high_, std::uint64_t low_) noexcept : high {high_}, low {low_} {}

    #define SIGNED_CONSTRUCTOR(expr) explicit constexpr uint128(expr v) noexcept : high {v < 0 ? UINT64_MAX : UINT64_C(0)}, low {static_cast<std::uint64_t>(v)} {}
    #define UNSIGNED_CONSTRUCTOR(expr) explicit constexpr uint128(expr v) noexcept : high {}, low {static_cast<std::uint64_t>(v)} {}

    SIGNED_CONSTRUCTOR(char)
    SIGNED_CONSTRUCTOR(signed char)
    SIGNED_CONSTRUCTOR(short)
    SIGNED_CONSTRUCTOR(int)
    SIGNED_CONSTRUCTOR(long)
    SIGNED_CONSTRUCTOR(long long)

    UNSIGNED_CONSTRUCTOR(unsigned char)
    UNSIGNED_CONSTRUCTOR(unsigned short)
    UNSIGNED_CONSTRUCTOR(unsigned)
    UNSIGNED_CONSTRUCTOR(unsigned long)
    UNSIGNED_CONSTRUCTOR(unsigned long long)

    // Assignment Operators
    #define SIGNED_ASSIGNMENT_OPERATOR(expr) BOOST_CHARCONV_CXX14_CONSTEXPR uint128 &operator=(expr v) noexcept { high = v < 0 ? UINT64_MAX : UINT64_C(0); low = static_cast<std::uint64_t>(v); return *this; }
    #define UNSIGNED_ASSIGNMENT_OPERATOR(expr) BOOST_CHARCONV_CXX14_CONSTEXPR uint128 &operator=(expr v) noexcept { high = 0U; low = static_cast<std::uint64_t>(v); return *this; }

    SIGNED_ASSIGNMENT_OPERATOR(char)
    SIGNED_ASSIGNMENT_OPERATOR(signed char)
    SIGNED_ASSIGNMENT_OPERATOR(short)
    SIGNED_ASSIGNMENT_OPERATOR(int)
    SIGNED_ASSIGNMENT_OPERATOR(long)
    SIGNED_ASSIGNMENT_OPERATOR(long long)

    UNSIGNED_ASSIGNMENT_OPERATOR(unsigned char)
    UNSIGNED_ASSIGNMENT_OPERATOR(unsigned short)
    UNSIGNED_ASSIGNMENT_OPERATOR(unsigned)
    UNSIGNED_ASSIGNMENT_OPERATOR(unsigned long)
    UNSIGNED_ASSIGNMENT_OPERATOR(unsigned long long)

    BOOST_CHARCONV_CXX14_CONSTEXPR uint128 &operator=(uint128 v) noexcept;

    // Conversion Operators
    #define INTEGER_CONVERSION_OPERATOR(expr) BOOST_CHARCONV_CXX14_CONSTEXPR explicit operator expr() const noexcept { return static_cast<expr>(low); }
    #define FLOAT_CONVERSION_OPERATOR(expr) explicit operator expr() const noexcept { return std::ldexp(static_cast<expr>(high), 64) + static_cast<expr>(low); }

    INTEGER_CONVERSION_OPERATOR(char)
    INTEGER_CONVERSION_OPERATOR(signed char)
    INTEGER_CONVERSION_OPERATOR(short)
    INTEGER_CONVERSION_OPERATOR(int)
    INTEGER_CONVERSION_OPERATOR(long)
    INTEGER_CONVERSION_OPERATOR(long long)
    INTEGER_CONVERSION_OPERATOR(unsigned char)
    INTEGER_CONVERSION_OPERATOR(unsigned short)
    INTEGER_CONVERSION_OPERATOR(unsigned)
    INTEGER_CONVERSION_OPERATOR(unsigned long)
    INTEGER_CONVERSION_OPERATOR(unsigned long long)

    FLOAT_CONVERSION_OPERATOR(float)
    FLOAT_CONVERSION_OPERATOR(double)
    FLOAT_CONVERSION_OPERATOR(long double)

    // Comparison Operators

    // Equality
    #define INTEGER_OPERATOR_EQUAL(expr) constexpr friend bool operator==(uint128 lhs, expr rhs) noexcept { return lhs.high == 0 && rhs >= 0 && lhs.low == rhs; }

    INTEGER_OPERATOR_EQUAL(char)
    INTEGER_OPERATOR_EQUAL(signed char)
    INTEGER_OPERATOR_EQUAL(short)
    INTEGER_OPERATOR_EQUAL(int)
    INTEGER_OPERATOR_EQUAL(long)
    INTEGER_OPERATOR_EQUAL(long long)
    INTEGER_OPERATOR_EQUAL(unsigned char)
    INTEGER_OPERATOR_EQUAL(unsigned short)
    INTEGER_OPERATOR_EQUAL(unsigned)
    INTEGER_OPERATOR_EQUAL(unsigned long)
    INTEGER_OPERATOR_EQUAL(unsigned long long)

    constexpr friend bool operator==(uint128 lhs, uint128 rhs) noexcept;

    #undef INTEGER_OPERATOR_EQUAL

    // Inequality
    #define INTEGER_OPERATOR_NOTEQUAL(expr) constexpr friend bool operator!=(uint128 lhs, expr rhs) noexcept { return !(lhs == rhs); }

    INTEGER_OPERATOR_NOTEQUAL(char)
    INTEGER_OPERATOR_NOTEQUAL(signed char)
    INTEGER_OPERATOR_NOTEQUAL(short)
    INTEGER_OPERATOR_NOTEQUAL(int)
    INTEGER_OPERATOR_NOTEQUAL(long)
    INTEGER_OPERATOR_NOTEQUAL(long long)
    INTEGER_OPERATOR_NOTEQUAL(unsigned char)
    INTEGER_OPERATOR_NOTEQUAL(unsigned short)
    INTEGER_OPERATOR_NOTEQUAL(unsigned)
    INTEGER_OPERATOR_NOTEQUAL(unsigned long)
    INTEGER_OPERATOR_NOTEQUAL(unsigned long long)

    constexpr friend bool operator!=(uint128 lhs, uint128 rhs) noexcept;

    #undef INTEGER_OPERATOR_NOTEQUAL

    // Less than
    #define INTEGER_OPERATOR_LESS_THAN(expr) constexpr friend bool operator<(uint128 lhs, expr rhs) noexcept { return lhs.high == 0U && rhs > 0 && lhs.low < rhs; }

    INTEGER_OPERATOR_LESS_THAN(char)
    INTEGER_OPERATOR_LESS_THAN(signed char)
    INTEGER_OPERATOR_LESS_THAN(short)
    INTEGER_OPERATOR_LESS_THAN(int)
    INTEGER_OPERATOR_LESS_THAN(long)
    INTEGER_OPERATOR_LESS_THAN(long long)
    INTEGER_OPERATOR_LESS_THAN(unsigned char)
    INTEGER_OPERATOR_LESS_THAN(unsigned short)
    INTEGER_OPERATOR_LESS_THAN(unsigned)
    INTEGER_OPERATOR_LESS_THAN(unsigned long)
    INTEGER_OPERATOR_LESS_THAN(unsigned long long)

    BOOST_CHARCONV_CXX14_CONSTEXPR friend bool operator<(uint128 lhs, uint128 rhs) noexcept;

    #undef INTEGER_OPERATOR_LESS_THAN

    // Less than or equal to
    #define INTEGER_OPERATOR_LESS_THAN_OR_EQUAL_TO(expr) constexpr friend bool operator<=(uint128 lhs, expr rhs) noexcept { return lhs.high == 0U && rhs >= 0 && lhs.low <= rhs; }

    INTEGER_OPERATOR_LESS_THAN_OR_EQUAL_TO(char)
    INTEGER_OPERATOR_LESS_THAN_OR_EQUAL_TO(signed char)
    INTEGER_OPERATOR_LESS_THAN_OR_EQUAL_TO(short)
    INTEGER_OPERATOR_LESS_THAN_OR_EQUAL_TO(int)
    INTEGER_OPERATOR_LESS_THAN_OR_EQUAL_TO(long)
    INTEGER_OPERATOR_LESS_THAN_OR_EQUAL_TO(long long)
    INTEGER_OPERATOR_LESS_THAN_OR_EQUAL_TO(unsigned char)
    INTEGER_OPERATOR_LESS_THAN_OR_EQUAL_TO(unsigned short)
    INTEGER_OPERATOR_LESS_THAN_OR_EQUAL_TO(unsigned)
    INTEGER_OPERATOR_LESS_THAN_OR_EQUAL_TO(unsigned long)
    INTEGER_OPERATOR_LESS_THAN_OR_EQUAL_TO(unsigned long long)

    BOOST_CHARCONV_CXX14_CONSTEXPR friend bool operator<=(uint128 lhs, uint128 rhs) noexcept;

    #undef INTEGER_OPERATOR_LESS_THAN_OR_EQUAL_TO

    // Greater than
    #define INTEGER_OPERATOR_GREATER_THAN(expr) constexpr friend bool operator>(uint128 lhs, expr rhs) noexcept { return lhs.high > 0U || rhs < 0 || lhs.low > rhs; }

    INTEGER_OPERATOR_GREATER_THAN(char)
    INTEGER_OPERATOR_GREATER_THAN(signed char)
    INTEGER_OPERATOR_GREATER_THAN(short)
    INTEGER_OPERATOR_GREATER_THAN(int)
    INTEGER_OPERATOR_GREATER_THAN(long)
    INTEGER_OPERATOR_GREATER_THAN(long long)
    INTEGER_OPERATOR_GREATER_THAN(unsigned char)
    INTEGER_OPERATOR_GREATER_THAN(unsigned short)
    INTEGER_OPERATOR_GREATER_THAN(unsigned)
    INTEGER_OPERATOR_GREATER_THAN(unsigned long)
    INTEGER_OPERATOR_GREATER_THAN(unsigned long long)

    BOOST_CHARCONV_CXX14_CONSTEXPR friend bool operator>(uint128 lhs, uint128 rhs) noexcept;

    #undef INTEGER_OPERATOR_GREATER_THAN

    // Greater than or equal to
    #define INTEGER_OPERATOR_GREATER_THAN_OR_EQUAL_TO(expr) constexpr friend bool operator>=(uint128 lhs, expr rhs) noexcept { return lhs.high > 0U || rhs < 0 || lhs.low >= rhs; }

    INTEGER_OPERATOR_GREATER_THAN_OR_EQUAL_TO(char)
    INTEGER_OPERATOR_GREATER_THAN_OR_EQUAL_TO(signed char)
    INTEGER_OPERATOR_GREATER_THAN_OR_EQUAL_TO(short)
    INTEGER_OPERATOR_GREATER_THAN_OR_EQUAL_TO(int)
    INTEGER_OPERATOR_GREATER_THAN_OR_EQUAL_TO(long)
    INTEGER_OPERATOR_GREATER_THAN_OR_EQUAL_TO(long long)
    INTEGER_OPERATOR_GREATER_THAN_OR_EQUAL_TO(unsigned char)
    INTEGER_OPERATOR_GREATER_THAN_OR_EQUAL_TO(unsigned short)
    INTEGER_OPERATOR_GREATER_THAN_OR_EQUAL_TO(unsigned)
    INTEGER_OPERATOR_GREATER_THAN_OR_EQUAL_TO(unsigned long)
    INTEGER_OPERATOR_GREATER_THAN_OR_EQUAL_TO(unsigned long long)

BOOST_CHARCONV_CXX14_CONSTEXPR uint128 &uint128::operator=(uint128 v) noexcept
{
    low = v.low;
    high = v.high;
    return *this;
}

constexpr bool operator==(uint128 lhs, uint128 rhs) noexcept
{
    return lhs.high == rhs.high && lhs.low == rhs.low;
}

constexpr bool operator!=(uint128 lhs, uint128 rhs) noexcept
{
    return !(lhs == rhs);
}

BOOST_CHARCONV_CXX14_CONSTEXPR bool operator<(uint128 lhs, uint128 rhs) noexcept
{
    if (lhs.high == rhs.high)
    {
        return lhs.low < rhs.low;
    }

    return lhs.high < rhs.high;
}

BOOST_CHARCONV_CXX14_CONSTEXPR bool operator<=(uint128 lhs, uint128 rhs) noexcept
{
    return !(rhs < lhs);
}

BOOST_CHARCONV_CXX14_CONSTEXPR bool operator>(uint128 lhs, uint128 rhs) noexcept
{
    return rhs < lhs;
}

BOOST_CHARCONV_CXX14_CONSTEXPR bool operator>=(uint128 lhs, uint128 rhs) noexcept
{
    return !(lhs < rhs);
}

        #if BOOST_CHARCONV_HAS_BUILTIN(__builtin_addcll)
        
        unsigned long long carry;
        low = __builtin_addcll(low, n, 0, &carry);
        high = __builtin_addcll(high, 0, carry, &carry);
        
        #elif BOOST_CHARCONV_HAS_BUILTIN(__builtin_ia32_addcarryx_u64)
        
        unsigned long long result;
        auto carry = __builtin_ia32_addcarryx_u64(0, low, n, &result);
        low = result;
        __builtin_ia32_addcarryx_u64(carry, high, 0, &result);
        high = result;
        
        #elif defined(BOOST_MSVC) && defined(_M_X64)
        
        auto carry = _addcarry_u64(0, low, n, &low);
        _addcarry_u64(carry, high, 0, &high);
        
        #else
        
        auto sum = low + n;
        high += (sum < low ? 1 : 0);
        low = sum;
        
        #endif
        return *this;
    }
};

static inline std::uint64_t umul64(std::uint32_t x, std::uint32_t y) noexcept 
{
#if defined(BOOST_CHARCONV_HAS_MSVC_32BIT_INTRINSICS)
    return __emulu(x, y);
#else
    return x * static_cast<std::uint64_t>(y);
#endif
}

// Get 128-bit result of multiplication of two 64-bit unsigned integers.
BOOST_CHARCONV_SAFEBUFFERS inline uint128 umul128(std::uint64_t x, std::uint64_t y) noexcept 
{
    #if defined(BOOST_CHARCONV_HAS_INT128)
    
    auto result = static_cast<boost::uint128_type>(x) * static_cast<boost::uint128_type>(y);
    return {static_cast<std::uint64_t>(result >> 64), static_cast<std::uint64_t>(result)};
    
    #elif defined(BOOST_CHARCONV_HAS_MSVC_64BIT_INTRINSICS)
    
    std::uint64_t high;
    std::uint64_t low = _umul128(x, y, &high);
    return {high, low};
    
    // https://developer.arm.com/documentation/dui0802/a/A64-General-Instructions/UMULH
    #elif defined(__arm__)

    std::uint64_t high = __umulh(x, y);
    std::uint64_t low = x * y;
    return {high, low};

    #else
    
    auto a = static_cast<std::uint32_t>(x >> 32);
    auto b = static_cast<std::uint32_t>(x);
    auto c = static_cast<std::uint32_t>(y >> 32);
    auto d = static_cast<std::uint32_t>(y);

    auto ac = umul64(a, c);
    auto bc = umul64(b, c);
    auto ad = umul64(a, d);
    auto bd = umul64(b, d);

    auto intermediate = (bd >> 32) + static_cast<std::uint32_t>(ad) + static_cast<std::uint32_t>(bc);

    return {ac + (intermediate >> 32) + (ad >> 32) + (bc >> 32),
            (intermediate << 32) + static_cast<std::uint32_t>(bd)};
    
    #endif
}

BOOST_CHARCONV_SAFEBUFFERS inline std::uint64_t umul128_upper64(std::uint64_t x, std::uint64_t y) noexcept
{
    #if defined(BOOST_CHARCONV_HAS_INT128)
    
    auto result = static_cast<boost::uint128_type>(x) * static_cast<boost::uint128_type>(y);
    return static_cast<std::uint64_t>(result >> 64);
    
    #elif defined(BOOST_CHARCONV_HAS_MSVC_64BIT_INTRINSICS)
    
    return __umulh(x, y);
    
    #else
    
    auto a = static_cast<std::uint32_t>(x >> 32);
    auto b = static_cast<std::uint32_t>(x);
    auto c = static_cast<std::uint32_t>(y >> 32);
    auto d = static_cast<std::uint32_t>(y);

    auto ac = umul64(a, c);
    auto bc = umul64(b, c);
    auto ad = umul64(a, d);
    auto bd = umul64(b, d);

    auto intermediate = (bd >> 32) + static_cast<std::uint32_t>(ad) + static_cast<std::uint32_t>(bc);

    return ac + (intermediate >> 32) + (ad >> 32) + (bc >> 32);
    
    #endif
}

// Get upper 128-bits of multiplication of a 64-bit unsigned integer and a 128-bit
// unsigned integer.
BOOST_CHARCONV_SAFEBUFFERS inline uint128 umul192_upper128(std::uint64_t x, uint128 y) noexcept
{
    auto r = umul128(x, y.high);
    r += umul128_upper64(x, y.low);
    return r;
}

// Get upper 64-bits of multiplication of a 32-bit unsigned integer and a 64-bit
// unsigned integer.
inline std::uint64_t umul96_upper64(std::uint32_t x, std::uint64_t y) noexcept 
{
    #if defined(BOOST_CHARCONV_HAS_INT128) || defined(BOOST_CHARCONV_HAS_MSVC_64BIT_INTRINSICS)
    
    return umul128_upper64(static_cast<std::uint64_t>(x) << 32, y);
    
    #else
    
    auto yh = static_cast<std::uint32_t>(y >> 32);
    auto yl = static_cast<std::uint32_t>(y);

    auto xyh = umul64(x, yh);
    auto xyl = umul64(x, yl);

    return xyh + (xyl >> 32);

    #endif
}

// Get lower 128-bits of multiplication of a 64-bit unsigned integer and a 128-bit
// unsigned integer.
BOOST_CHARCONV_SAFEBUFFERS inline uint128 umul192_lower128(std::uint64_t x, uint128 y) noexcept
{
    auto high = x * y.high;
    auto highlow = umul128(x, y.low);
    return {high + highlow.high, highlow.low};
}

// Get lower 64-bits of multiplication of a 32-bit unsigned integer and a 64-bit
// unsigned integer.
inline std::uint64_t umul96_lower64(std::uint32_t x, std::uint64_t y) noexcept 
{
    return x * y;
}

}}} // Namespaces

#endif // BOOST_CHARCONV_DETAIL_EMULATED128_HPP
