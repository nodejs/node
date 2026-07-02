//===-- Utilities to convert floating point values to string ----*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_FLOAT_TO_STRING_H
#define LLVM_LIBC_SRC___SUPPORT_FLOAT_TO_STRING_H

#include "hdr/stdint_proxy.h"
#include "src/__support/CPP/limits.h"
#include "src/__support/CPP/type_traits.h"
#include "src/__support/FPUtil/FPBits.h"
#include "src/__support/FPUtil/dyadic_float.h"
#include "src/__support/big_int.h"
#include "src/__support/common.h"
#include "src/__support/libc_assert.h"
#include "src/__support/macros/attributes.h"
#include "src/__support/macros/config.h"
#include "src/__support/sign.h"

// This file has 5 compile-time flags to allow the user to configure the float
// to string behavior. These were used to explore tradeoffs during the design
// phase, and can still be used to gain specific properties. Unless you
// specifically know what you're doing, you should leave all these flags off.

// LIBC_COPT_FLOAT_TO_STR_NO_SPECIALIZE_LD
//  This flag disables the separate long double conversion implementation. It is
//  not based on the Ryu algorithm, instead generating the digits by
//  multiplying/dividing the written-out number by 10^9 to get blocks. It's
//  significantly faster than INT_CALC, only about 10x slower than MEGA_TABLE,
//  and is small in binary size. Its downside is that it always calculates all
//  of the digits above the decimal point, making it inefficient for %e calls
//  with large exponents. This specialization overrides other flags, so this
//  flag must be set for other flags to effect the long double behavior.

// LIBC_COPT_FLOAT_TO_STR_USE_MEGA_LONG_DOUBLE_TABLE
//  The Mega Table is ~5 megabytes when compiled. It lists the constants needed
//  to perform the Ryu Printf algorithm (described below) for all long double
//  values. This makes it extremely fast for both doubles and long doubles, in
//  exchange for large binary size.

// LIBC_COPT_FLOAT_TO_STR_USE_DYADIC_FLOAT
//  Dyadic floats are software floating point numbers, and their accuracy can be
//  as high as necessary. This option uses 256 bit dyadic floats to calculate
//  the table values that Ryu Printf needs. This is reasonably fast and very
//  small compared to the Mega Table, but the 256 bit floats only give accurate
//  results for the first ~50 digits of the output. In practice this shouldn't
//  be a problem since long doubles are only accurate for ~35 digits, but the
//  trailing values all being 0s may cause brittle tests to fail.

// LIBC_COPT_FLOAT_TO_STR_USE_INT_CALC
//  Integer Calculation uses wide integers to do the calculations for the Ryu
//  Printf table, which is just as accurate as the Mega Table without requiring
//  as much code size. These integers can be very large (~32KB at max, though
//  always on the stack) to handle the edges of the long double range. They are
//  also very slow, taking multiple seconds on a powerful CPU to calculate the
//  values at the end of the range. If no flag is set, this is used for long
//  doubles, the flag only changes the double behavior.

// LIBC_COPT_FLOAT_TO_STR_NO_TABLE
//  This flag doesn't change the actual calculation method, instead it is used
//  to disable the normal Ryu Printf table for configurations that don't use any
//  table at all.

// Default Config:
//  If no flags are set, doubles use the normal (and much more reasonably sized)
//  Ryu Printf table and long doubles use their specialized implementation. This
//  provides good performance and binary size.

#ifdef LIBC_COPT_FLOAT_TO_STR_USE_MEGA_LONG_DOUBLE_TABLE
#include "src/__support/ryu_long_double_constants.h"
#elif !defined(LIBC_COPT_FLOAT_TO_STR_NO_TABLE)
#include "src/__support/ryu_constants.h"
#else
constexpr size_t IDX_SIZE = 1;
constexpr size_t MID_INT_SIZE = 192;
#endif

// This implementation is based on the Ryu Printf algorithm by Ulf Adams:
// Ulf Adams. 2019. Ryū revisited: printf floating point conversion.
// Proc. ACM Program. Lang. 3, OOPSLA, Article 169 (October 2019), 23 pages.
// https://doi.org/10.1145/3360595

// This version is modified to require significantly less memory (it doesn't use
// a large buffer to store the result).

