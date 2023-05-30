// Copyright 2020-2023 Daniel Lemire
// Copyright 2023 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt
//
// Derivative of: https://github.com/fastfloat/fast_float

#ifndef BOOST_CHARCONV_DETAIL_FAST_FLOAT_DIGIT_COMPARISON_HPP
#define BOOST_CHARCONV_DETAIL_FAST_FLOAT_DIGIT_COMPARISON_HPP

#include <boost/charconv/detail/fast_float/float_common.hpp>
#include <boost/charconv/detail/fast_float/ascii_number.hpp>
#include <boost/charconv/detail/fast_float/bigint.hpp>
#include <boost/charconv/detail/config.hpp>
#include <algorithm>
#include <iterator>
#include <cstdint>
#include <cstring>

#ifndef BOOST_CHARCONV_HAS_STD_BITCAST
#  include <boost/core/bit.hpp>
#endif

namespace boost { namespace charconv { namespace detail { namespace fast_float {

// 1e0 to 1e19
static constexpr std::uint64_t powers_of_ten_uint64[] = {
    UINT64_C(1), UINT64_C(10), UINT64_C(100), UINT64_C(1000), UINT64_C(10000), UINT64_C(100000), UINT64_C(1000000), 
    UINT64_C(10000000), UINT64_C(100000000), UINT64_C(1000000000), UINT64_C(10000000000), UINT64_C(100000000000), 
    UINT64_C(1000000000000), UINT64_C(10000000000000), UINT64_C(100000000000000), UINT64_C(1000000000000000), 
    UINT64_C(10000000000000000), UINT64_C(100000000000000000), UINT64_C(1000000000000000000), 
    UINT64_C(10000000000000000000)
    };

// calculate the exponent, in scientific notation, of the number.
// this algorithm is not even close to optimized, but it has no practical
// effect on performance: in order to have a faster algorithm, we'd need
// to slow down performance for faster algorithms, and this is still fast.
template <typename UC>
BOOST_FORCEINLINE BOOST_CHARCONV_CXX14_CONSTEXPR_NO_INLINE 
std::int32_t scientific_exponent(parsed_number_string_t<UC> & num) noexcept 
{
    std::uint64_t mantissa = num.mantissa;
    std::int32_t exponent = static_cast<std::int32_t>(num.exponent);
    while (mantissa >= 10000)
    {
        mantissa /= 10000;
        exponent += 4;
    }
    while (mantissa >= 100)
    {
        mantissa /= 100;
        exponent += 2;
    }
    while (mantissa >= 10)
    {
        mantissa /= 10;
        exponent += 1;
    }
    
    return exponent;
}

// this converts a native floating-point number to an extended-precision float.
template <typename T>
BOOST_FORCEINLINE BOOST_CHARCONV_CXX20_CONSTEXPR 
adjusted_mantissa to_extended(T value) noexcept
{
    using equiv_uint = typename binary_format<T>::equiv_uint;
    constexpr equiv_uint exponent_mask = binary_format<T>::exponent_mask();
    constexpr equiv_uint mantissa_mask = binary_format<T>::mantissa_mask();
    constexpr equiv_uint hidden_bit_mask = binary_format<T>::hidden_bit_mask();

    adjusted_mantissa am;
    std::int32_t bias = binary_format<T>::mantissa_explicit_bits() - binary_format<T>::minimum_exponent();
    equiv_uint bits;
    
    #ifdef BOOST_CHARCONV_HAS_STD_BITCAST
    
    bits = std::bit_cast<equiv_uint>(value);
    
    #else
    
    bits = boost::core::bit_cast<equiv_uint>(value);
    
    #endif
    
    if ((bits & exponent_mask) == 0)
    {
        // denormal
        am.power2 = 1 - bias;
        am.mantissa = bits & mantissa_mask;
    }
    else 
    {
        // normal
        am.power2 = static_cast<std::int32_t>((bits & exponent_mask) >> binary_format<T>::mantissa_explicit_bits());
        am.power2 -= bias;
        am.mantissa = (bits & mantissa_mask) | hidden_bit_mask;
    }

    return am;
}

// get the extended precision value of the halfway point between b and b+u.
// we are given a native float that represents b, so we need to adjust it
// halfway between b and b+u.
template <typename T>
BOOST_FORCEINLINE BOOST_CHARCONV_CXX20_CONSTEXPR 
adjusted_mantissa to_extended_halfway(T value) noexcept
{
    adjusted_mantissa am = to_extended(value);
    am.mantissa <<= 1;
    am.mantissa += 1;
    am.power2 -= 1;
    return am;
}

// round an extended-precision float to the nearest machine float.
template <typename T, typename callback>
BOOST_FORCEINLINE BOOST_CHARCONV_CXX14_CONSTEXPR_NO_INLINE 
void round(adjusted_mantissa& am, callback cb) noexcept
{
    std::int32_t mantissa_shift = 64 - binary_format<T>::mantissa_explicit_bits() - 1;
    if (-am.power2 >= mantissa_shift)
    {
        // have a denormal float
        int32_t shift = -am.power2 + 1;
        cb(am, std::min<int32_t>(shift, 64));
        // check for round-up: if rounding-nearest carried us to the hidden bit.
        am.power2 = (am.mantissa < (uint64_t(1) << binary_format<T>::mantissa_explicit_bits())) ? 0 : 1;
        return;
    }

    // have a normal float, use the default shift.
    cb(am, mantissa_shift);

    // check for carry
    if (am.mantissa >= (uint64_t(2) << binary_format<T>::mantissa_explicit_bits())) 
    {
        am.mantissa = (uint64_t(1) << binary_format<T>::mantissa_explicit_bits());
        am.power2++;
    }

    // check for infinite: we could have carried to an infinite power
    am.mantissa &= ~(uint64_t(1) << binary_format<T>::mantissa_explicit_bits());
    if (am.power2 >= binary_format<T>::infinite_power())
    {
        am.power2 = binary_format<T>::infinite_power();
        am.mantissa = 0;
    }
}

template <typename callback>
BOOST_FORCEINLINE BOOST_CHARCONV_CXX14_CONSTEXPR_NO_INLINE 
void round_nearest_tie_even(adjusted_mantissa& am, std::int32_t shift, callback cb) noexcept
{
    const std::uint64_t mask = (shift == 64) ? UINT64_MAX : (UINT64_C(1) << shift) - 1;
    const std::uint64_t halfway = (shift == 0) ? 0 : UINT64_C(1) << (shift - 1);
    uint64_t truncated_bits = am.mantissa & mask;
    bool is_above = truncated_bits > halfway;
    bool is_halfway = truncated_bits == halfway;

    // shift digits into position
    if (shift == 64)
    {
        am.mantissa = 0;
    } 
    else
    {
        am.mantissa >>= shift;
    }
    am.power2 += shift;

    bool is_odd = (am.mantissa & 1) == 1;
    am.mantissa += static_cast<std::uint64_t>(cb(is_odd, is_halfway, is_above));
}

BOOST_FORCEINLINE BOOST_CHARCONV_CXX14_CONSTEXPR_NO_INLINE
void round_down(adjusted_mantissa& am, std::int32_t shift) noexcept
{
    if (shift == 64) 
    {
        am.mantissa = 0;
    } 
    else 
    {
        am.mantissa >>= shift;
    }
    
    am.power2 += shift;
}

template <typename UC>
BOOST_FORCEINLINE BOOST_CHARCONV_CXX20_CONSTEXPR
void skip_zeros(const UC* & first, const UC* last) noexcept 
{
    uint64_t val;
    while (!BOOST_CHARCONV_IS_CONSTANT_EVALUATED(first) && std::distance(first, last) >= int_cmp_len<UC>())
    {
        ::memcpy(&val, first, sizeof(std::uint64_t));
        if (val != int_cmp_zeros<UC>())
        {
            break;
        }
        first += int_cmp_len<UC>();
    }
    while (first != last)
    {
        if (*first != static_cast<UC>('0')) 
        {
            break;
        }

        first++;
    }
}

// determine if any non-zero digits were truncated.
// all characters must be valid digits.
template <typename UC>
BOOST_FORCEINLINE BOOST_CHARCONV_CXX20_CONSTEXPR
bool is_truncated(const UC* first, const UC* last) noexcept
{
    // do 8-bit optimizations, can just compare to 8 literal 0s.
    uint64_t val;
    while (!BOOST_CHARCONV_IS_CONSTANT_EVALUATED(first) && std::distance(first, last) >= int_cmp_len<UC>())
    {
        ::memcpy(&val, first, sizeof(std::uint64_t));
        if (val != int_cmp_zeros<UC>())
        {
            return true;
        }

        first += int_cmp_len<UC>();
    }
    while (first != last)
    {
        if (*first != static_cast<UC>('0'))
        {
            return true;
        }

        ++first;
    }
    return false;
}

template <typename UC>
BOOST_FORCEINLINE BOOST_CHARCONV_CXX20_CONSTEXPR
bool is_truncated(span<const UC> s) noexcept
{
    return is_truncated(s.ptr, s.ptr + s.len());
}

BOOST_FORCEINLINE BOOST_CHARCONV_CXX20_CONSTEXPR
void parse_eight_digits(const char16_t*& , limb& , std::size_t& , std::size_t& ) noexcept
{
    // currently unused
}

BOOST_FORCEINLINE BOOST_CHARCONV_CXX20_CONSTEXPR
void parse_eight_digits(const char32_t*& , limb& , std::size_t& , std::size_t& ) noexcept
{
    // currently unused
}

BOOST_FORCEINLINE BOOST_CHARCONV_CXX20_CONSTEXPR
void parse_eight_digits(const char*& p, limb& value, std::size_t& counter, std::size_t& count) noexcept
{
    value = value * 100000000 + parse_eight_digits_unrolled(p);
    p += 8;
    counter += 8;
    count += 8;
}

template <typename UC>
BOOST_FORCEINLINE BOOST_CHARCONV_CXX14_CONSTEXPR_NO_INLINE
void parse_one_digit(const UC*& p, limb& value, std::size_t& counter, std::size_t& count) noexcept
{
    value = value * 10 + limb(*p - static_cast<UC>('0'));
    p++;
    counter++;
    count++;
}

BOOST_FORCEINLINE BOOST_CHARCONV_CXX20_CONSTEXPR
void add_native(bigint& big, limb power, limb value) noexcept
{
    big.mul(power);
    big.add(value);
}

BOOST_FORCEINLINE BOOST_CHARCONV_CXX20_CONSTEXPR
void round_up_bigint(bigint& big, size_t& count) noexcept
{
    // need to round-up the digits, but need to avoid rounding
    // ....9999 to ...10000, which could cause a false halfway point.
    add_native(big, 10, 1);
    count++;
}

// parse the significant digits into a big integer
template <typename UC>
inline BOOST_CHARCONV_CXX20_CONSTEXPR
void parse_mantissa(bigint& result, parsed_number_string_t<UC>& num, std::size_t max_digits, std::size_t& digits) noexcept
{
    // try to minimize the number of big integer and scalar multiplication.
    // therefore, try to parse 8 digits at a time, and multiply by the largest
    // scalar value (9 or 19 digits) for each step.
    std::size_t counter = 0;
    digits = 0;
    limb value = 0;
    #ifdef BOOST_CHARCONV_FASTFLOAT_64BIT
    std::size_t step = 19;
    #else
    std::size_t step = 9;
    #endif

    // process all integer digits.
    UC const * p = num.integer.ptr;
    UC const * pend = p + num.integer.len();
    skip_zeros(p, pend);
    // process all digits, in increments of step per loop
    while (p != pend)
    {
        BOOST_IF_CONSTEXPR (std::is_same<UC, char>::value)
        {
            while ((std::distance(p, pend) >= 8) && (step - counter >= 8) && (max_digits - digits >= 8))
            {
                parse_eight_digits(p, value, counter, digits);
            }
        }
        
        while (counter < step && p != pend && digits < max_digits)
        {
            parse_one_digit(p, value, counter, digits);
        }

        if (digits == max_digits) 
        {
            // add the temporary value, then check if we've truncated any digits
            add_native(result, static_cast<limb>(powers_of_ten_uint64[counter]), value);
            bool truncated = is_truncated(p, pend);
            if (num.fraction.ptr != nullptr)
            {
                truncated |= is_truncated(num.fraction);
            }
            if (truncated)
            {
                round_up_bigint(result, digits);
            }
            return;
        } 
        else 
        {
            add_native(result, static_cast<limb>(powers_of_ten_uint64[counter]), value);
            counter = 0;
            value = 0;
        }
    }

    // add our fraction digits, if they're available.
    if (num.fraction.ptr != nullptr)
    {
        p = num.fraction.ptr;
        pend = p + num.fraction.len();
        if (digits == 0) 
        {
            skip_zeros(p, pend);
        }
        // process all digits, in increments of step per loop
        while (p != pend) 
        {
            BOOST_IF_CONSTEXPR (std::is_same<UC,char>::value)
            {
                while ((std::distance(p, pend) >= 8) && (step - counter >= 8) && (max_digits - digits >= 8))
                {
                    parse_eight_digits(p, value, counter, digits);
                }
            }
            while (counter < step && p != pend && digits < max_digits)
            {
                parse_one_digit(p, value, counter, digits);
            }
            if (digits == max_digits)
            {
                // add the temporary value, then check if we've truncated any digits
                add_native(result, static_cast<limb>(powers_of_ten_uint64[counter]), value);
                bool truncated = is_truncated(p, pend);
                if (truncated)
                {
                    round_up_bigint(result, digits);
                }
                return;
            } 
            else 
            {
                add_native(result, static_cast<limb>(powers_of_ten_uint64[counter]), value);
                counter = 0;
                value = 0;
            }
        }
    }

    if (counter != 0)
    {
        add_native(result, static_cast<limb>(powers_of_ten_uint64[counter]), value);
    }
}

#ifdef BOOST_MSVC
# pragma warning(push)
# pragma warning(disable: 4100) // Exponent is marked maybe unused
#endif

template <typename T>
inline BOOST_CHARCONV_CXX20_CONSTEXPR
adjusted_mantissa positive_digit_comp(bigint& bigmant, BOOST_ATTRIBUTE_UNUSED std::int32_t exponent) noexcept
{
    BOOST_CHARCONV_ASSERT(bigmant.pow10(static_cast<std::uint32_t>(exponent)));
    adjusted_mantissa answer;
    bool truncated;
    answer.mantissa = bigmant.hi64(truncated);
    int bias = binary_format<T>::mantissa_explicit_bits() - binary_format<T>::minimum_exponent();
    answer.power2 = bigmant.bit_length() - 64 + bias;

    round<T>(answer, [truncated](adjusted_mantissa& a, std::int32_t shift)
    {
        round_nearest_tie_even(a, shift, [truncated](bool is_odd, bool is_halfway, bool is_above) -> bool
        {
            return is_above || (is_halfway && truncated) || (is_odd && is_halfway);
        });
    });

    return answer;
}

#ifdef BOOST_MSVC
# pragma warning(pop)
#endif 

// the scaling here is quite simple: we have, for the real digits `m * 10^e`,
// and for the theoretical digits `n * 2^f`. Since `e` is always negative,
// to scale them identically, we do `n * 2^f * 5^-f`, so we now have `m * 2^e`.
// we then need to scale by `2^(f- e)`, and then the two significant digits
// are of the same magnitude.
template <typename T>
inline BOOST_CHARCONV_CXX20_CONSTEXPR
adjusted_mantissa negative_digit_comp(bigint& bigmant, adjusted_mantissa am, std::int32_t exponent) noexcept
{
    bigint& real_digits = bigmant;
    std::int32_t real_exp = exponent;

    // get the value of `b`, rounded down, and get a bigint representation of b+h
    adjusted_mantissa am_b = am;
    // gcc7 buf: use a lambda to remove the noexcept qualifier bug with -Wnoexcept-type.
    round<T>(am_b, [](adjusted_mantissa&a, std::int32_t shift) { round_down(a, shift); });
    T b;
    to_float(false, am_b, b);
    adjusted_mantissa theor = to_extended_halfway(b);
    bigint theor_digits(theor.mantissa);
    std::int32_t theor_exp = theor.power2;

    // scale real digits and theor digits to be same power.
    std::int32_t pow2_exp = theor_exp - real_exp;
    std::uint32_t pow5_exp = static_cast<std::uint32_t>(-real_exp);
    if (pow5_exp != 0)
    {
        BOOST_CHARCONV_ASSERT(theor_digits.pow5(pow5_exp));
    }
    if (pow2_exp > 0)
    {
        BOOST_CHARCONV_ASSERT(theor_digits.pow2(static_cast<std::uint32_t>(pow2_exp)));
    }
    else if (pow2_exp < 0)
    {
        BOOST_CHARCONV_ASSERT(real_digits.pow2(static_cast<std::uint32_t>(-pow2_exp)));
    }

    // compare digits, and use it to director rounding
    int ord = real_digits.compare(theor_digits);
    adjusted_mantissa answer = am;
    round<T>(answer, [ord](adjusted_mantissa& a, std::int32_t shift) 
    {
        round_nearest_tie_even(a, shift, [ord](bool is_odd, bool, bool) -> bool 
        {
            //(void)_;  // not needed, since we've done our comparison
            //(void)__; // not needed, since we've done our comparison
            if (ord > 0)
            {
                return true;
            } 
            else if (ord < 0) 
            {
                return false;
            } 

            return is_odd;
        });
    });

    return answer;
}

// parse the significant digits as a big integer to unambiguously round the
// the significant digits. here, we are trying to determine how to round
// an extended float representation close to `b+h`, halfway between `b`
// (the float rounded-down) and `b+u`, the next positive float. this
// algorithm is always correct, and uses one of two approaches. when
// the exponent is positive relative to the significant digits (such as
// 1234), we create a big-integer representation, get the high 64-bits,
// determine if any lower bits are truncated, and use that to direct
// rounding. in case of a negative exponent relative to the significant
// digits (such as 1.2345), we create a theoretical representation of
// `b` as a big-integer type, scaled to the same binary exponent as
// the actual digits. we then compare the big integer representations
// of both, and use that to direct rounding.
template <typename T, typename UC>
inline BOOST_CHARCONV_CXX20_CONSTEXPR
adjusted_mantissa digit_comp(parsed_number_string_t<UC>& num, adjusted_mantissa am) noexcept
{
    // remove the invalid exponent bias
    am.power2 -= invalid_am_bias;

    std::int32_t sci_exp = scientific_exponent(num);
    std::size_t max_digits = binary_format<T>::max_digits();
    std::size_t digits = 0;
    bigint bigmant;
    parse_mantissa(bigmant, num, max_digits, digits);
    // can't underflow, since digits is at most max_digits.
    std::int32_t exponent = sci_exp + 1 - int32_t(digits);
    if (exponent >= 0) 
    {
        return positive_digit_comp<T>(bigmant, exponent);
    }
        
    return negative_digit_comp<T>(bigmant, am, exponent);
}


}}}} // Namespaces

#endif // BOOST_CHARCONV_DETAIL_FAST_FLOAT_DIGIT_COMPARISON_HPP