// The general concept of this algorithm is as follows:
// We want to calculate a 9 digit segment of a floating point number using this
// formula: floor((mantissa * 2^exponent)/10^i) % 10^9.
// To do so normally would involve large integers (~1000 bits for doubles), so
// we use a shortcut. We can avoid calculating 2^exponent / 10^i by using a
// lookup table. The resulting intermediate value needs to be about 192 bits to
// store the result with enough precision. Since this is all being done with
// integers for appropriate precision, we would run into a problem if
// i > exponent since then 2^exponent / 10^i would be less than 1. To correct
// for this, the actual calculation done is 2^(exponent + c) / 10^i, and then
// when multiplying by the mantissa we reverse this by dividing by 2^c, like so:
// floor((mantissa * table[exponent][i])/(2^c)) % 10^9.
// This gives a 9 digit value, which is small enough to fit in a 32 bit integer,
// and that integer is converted into a string as normal, and called a block. In
// this implementation, the most recent block is buffered, so that if rounding
// is necessary the block can be adjusted before being written to the output.
// Any block that is all 9s adds one to the max block counter and doesn't clear
// the buffer because they can cause the block above them to be rounded up.

namespace LIBC_NAMESPACE_DECL {

using BlockInt = uint32_t;
constexpr uint32_t BLOCK_SIZE = 9;
constexpr uint64_t EXP5_9 = 1953125;
constexpr uint64_t EXP10_9 = 1000000000;

using FPBits = fputil::FPBits<long double>;

// Larger numbers prefer a slightly larger constant than is used for the smaller
// numbers.
constexpr size_t CALC_SHIFT_CONST = 128;

namespace internal {

// Returns floor(log_10(2^e)); requires 0 <= e <= 42039.
LIBC_INLINE constexpr uint32_t log10_pow2(uint64_t e) {
  LIBC_ASSERT(e <= 42039 &&
              "Incorrect exponent to perform log10_pow2 approximation.");
  // This approximation is based on the float value for log_10(2). It first
  // gives an incorrect result for our purposes at 42039 (well beyond the 16383
  // maximum for long doubles).

  // To get these constants I first evaluated log_10(2) to get an approximation
  // of 0.301029996. Next I passed that value through a string to double
  // conversion to get an explicit mantissa of 0x13441350fbd738 and an exponent
  // of -2 (which becomes -54 when we shift the mantissa to be a non-fractional
  // number). Next I shifted the mantissa right 12 bits to create more space for
  // the multiplication result, adding 12 to the exponent to compensate. To
  // check that this approximation works for our purposes I used the following
  // python code:
  // for i in range(16384):
  //   if(len(str(2**i)) != (((i*0x13441350fbd)>>42)+1)):
  //     print(i)
  // The reason we add 1 is because this evaluation truncates the result, giving
  // us the floor, whereas counting the digits of the power of 2 gives us the
  // ceiling. With a similar loop I checked the maximum valid value and found
  // 42039.
  return static_cast<uint32_t>((e * 0x13441350fbdll) >> 42);
}

// Same as above, but with different constants.
LIBC_INLINE constexpr uint32_t log2_pow5(uint64_t e) {
  return static_cast<uint32_t>((e * 0x12934f0979bll) >> 39);
}

// Returns 1 + floor(log_10(2^e). This could technically be off by 1 if any
// power of 2 was also a power of 10, but since that doesn't exist this is
// always accurate. This is used to calculate the maximum number of base-10
// digits a given e-bit number could have.
LIBC_INLINE constexpr uint32_t ceil_log10_pow2(uint32_t e) {
  return log10_pow2(e) + 1;
}

LIBC_INLINE constexpr uint32_t div_ceil(uint32_t num, uint32_t denom) {
  return (num + (denom - 1)) / denom;
}

// Returns the maximum number of 9 digit blocks a number described by the given
// index (which is ceil(exponent/16)) and mantissa width could need.
LIBC_INLINE constexpr uint32_t length_for_num(uint32_t idx,
                                              uint32_t mantissa_width) {
  return div_ceil(ceil_log10_pow2(idx) + ceil_log10_pow2(mantissa_width + 1),
                  BLOCK_SIZE);
}

// The formula for the table when i is positive (or zero) is as follows:
// floor(10^(-9i) * 2^(e + c_1) + 1) % (10^9 * 2^c_1)
// Rewritten slightly we get:
// floor(5^(-9i) * 2^(e + c_1 - 9i) + 1) % (10^9 * 2^c_1)

// TODO: Fix long doubles (needs bigger table or alternate algorithm.)
// Currently the table values are generated, which is very slow.
template <size_t INT_SIZE>
LIBC_INLINE constexpr UInt<MID_INT_SIZE> get_table_positive(int exponent,
                                                            size_t i) {
  // INT_SIZE is the size of int that is used for the internal calculations of
  // this function. It should be large enough to hold 2^(exponent+constant), so
  // ~1000 for double and ~16000 for long double. Be warned that the time
  // complexity of exponentiation is O(n^2 * log_2(m)) where n is the number of
  // bits in the number being exponentiated and m is the exponent.
  const int shift_amount =
      static_cast<int>(exponent + CALC_SHIFT_CONST - (BLOCK_SIZE * i));
  if (shift_amount < 0) {
    return 1;
  }
  UInt<INT_SIZE> num(0);
  // MOD_SIZE is one of the limiting factors for how big the constant argument
  // can get, since it needs to be small enough to fit in the result UInt,
  // otherwise we'll get truncation on return.
  constexpr UInt<INT_SIZE> MOD_SIZE =
      (UInt<INT_SIZE>(EXP10_9)
       << (CALC_SHIFT_CONST + (IDX_SIZE > 1 ? IDX_SIZE : 0)));

  num = UInt<INT_SIZE>(1) << (shift_amount);
  if (i > 0) {
    UInt<INT_SIZE> fives(EXP5_9);
    fives.pow_n(i);
    num = num / fives;
  }

  num = num + 1;
  if (num > MOD_SIZE) {
    auto rem = num.div_uint_half_times_pow_2(
                      EXP10_9, CALC_SHIFT_CONST + (IDX_SIZE > 1 ? IDX_SIZE : 0))
                   .value();
    num = rem;
  }
  return num;
}

template <size_t INT_SIZE>
LIBC_INLINE UInt<MID_INT_SIZE> get_table_positive_df(int exponent, size_t i) {
  static_assert(INT_SIZE == 256,
                "Only 256 is supported as an int size right now.");
  // This version uses dyadic floats with 256 bit mantissas to perform the same
  // calculation as above. Due to floating point imprecision it is only accurate
  // for the first 50 digits, but it's much faster. Since even 128 bit long
  // doubles are only accurate to ~35 digits, the 50 digits of accuracy are
  // enough for these floats to be converted back and forth safely. This is
  // ideal for avoiding the size of the long double table.
  const int shift_amount =
      static_cast<int>(exponent + CALC_SHIFT_CONST - (9 * i));
  if (shift_amount < 0) {
    return 1;
  }
  fputil::DyadicFloat<INT_SIZE> num(Sign::POS, 0, 1);
  constexpr UInt<INT_SIZE> MOD_SIZE =
      (UInt<INT_SIZE>(EXP10_9)
       << (CALC_SHIFT_CONST + (IDX_SIZE > 1 ? IDX_SIZE : 0)));

  constexpr UInt<INT_SIZE> FIVE_EXP_MINUS_NINE_MANT{
      {0xf387295d242602a7, 0xfdd7645e011abac9, 0x31680a88f8953030,
       0x89705f4136b4a597}};

  static const fputil::DyadicFloat<INT_SIZE> FIVE_EXP_MINUS_NINE(
      Sign::POS, -276, FIVE_EXP_MINUS_NINE_MANT);

  if (i > 0) {
    fputil::DyadicFloat<INT_SIZE> fives =
        fputil::pow_n(FIVE_EXP_MINUS_NINE, static_cast<uint32_t>(i));
    num = fives;
  }
  num = mul_pow_2(num, shift_amount);

  // Adding one is part of the formula.
  UInt<INT_SIZE> int_num = num.as_mantissa_type() + 1;
  if (int_num > MOD_SIZE) {
    auto rem =
        int_num
            .div_uint_half_times_pow_2(
                EXP10_9, CALC_SHIFT_CONST + (IDX_SIZE > 1 ? IDX_SIZE : 0))
            .value();
    int_num = rem;
  }

  UInt<MID_INT_SIZE> result = int_num;

  return result;
}

// The formula for the table when i is negative (or zero) is as follows:
// floor(10^(-9i) * 2^(c_0 - e)) % (10^9 * 2^c_0)
// Since we know i is always negative, we just take it as unsigned and treat it
// as negative. We do the same with exponent, while they're both always negative
// in theory, in practice they're converted to positive for simpler
// calculations.
// The formula being used looks more like this:
// floor(10^(9*(-i)) * 2^(c_0 + (-e))) % (10^9 * 2^c_0)
template <size_t INT_SIZE>
LIBC_INLINE UInt<MID_INT_SIZE> get_table_negative(int exponent, size_t i) {
  int shift_amount = CALC_SHIFT_CONST - exponent;
  UInt<INT_SIZE> num(1);
  constexpr UInt<INT_SIZE> MOD_SIZE =
      (UInt<INT_SIZE>(EXP10_9)
       << (CALC_SHIFT_CONST + (IDX_SIZE > 1 ? IDX_SIZE : 0)));

  size_t ten_blocks = i;
  size_t five_blocks = 0;
  if (shift_amount < 0) {
    int block_shifts = (-shift_amount) / static_cast<int>(BLOCK_SIZE);
    if (block_shifts < static_cast<int>(ten_blocks)) {
      ten_blocks = ten_blocks - block_shifts;
      five_blocks = block_shifts;
      shift_amount = shift_amount + (block_shifts * BLOCK_SIZE);
    } else {
      ten_blocks = 0;
      five_blocks = i;
      shift_amount = shift_amount + (static_cast<int>(i) * BLOCK_SIZE);
    }
  }

  if (five_blocks > 0) {
    UInt<INT_SIZE> fives(EXP5_9);
    fives.pow_n(five_blocks);
    num = fives;
  }
  if (ten_blocks > 0) {
    UInt<INT_SIZE> tens(EXP10_9);
    tens.pow_n(ten_blocks);
    if (five_blocks <= 0) {
      num = tens;
    } else {
      num *= tens;
    }
  }

  if (shift_amount > 0) {
    num = num << shift_amount;
  } else {
    num = num >> (-shift_amount);
  }
  if (num > MOD_SIZE) {
    auto rem = num.div_uint_half_times_pow_2(
                      EXP10_9, CALC_SHIFT_CONST + (IDX_SIZE > 1 ? IDX_SIZE : 0))
                   .value();
    num = rem;
  }
  return num;
}

template <size_t INT_SIZE>
LIBC_INLINE UInt<MID_INT_SIZE> get_table_negative_df(int exponent, size_t i) {
  static_assert(INT_SIZE == 256,
                "Only 256 is supported as an int size right now.");
  // This version uses dyadic floats with 256 bit mantissas to perform the same
  // calculation as above. Due to floating point imprecision it is only accurate
  // for the first 50 digits, but it's much faster. Since even 128 bit long
  // doubles are only accurate to ~35 digits, the 50 digits of accuracy are
  // enough for these floats to be converted back and forth safely. This is
  // ideal for avoiding the size of the long double table.

  int shift_amount = CALC_SHIFT_CONST - exponent;

  fputil::DyadicFloat<INT_SIZE> num(Sign::POS, 0, 1);
  constexpr UInt<INT_SIZE> MOD_SIZE =
      (UInt<INT_SIZE>(EXP10_9)
       << (CALC_SHIFT_CONST + (IDX_SIZE > 1 ? IDX_SIZE : 0)));

  constexpr UInt<INT_SIZE> TEN_EXP_NINE_MANT(EXP10_9);

  static const fputil::DyadicFloat<INT_SIZE> TEN_EXP_NINE(Sign::POS, 0,
                                                          TEN_EXP_NINE_MANT);

  if (i > 0) {
    fputil::DyadicFloat<INT_SIZE> tens =
        fputil::pow_n(TEN_EXP_NINE, static_cast<uint32_t>(i));
    num = tens;
  }
  num = mul_pow_2(num, shift_amount);

  UInt<INT_SIZE> int_num = num.as_mantissa_type();
  if (int_num > MOD_SIZE) {
    auto rem =
        int_num
            .div_uint_half_times_pow_2(
                EXP10_9, CALC_SHIFT_CONST + (IDX_SIZE > 1 ? IDX_SIZE : 0))
            .value();
    int_num = rem;
  }

  UInt<MID_INT_SIZE> result = int_num;

  return result;
}

LIBC_INLINE uint32_t mul_shift_mod_1e9(const FPBits::StorageType mantissa,
                                       const UInt<MID_INT_SIZE> &large,
                                       const int32_t shift_amount) {
  // make sure the number of bits is always divisible by 64
  UInt<internal::div_ceil(MID_INT_SIZE + FPBits::STORAGE_LEN, 64) * 64> val(
      large);
  val = (val * mantissa) >> shift_amount;
  return static_cast<uint32_t>(
      val.div_uint_half_times_pow_2(static_cast<uint32_t>(EXP10_9), 0).value());
}

} // namespace internal

// Convert floating point values to their string representation.
// Because the result may not fit in a reasonably sized array, the caller must
// request blocks of digits and convert them from integers to strings themself.
// Blocks contain the most digits that can be stored in an BlockInt. This is 9
// digits for a 32 bit int and 18 digits for a 64 bit int.
// The intended use pattern is to create a FloatToString object of the
// appropriate type, then call get_positive_blocks to get an approximate number
// of blocks there are before the decimal point. Now the client code can start
// calling get_positive_block in a loop from the number of positive blocks to
// zero. This will give all digits before the decimal point. Then the user can
// start calling get_negative_block in a loop from 0 until the number of digits
// they need is reached. As an optimization, the client can use
// zero_blocks_after_point to find the number of blocks that are guaranteed to
// be zero after the decimal point and before the non-zero digits. Additionally,
// is_lowest_block will return if the current block is the lowest non-zero
// block.
template <typename T, cpp::enable_if_t<cpp::is_floating_point_v<T>, int> = 0>
class FloatToString {
  fputil::FPBits<T> float_bits;
  int exponent;
  FPBits::StorageType mantissa;

  static constexpr int FRACTION_LEN = fputil::FPBits<T>::FRACTION_LEN;
  static constexpr int EXP_BIAS = fputil::FPBits<T>::EXP_BIAS;

public:
  LIBC_INLINE constexpr FloatToString(T init_float) : float_bits(init_float) {
    exponent = float_bits.get_explicit_exponent();
    mantissa = float_bits.get_explicit_mantissa();

    // Adjust for the width of the mantissa.
    exponent -= FRACTION_LEN;
  }

  LIBC_INLINE constexpr bool is_nan() { return float_bits.is_nan(); }
  LIBC_INLINE constexpr bool is_inf() { return float_bits.is_inf(); }
  LIBC_INLINE constexpr bool is_inf_or_nan() {
    return float_bits.is_inf_or_nan();
  }

  // get_block returns an integer that represents the digits in the requested
  // block.
  LIBC_INLINE constexpr BlockInt get_positive_block(int block_index) {
    if (exponent >= -FRACTION_LEN) {
      // idx is ceil(exponent/16) or 0 if exponent is negative. This is used to
      // find the coarse section of the POW10_SPLIT table that will be used to
      // calculate the 9 digit window, as well as some other related values.
      const uint32_t idx =
          exponent < 0
              ? 0
              : static_cast<uint32_t>(exponent + (IDX_SIZE - 1)) / IDX_SIZE;

      // shift_amount = -(c0 - exponent) = c_0 + 16 * ceil(exponent/16) -
      // exponent

      const uint32_t pos_exp = idx * IDX_SIZE;

      UInt<MID_INT_SIZE> val;

#if defined(LIBC_COPT_FLOAT_TO_STR_USE_DYADIC_FLOAT)
      // ----------------------- DYADIC FLOAT CALC MODE ------------------------
      const int32_t SHIFT_CONST = CALC_SHIFT_CONST;
      val = internal::get_table_positive_df<256>(IDX_SIZE * idx, block_index);
#elif defined(LIBC_COPT_FLOAT_TO_STR_USE_INT_CALC)

      // ---------------------------- INT CALC MODE ----------------------------
      const int32_t SHIFT_CONST = CALC_SHIFT_CONST;
      const uint64_t MAX_POW_2_SIZE =
          pos_exp + CALC_SHIFT_CONST - (BLOCK_SIZE * block_index);
      const uint64_t MAX_POW_5_SIZE =
          internal::log2_pow5(BLOCK_SIZE * block_index);
      const uint64_t MAX_INT_SIZE =
          (MAX_POW_2_SIZE > MAX_POW_5_SIZE) ? MAX_POW_2_SIZE : MAX_POW_5_SIZE;

      if (MAX_INT_SIZE < 1024) {
        val = internal::get_table_positive<1024>(pos_exp, block_index);
      } else if (MAX_INT_SIZE < 2048) {
        val = internal::get_table_positive<2048>(pos_exp, block_index);
      } else if (MAX_INT_SIZE < 4096) {
        val = internal::get_table_positive<4096>(pos_exp, block_index);
      } else if (MAX_INT_SIZE < 8192) {
        val = internal::get_table_positive<8192>(pos_exp, block_index);
      } else if (MAX_INT_SIZE < 16384) {
        val = internal::get_table_positive<16384>(pos_exp, block_index);
      } else {
        val = internal::get_table_positive<16384 + 128>(pos_exp, block_index);
      }
#else
      // ----------------------------- TABLE MODE ------------------------------
      const int32_t SHIFT_CONST = TABLE_SHIFT_CONST;

      val = POW10_SPLIT[POW10_OFFSET[idx] + block_index];
#endif
      const uint32_t shift_amount = SHIFT_CONST + pos_exp - exponent;

      const BlockInt digits =
          internal::mul_shift_mod_1e9(mantissa, val, (int32_t)(shift_amount));
      return digits;
    } else {
      return 0;
    }
  }

  LIBC_INLINE constexpr BlockInt get_negative_block(int block_index) {
    if (exponent < 0) {
      const int32_t idx = -exponent / static_cast<int32_t>(IDX_SIZE);

      UInt<MID_INT_SIZE> val;

      const uint32_t pos_exp = static_cast<uint32_t>(idx * IDX_SIZE);

#if defined(LIBC_COPT_FLOAT_TO_STR_USE_DYADIC_FLOAT)
      // ----------------------- DYADIC FLOAT CALC MODE ------------------------
      const int32_t SHIFT_CONST = CALC_SHIFT_CONST;
      val = internal::get_table_negative_df<256>(pos_exp, block_index + 1);
#elif defined(LIBC_COPT_FLOAT_TO_STR_USE_INT_CALC)
      // ---------------------------- INT CALC MODE ----------------------------
      const int32_t SHIFT_CONST = CALC_SHIFT_CONST;

      const uint64_t NUM_FIVES = (block_index + 1) * BLOCK_SIZE;
      // Round MAX_INT_SIZE up to the nearest 64 (adding 1 because log2_pow5
      // implicitly rounds down).
      const uint64_t MAX_INT_SIZE =
          ((internal::log2_pow5(NUM_FIVES) / 64) + 1) * 64;

      if (MAX_INT_SIZE < 1024) {
        val = internal::get_table_negative<1024>(pos_exp, block_index + 1);
      } else if (MAX_INT_SIZE < 2048) {
        val = internal::get_table_negative<2048>(pos_exp, block_index + 1);
      } else if (MAX_INT_SIZE < 4096) {
        val = internal::get_table_negative<4096>(pos_exp, block_index + 1);
      } else if (MAX_INT_SIZE < 8192) {
        val = internal::get_table_negative<8192>(pos_exp, block_index + 1);
      } else if (MAX_INT_SIZE < 16384) {
        val = internal::get_table_negative<16384>(pos_exp, block_index + 1);
      } else {
        val = internal::get_table_negative<16384 + 8192>(pos_exp,
                                                         block_index + 1);
      }
#else
      // ----------------------------- TABLE MODE ------------------------------
      // if the requested block is zero
      const int32_t SHIFT_CONST = TABLE_SHIFT_CONST;
      if (block_index < MIN_BLOCK_2[idx]) {
        return 0;
      }
      const uint32_t p = POW10_OFFSET_2[idx] + block_index - MIN_BLOCK_2[idx];
      // If every digit after the requested block is zero.
      if (p >= POW10_OFFSET_2[idx + 1]) {
        return 0;
      }

      val = POW10_SPLIT_2[p];
#endif
      const int32_t shift_amount =
          SHIFT_CONST + (-exponent - static_cast<int32_t>(pos_exp));
      BlockInt digits =
          internal::mul_shift_mod_1e9(mantissa, val, shift_amount);
      return digits;
    } else {
      return 0;
    }
  }

  LIBC_INLINE constexpr BlockInt get_block(int block_index) {
    if (block_index >= 0) {
      return get_positive_block(block_index);
    } else {
      return get_negative_block(-1 - block_index);
    }
  }

  LIBC_INLINE constexpr size_t get_positive_blocks() {
    if (exponent < -FRACTION_LEN)
      return 0;
    const uint32_t idx =
        exponent < 0
            ? 0
            : static_cast<uint32_t>(exponent + (IDX_SIZE - 1)) / IDX_SIZE;
    return internal::length_for_num(idx * IDX_SIZE, FRACTION_LEN);
  }

  // This takes the index of a block after the decimal point (a negative block)
  // and return if it's sure that all of the digits after it are zero.
  LIBC_INLINE constexpr bool is_lowest_block(size_t negative_block_index) {
#ifdef LIBC_COPT_FLOAT_TO_STR_NO_TABLE
    // The decimal representation of 2**(-i) will have exactly i digits after
    // the decimal point.
    int num_requested_digits =
        static_cast<int>((negative_block_index + 1) * BLOCK_SIZE);

    return num_requested_digits > -exponent;
#else
    const int32_t idx = -exponent / static_cast<int32_t>(IDX_SIZE);
    const size_t p =
        POW10_OFFSET_2[idx] + negative_block_index - MIN_BLOCK_2[idx];
    // If the remaining digits are all 0, then this is the lowest block.
    return p >= POW10_OFFSET_2[idx + 1];
#endif
  }

  LIBC_INLINE constexpr size_t zero_blocks_after_point() {
#ifdef LIBC_COPT_FLOAT_TO_STR_NO_TABLE
    if (exponent < -FRACTION_LEN) {
      const int pos_exp = -exponent - 1;
      const uint32_t pos_idx =
          static_cast<uint32_t>(pos_exp + (IDX_SIZE - 1)) / IDX_SIZE;
      const int32_t pos_len = ((internal::ceil_log10_pow2(pos_idx * IDX_SIZE) -
                                internal::ceil_log10_pow2(FRACTION_LEN + 1)) /
                               BLOCK_SIZE) -
                              1;
      return static_cast<uint32_t>(pos_len > 0 ? pos_len : 0);
    }
    return 0;
#else
    return MIN_BLOCK_2[-exponent / static_cast<int32_t>(IDX_SIZE)];
#endif
  }
};

#if !defined(LIBC_TYPES_LONG_DOUBLE_IS_FLOAT64) &&                             \
    !defined(LIBC_TYPES_LONG_DOUBLE_IS_DOUBLE_DOUBLE) &&                       \
    !defined(LIBC_COPT_FLOAT_TO_STR_NO_SPECIALIZE_LD)
// --------------------------- LONG DOUBLE FUNCTIONS ---------------------------

// this algorithm will work exactly the same for 80 bit and 128 bit long
// doubles. They have the same max exponent, but even if they didn't the
// constants should be calculated to be correct for any provided floating point
// type.

template <> class FloatToString<long double> {
  fputil::FPBits<long double> float_bits;
  bool is_negative = 0;
  int exponent = 0;
  fputil::FPBits<long double>::StorageType mantissa = 0;

  static constexpr int FRACTION_LEN = fputil::FPBits<long double>::FRACTION_LEN;
  static constexpr int EXP_BIAS = fputil::FPBits<long double>::EXP_BIAS;
  static constexpr size_t UINT_WORD_SIZE = 64;

  static constexpr size_t FLOAT_AS_INT_WIDTH =
      internal::div_ceil(
          fputil::FPBits<long double>::MAX_BIASED_EXPONENT -
              fputil::FPBits<long double>::EXP_BIAS +
              FRACTION_LEN, // Add fraction len to provide space for subnormals.
          UINT_WORD_SIZE) *
      UINT_WORD_SIZE;
  static constexpr size_t EXTRA_INT_WIDTH =
      internal::div_ceil(sizeof(long double) * CHAR_BIT, UINT_WORD_SIZE) *
      UINT_WORD_SIZE;

  using wide_int = UInt<FLOAT_AS_INT_WIDTH + EXTRA_INT_WIDTH>;

  // float_as_fixed represents the floating point number as a fixed point number
  // with the point EXTRA_INT_WIDTH bits from the left of the number. This can
  // store any number with a negative exponent.
  wide_int float_as_fixed = 0;
  int int_block_index = 0;

  static constexpr size_t BLOCK_BUFFER_LEN =
      internal::div_ceil(internal::log10_pow2(FLOAT_AS_INT_WIDTH), BLOCK_SIZE) +
      1;
  BlockInt block_buffer[BLOCK_BUFFER_LEN] = {0};
  size_t block_buffer_valid = 0;

  template <size_t Bits>
  LIBC_INLINE static constexpr BlockInt grab_digits(UInt<Bits> &int_num) {
    auto wide_result = int_num.div_uint_half_times_pow_2(EXP5_9, 9);
    // the optional only comes into effect when dividing by 0, which will
    // never happen here. Thus, we just assert that it has value.
    LIBC_ASSERT(wide_result.has_value());
    return static_cast<BlockInt>(wide_result.value());
  }

  LIBC_INLINE static constexpr void zero_leading_digits(wide_int &int_num) {
    // WORD_SIZE is the width of the numbers used to internally represent the
    // UInt
    for (size_t i = 0; i < EXTRA_INT_WIDTH / wide_int::WORD_SIZE; ++i)
      int_num[i + (FLOAT_AS_INT_WIDTH / wide_int::WORD_SIZE)] = 0;
  }

  // init_convert initializes float_as_int, cur_block, and block_buffer based on
  // the mantissa and exponent of the initial number. Calling it will always
  // return the class to the starting state.
  LIBC_INLINE constexpr void init_convert() {
    // No calculation necessary for the 0 case.
    if (mantissa == 0 && exponent == 0)
      return;

    if (exponent > 0) {
      // if the exponent is positive, then the number is fully above the decimal
      // point. In this case we represent the float as an integer, then divide
      // by 10^BLOCK_SIZE and take the remainder as our next block. This
      // generates the digits from right to left, but the digits will be written
      // from left to right, so it caches the results so they can be read in
      // reverse order.

      wide_int float_as_int = mantissa;

      float_as_int <<= exponent;
      int_block_index = 0;

      while (float_as_int > 0) {
        LIBC_ASSERT(int_block_index < static_cast<int>(BLOCK_BUFFER_LEN));
        block_buffer[int_block_index] =
            grab_digits<FLOAT_AS_INT_WIDTH + EXTRA_INT_WIDTH>(float_as_int);
        ++int_block_index;
      }
      block_buffer_valid = int_block_index;

    } else {
      // if the exponent is not positive, then the number is at least partially
      // below the decimal point. In this case we represent the float as a fixed
      // point number with the decimal point after the top EXTRA_INT_WIDTH bits.
      float_as_fixed = mantissa;

      const int SHIFT_AMOUNT = FLOAT_AS_INT_WIDTH + exponent;
      // if the shift amount would be negative, then the shift would cause a
      // loss of precision.
      LIBC_ASSERT(SHIFT_AMOUNT >= 0);
      static_assert(EXTRA_INT_WIDTH >= sizeof(long double) * 8);
      float_as_fixed <<= SHIFT_AMOUNT;

      // If there are still digits above the decimal point, handle those.
      if (cpp::countl_zero(float_as_fixed) <
          static_cast<int>(EXTRA_INT_WIDTH)) {
        UInt<EXTRA_INT_WIDTH> above_decimal_point =
            float_as_fixed >> FLOAT_AS_INT_WIDTH;

        size_t positive_int_block_index = 0;
        while (above_decimal_point > 0) {
          block_buffer[positive_int_block_index] =
              grab_digits<EXTRA_INT_WIDTH>(above_decimal_point);
          ++positive_int_block_index;
        }
        block_buffer_valid = positive_int_block_index;

        // Zero all digits above the decimal point.
        zero_leading_digits(float_as_fixed);
        int_block_index = 0;
      }
    }
  }

public:
  LIBC_INLINE constexpr FloatToString(long double init_float)
      : float_bits(init_float) {
    is_negative = float_bits.is_neg();
    exponent = float_bits.get_explicit_exponent();
    mantissa = float_bits.get_explicit_mantissa();

    // Adjust for the width of the mantissa.
    exponent -= FRACTION_LEN;

    this->init_convert();
  }

  LIBC_INLINE constexpr size_t get_positive_blocks() {
    if (exponent < -FRACTION_LEN)
      return 0;

    const uint32_t idx =
        exponent < 0
            ? 0
            : static_cast<uint32_t>(exponent + (IDX_SIZE - 1)) / IDX_SIZE;
    return internal::length_for_num(idx * IDX_SIZE, FRACTION_LEN);
  }

  LIBC_INLINE constexpr size_t zero_blocks_after_point() {
#ifdef LIBC_COPT_FLOAT_TO_STR_USE_MEGA_LONG_DOUBLE_TABLE
    return MIN_BLOCK_2[-exponent / IDX_SIZE];
#else
    if (exponent >= -FRACTION_LEN)
      return 0;

    const int pos_exp = -exponent - 1;
    const uint32_t pos_idx =
        static_cast<uint32_t>(pos_exp + (IDX_SIZE - 1)) / IDX_SIZE;
    const int32_t pos_len = ((internal::ceil_log10_pow2(pos_idx * IDX_SIZE) -
                              internal::ceil_log10_pow2(FRACTION_LEN + 1)) /
                             BLOCK_SIZE) -
                            1;
    return static_cast<uint32_t>(pos_len > 0 ? pos_len : 0);
#endif
  }

  LIBC_INLINE constexpr bool is_lowest_block(size_t negative_block_index) {
    // The decimal representation of 2**(-i) will have exactly i digits after
    // the decimal point.
    const int num_requested_digits =
        static_cast<int>(negative_block_index * BLOCK_SIZE);

    return num_requested_digits > -exponent;
  }

  LIBC_INLINE constexpr BlockInt get_positive_block(int block_index) {
    if (exponent < -FRACTION_LEN)
      return 0;
    if (block_index > static_cast<int>(block_buffer_valid) || block_index < 0)
      return 0;

    LIBC_ASSERT(block_index < static_cast<int>(BLOCK_BUFFER_LEN));

    return block_buffer[block_index];
  }

  LIBC_INLINE constexpr BlockInt get_negative_block(int negative_block_index) {
    if (exponent >= 0)
      return 0;

    // negative_block_index starts at 0 with the first block after the decimal
    // point, and 1 with the second and so on. This converts to the same
    // block_index used everywhere else.

    const int block_index = -1 - negative_block_index;

    // If we're currently after the requested block (remember these are
    // negative indices) we reset the number to the start. This is only
    // likely to happen in %g calls. This will also reset int_block_index.
    // if (block_index > int_block_index) {
    //   init_convert();
    // }

    // Printf is the only existing user of this code and it will only ever move
    // downwards, except for %g but that currently creates a second
    // float_to_string object so this assertion still holds. If a new user needs
    // the ability to step backwards, uncomment the code above.
    LIBC_ASSERT(block_index <= int_block_index);

    // If we are currently before the requested block. Step until we reach the
    // requested block. This is likely to only be one step.
    while (block_index < int_block_index) {
      zero_leading_digits(float_as_fixed);
      float_as_fixed.mul(EXP10_9);
      --int_block_index;
    }

    // We're now on the requested block, return the current block.
    return static_cast<BlockInt>(float_as_fixed >> FLOAT_AS_INT_WIDTH);
  }

  LIBC_INLINE constexpr BlockInt get_block(int block_index) {
    if (block_index >= 0)
      return get_positive_block(block_index);

    return get_negative_block(-1 - block_index);
  }
};

#endif // !LIBC_TYPES_LONG_DOUBLE_IS_FLOAT64 &&
       // !LIBC_COPT_FLOAT_TO_STR_NO_SPECIALIZE_LD

} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_FLOAT_TO_STRING_H
