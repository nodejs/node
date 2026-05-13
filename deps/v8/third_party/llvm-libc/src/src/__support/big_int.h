//===-- A class to manipulate wide integers. --------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_BIG_INT_H
#define LLVM_LIBC_SRC___SUPPORT_BIG_INT_H

#include "hdr/stdint_proxy.h"
#include "src/__support/CPP/array.h"
#include "src/__support/CPP/bit.h" // countl_zero
#include "src/__support/CPP/limits.h"
#include "src/__support/CPP/optional.h"
#include "src/__support/CPP/type_traits.h"
#include "src/__support/macros/attributes.h" // LIBC_INLINE
#include "src/__support/macros/config.h"
#include "src/__support/macros/optimization.h"        // LIBC_UNLIKELY
#include "src/__support/macros/properties/compiler.h" // LIBC_COMPILER_IS_CLANG
#include "src/__support/macros/properties/types.h" // LIBC_TYPES_HAS_INT128, LIBC_TYPES_HAS_INT64
#include "src/__support/math_extras.h" // add_with_carry, sub_with_borrow
#include "src/__support/number_pair.h"

#include <stddef.h> // For size_t

namespace LIBC_NAMESPACE_DECL {

namespace multiword {

// A type trait mapping unsigned integers to their half-width unsigned
// counterparts.
template <typename T> struct half_width;
template <> struct half_width<uint16_t> : cpp::type_identity<uint8_t> {};
template <> struct half_width<uint32_t> : cpp::type_identity<uint16_t> {};
#ifdef LIBC_TYPES_HAS_INT64
template <> struct half_width<uint64_t> : cpp::type_identity<uint32_t> {};
#ifdef LIBC_TYPES_HAS_INT128
template <> struct half_width<__uint128_t> : cpp::type_identity<uint64_t> {};
#endif // LIBC_TYPES_HAS_INT128
#endif // LIBC_TYPES_HAS_INT64
template <typename T> using half_width_t = typename half_width<T>::type;

// An array of two elements that can be used in multiword operations.
template <typename T> struct DoubleWide final : cpp::array<T, 2> {
  using UP = cpp::array<T, 2>;
  using UP::UP;
  LIBC_INLINE constexpr DoubleWide(T lo, T hi) : UP({lo, hi}) {}
};

// Converts an unsigned value into a DoubleWide<half_width_t<T>>.
template <typename T> LIBC_INLINE constexpr auto split(T value) {
  static_assert(cpp::is_unsigned_v<T>);
  using half_type = half_width_t<T>;
  return DoubleWide<half_type>(
      half_type(value),
      half_type(value >> cpp::numeric_limits<half_type>::digits));
}

// The low part of a DoubleWide value.
template <typename T> LIBC_INLINE constexpr T lo(const DoubleWide<T> &value) {
  return value[0];
}
// The high part of a DoubleWide value.
template <typename T> LIBC_INLINE constexpr T hi(const DoubleWide<T> &value) {
  return value[1];
}
// The low part of an unsigned value.
template <typename T> LIBC_INLINE constexpr half_width_t<T> lo(T value) {
  return lo(split(value));
}
// The high part of an unsigned value.
template <typename T> LIBC_INLINE constexpr half_width_t<T> hi(T value) {
  return hi(split(value));
}

// Returns 'a' times 'b' in a DoubleWide<word>. Cannot overflow by construction.
template <typename word>
LIBC_INLINE constexpr DoubleWide<word> mul2(word a, word b) {
  if constexpr (cpp::is_same_v<word, uint8_t>) {
    return split<uint16_t>(uint16_t(a) * uint16_t(b));
  } else if constexpr (cpp::is_same_v<word, uint16_t>) {
    return split<uint32_t>(uint32_t(a) * uint32_t(b));
  }
#ifdef LIBC_TYPES_HAS_INT64
  else if constexpr (cpp::is_same_v<word, uint32_t>) {
    return split<uint64_t>(uint64_t(a) * uint64_t(b));
  }
#endif
#ifdef LIBC_TYPES_HAS_INT128
  else if constexpr (cpp::is_same_v<word, uint64_t>) {
    return split<__uint128_t>(__uint128_t(a) * __uint128_t(b));
  }
#endif
  else {
    using half_word = half_width_t<word>;
    constexpr auto shiftl = [](word value) -> word {
      return value << cpp::numeric_limits<half_word>::digits;
    };
    constexpr auto shiftr = [](word value) -> word {
      return value >> cpp::numeric_limits<half_word>::digits;
    };
    // Here we do a one digit multiplication where 'a' and 'b' are of type
    // word. We split 'a' and 'b' into half words and perform the classic long
    // multiplication with 'a' and 'b' being two-digit numbers.

    //    a      a_hi a_lo
    //  x b => x b_hi b_lo
    // ----    -----------
    //    c         result
    // We convert 'lo' and 'hi' from 'half_word' to 'word' so multiplication
    // doesn't overflow.
    word a_lo = lo(a);
    word b_lo = lo(b);
    word a_hi = hi(a);
    word b_hi = hi(b);
    word step1 = b_lo * a_lo; // no overflow;
    word step2 = b_lo * a_hi; // no overflow;
    word step3 = b_hi * a_lo; // no overflow;
    word step4 = b_hi * a_hi; // no overflow;
    word lo_digit = step1;
    word hi_digit = step4;
    word no_carry = 0;
    word carry = 0;
    [[maybe_unused]] word _ = 0; // unused carry variable.
    lo_digit = add_with_carry<word>(lo_digit, shiftl(step2), no_carry, carry);
    hi_digit = add_with_carry<word>(hi_digit, shiftr(step2), carry, _);
    lo_digit = add_with_carry<word>(lo_digit, shiftl(step3), no_carry, carry);
    hi_digit = add_with_carry<word>(hi_digit, shiftr(step3), carry, _);
    return DoubleWide<word>(lo_digit, hi_digit);
  }
}

// In-place 'dst op= rhs' with operation with carry propagation. Returns carry.
template <typename Function, typename word, size_t N, size_t M>
LIBC_INLINE constexpr word inplace_binop(Function op_with_carry,
                                         cpp::array<word, N> &dst,
                                         const cpp::array<word, M> &rhs) {
  static_assert(N >= M);
  word carry_out = 0;
  for (size_t i = 0; i < N; ++i) {
    const bool has_rhs_value = i < M;
    const word rhs_value = has_rhs_value ? rhs[i] : 0;
    const word carry_in = carry_out;
    dst[i] = op_with_carry(dst[i], rhs_value, carry_in, carry_out);
    // stop early when rhs is over and no carry is to be propagated.
    if (!has_rhs_value && carry_out == 0)
      break;
  }
  return carry_out;
}

// In-place addition. Returns carry.
template <typename word, size_t N, size_t M>
LIBC_INLINE constexpr word add_with_carry(cpp::array<word, N> &dst,
                                          const cpp::array<word, M> &rhs) {
  return inplace_binop(LIBC_NAMESPACE::add_with_carry<word>, dst, rhs);
}

// In-place subtraction. Returns borrow.
template <typename word, size_t N, size_t M>
LIBC_INLINE constexpr word sub_with_borrow(cpp::array<word, N> &dst,
                                           const cpp::array<word, M> &rhs) {
  return inplace_binop(LIBC_NAMESPACE::sub_with_borrow<word>, dst, rhs);
}

// In-place multiply-add. Returns carry.
// i.e., 'dst += b * c'
template <typename word, size_t N>
LIBC_INLINE constexpr word mul_add_with_carry(cpp::array<word, N> &dst, word b,
                                              word c) {
  return add_with_carry(dst, mul2(b, c));
}

// An array of two elements serving as an accumulator during multiword
// computations.
template <typename T> struct Accumulator final : cpp::array<T, 2> {
  using UP = cpp::array<T, 2>;
  LIBC_INLINE constexpr Accumulator() : UP({0, 0}) {}
  LIBC_INLINE constexpr T advance(T carry_in) {
    auto result = UP::front();
    UP::front() = UP::back();
    UP::back() = carry_in;
    return result;
  }
  LIBC_INLINE constexpr T sum() const { return UP::front(); }
  LIBC_INLINE constexpr T carry() const { return UP::back(); }
};

// In-place multiplication by a single word. Returns carry.
template <typename word, size_t N>
LIBC_INLINE constexpr word scalar_multiply_with_carry(cpp::array<word, N> &dst,
                                                      word x) {
  Accumulator<word> acc;
  for (auto &val : dst) {
    const word carry = mul_add_with_carry(acc, val, x);
    val = acc.advance(carry);
  }
  return acc.carry();
}

// Multiplication of 'lhs' by 'rhs' into 'dst'. Returns carry.
// This function is safe to use for signed numbers.
// https://stackoverflow.com/a/20793834
// https://pages.cs.wisc.edu/%7Emarkhill/cs354/Fall2008/beyond354/int.mult.html
template <typename word, size_t O, size_t M, size_t N>
LIBC_INLINE constexpr word multiply_with_carry(cpp::array<word, O> &dst,
                                               const cpp::array<word, M> &lhs,
                                               const cpp::array<word, N> &rhs) {
  static_assert(O >= M + N);
  Accumulator<word> acc;
  for (size_t i = 0; i < O; ++i) {
    const size_t lower_idx = i < N ? 0 : i - N + 1;
    const size_t upper_idx = i < M ? i : M - 1;
    word carry = 0;
    for (size_t j = lower_idx; j <= upper_idx; ++j)
      carry += mul_add_with_carry(acc, lhs[j], rhs[i - j]);
    dst[i] = acc.advance(carry);
  }
  return acc.carry();
}

template <typename word, size_t N>
LIBC_INLINE constexpr void quick_mul_hi(cpp::array<word, N> &dst,
                                        const cpp::array<word, N> &lhs,
                                        const cpp::array<word, N> &rhs) {
  Accumulator<word> acc;
  word carry = 0;
  // First round of accumulation for those at N - 1 in the full product.
  for (size_t i = 0; i < N; ++i)
    carry += mul_add_with_carry(acc, lhs[i], rhs[N - 1 - i]);
  for (size_t i = N; i < 2 * N - 1; ++i) {
    acc.advance(carry);
    carry = 0;
    for (size_t j = i - N + 1; j < N; ++j)
      carry += mul_add_with_carry(acc, lhs[j], rhs[i - j]);
    dst[i - N] = acc.sum();
  }
  dst.back() = acc.carry();
}

template <typename word, size_t N>
LIBC_INLINE constexpr bool is_negative(const cpp::array<word, N> &array) {
  using signed_word = cpp::make_signed_t<word>;
  return cpp::bit_cast<signed_word>(array.back()) < 0;
}

// An enum for the shift function below.
enum Direction { LEFT, RIGHT };

// A bitwise shift on an array of elements.
// 'offset' must be less than TOTAL_BITS (i.e., sizeof(word) * CHAR_BIT * N)
// otherwise the behavior is undefined.
template <Direction direction, bool is_signed, typename word, size_t N>
LIBC_INLINE constexpr cpp::array<word, N> shift(cpp::array<word, N> array,
                                                size_t offset) {
  static_assert(direction == LEFT || direction == RIGHT);
  constexpr size_t WORD_BITS = cpp::numeric_limits<word>::digits;
#ifdef LIBC_TYPES_HAS_INT128
  constexpr size_t TOTAL_BITS = N * WORD_BITS;
  if constexpr (TOTAL_BITS == 128 &&
                __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__) {
    using type = cpp::conditional_t<is_signed, __int128_t, __uint128_t>;
    auto tmp = cpp::bit_cast<type>(array);
    if constexpr (direction == LEFT)
      tmp <<= offset;
    else
      tmp >>= offset;
    return cpp::bit_cast<cpp::array<word, N>>(tmp);
  }
#endif
  if (LIBC_UNLIKELY(offset == 0))
    return array;
  const bool is_neg = is_signed && is_negative(array);
  constexpr auto at = [](size_t index) -> int {
    // reverse iteration when direction == LEFT.
    if constexpr (direction == LEFT)
      return int(N) - int(index) - 1;
    return int(index);
  };
  const auto safe_get_at = [&](size_t index) -> word {
    // return appropriate value when accessing out of bound elements.
    const int i = at(index);
    if (i < 0)
      return 0;
    if (i >= int(N))
      return is_neg ? cpp::numeric_limits<word>::max() : 0;
    return array[static_cast<unsigned>(i)];
  };
  const size_t index_offset = offset / WORD_BITS;
  const size_t bit_offset = offset % WORD_BITS;
#ifdef LIBC_COMPILER_IS_CLANG
  __builtin_assume(index_offset < N);
#endif
  cpp::array<word, N> out = {};
  for (size_t index = 0; index < N; ++index) {
    const word part1 = safe_get_at(index + index_offset);
    const word part2 = safe_get_at(index + index_offset + 1);
    word &dst = out[static_cast<unsigned>(at(index))];
    if (bit_offset == 0)
      dst = part1; // no crosstalk between parts.
    else if constexpr (direction == LEFT)
      dst = static_cast<word>((part1 << bit_offset) |
                              (part2 >> (WORD_BITS - bit_offset)));
    else
      dst = static_cast<word>((part1 >> bit_offset) |
                              (part2 << (WORD_BITS - bit_offset)));
  }
  return out;
}

#define DECLARE_COUNTBIT(NAME, INDEX_EXPR)                                     \
  template <typename word, size_t N>                                           \
  LIBC_INLINE constexpr int NAME(const cpp::array<word, N> &val) {             \
    int bit_count = 0;                                                         \
    for (size_t i = 0; i < N; ++i) {                                           \
      const int word_count = cpp::NAME<word>(val[INDEX_EXPR]);                 \
      bit_count += word_count;                                                 \
      if (word_count != cpp::numeric_limits<word>::digits)                     \
        break;                                                                 \
    }                                                                          \
    return bit_count;                                                          \
  }

DECLARE_COUNTBIT(countr_zero, i)         // iterating forward
DECLARE_COUNTBIT(countr_one, i)          // iterating forward
DECLARE_COUNTBIT(countl_zero, N - i - 1) // iterating backward
DECLARE_COUNTBIT(countl_one, N - i - 1)  // iterating backward

} // namespace multiword

template <size_t Bits, bool Signed, typename WordType = uint64_t>
struct BigInt {
private:
  static_assert(cpp::is_integral_v<WordType> && cpp::is_unsigned_v<WordType>,
                "WordType must be unsigned integer.");

  struct Division {
    BigInt quotient;
    BigInt remainder;
  };

public:
  using word_type = WordType;
  using unsigned_type = BigInt<Bits, false, word_type>;
  using signed_type = BigInt<Bits, true, word_type>;

  LIBC_INLINE_VAR static constexpr bool SIGNED = Signed;
  LIBC_INLINE_VAR static constexpr size_t BITS = Bits;
  LIBC_INLINE_VAR
  static constexpr size_t WORD_SIZE = sizeof(WordType) * CHAR_BIT;

  static_assert(Bits > 0 && Bits % WORD_SIZE == 0,
                "Number of bits in BigInt should be a multiple of WORD_SIZE.");

  LIBC_INLINE_VAR static constexpr size_t WORD_COUNT = Bits / WORD_SIZE;

  cpp::array<WordType, WORD_COUNT> val{}; // zero initialized.

  LIBC_INLINE constexpr BigInt() = default;

  LIBC_INLINE constexpr BigInt(const BigInt &other) = default;

  template <size_t OtherBits, bool OtherSigned, typename OtherWordType>
  LIBC_INLINE constexpr BigInt(
      const BigInt<OtherBits, OtherSigned, OtherWordType> &other) {
    using BigIntOther = BigInt<OtherBits, OtherSigned, OtherWordType>;
    const bool should_sign_extend = Signed && other.is_neg();

    static_assert(!(Bits == OtherBits && WORD_SIZE != BigIntOther::WORD_SIZE) &&
                  "This is currently untested for casting between bigints with "
                  "the same bit width but different word sizes.");

    if constexpr (BigIntOther::WORD_SIZE < WORD_SIZE) {
      // OtherWordType is smaller
      constexpr size_t WORD_SIZE_RATIO = WORD_SIZE / BigIntOther::WORD_SIZE;
      static_assert(
          (WORD_SIZE % BigIntOther::WORD_SIZE) == 0 &&
          "Word types must be multiples of each other for correct conversion.");
      if constexpr (OtherBits >= Bits) { // truncate
        // for each big word
        for (size_t i = 0; i < WORD_COUNT; ++i) {
          WordType cur_word = 0;
          // combine WORD_SIZE_RATIO small words into a big word
          for (size_t j = 0; j < WORD_SIZE_RATIO; ++j)
            cur_word |= static_cast<WordType>(other[(i * WORD_SIZE_RATIO) + j])
                        << (BigIntOther::WORD_SIZE * j);

          val[i] = cur_word;
        }
      } else { // zero or sign extend
        size_t i = 0;
        WordType cur_word = 0;
        // for each small word
        for (; i < BigIntOther::WORD_COUNT; ++i) {
          // combine WORD_SIZE_RATIO small words into a big word
          cur_word |= static_cast<WordType>(other[i])
                      << (BigIntOther::WORD_SIZE * (i % WORD_SIZE_RATIO));
          // if we've completed a big word, copy it into place and reset
          if ((i % WORD_SIZE_RATIO) == WORD_SIZE_RATIO - 1) {
            val[i / WORD_SIZE_RATIO] = cur_word;
            cur_word = 0;
          }
        }
        // Pretend there are extra words of the correct sign extension as needed

        const WordType extension_bits =
            should_sign_extend ? cpp::numeric_limits<WordType>::max()
                               : cpp::numeric_limits<WordType>::min();
        if ((i % WORD_SIZE_RATIO) != 0) {
          cur_word |= static_cast<WordType>(extension_bits)
                      << (BigIntOther::WORD_SIZE * (i % WORD_SIZE_RATIO));
        }
        // Copy the last word into place.
        val[(i / WORD_SIZE_RATIO)] = cur_word;
        extend((i / WORD_SIZE_RATIO) + 1, should_sign_extend);
      }
    } else if constexpr (BigIntOther::WORD_SIZE == WORD_SIZE) {
      if constexpr (OtherBits >= Bits) { // truncate
        for (size_t i = 0; i < WORD_COUNT; ++i)
          val[i] = other[i];
      } else { // zero or sign extend
        size_t i = 0;
        for (; i < BigIntOther::WORD_COUNT; ++i)
          val[i] = other[i];
        extend(i, should_sign_extend);
      }
    } else {
      // OtherWordType is bigger.
      constexpr size_t WORD_SIZE_RATIO = BigIntOther::WORD_SIZE / WORD_SIZE;
      static_assert(
          (BigIntOther::WORD_SIZE % WORD_SIZE) == 0 &&
          "Word types must be multiples of each other for correct conversion.");
      if constexpr (OtherBits >= Bits) { // truncate
        // for each small word
        for (size_t i = 0; i < WORD_COUNT; ++i) {
          // split each big word into WORD_SIZE_RATIO small words
          val[i] = static_cast<WordType>(other[i / WORD_SIZE_RATIO] >>
                                         ((i % WORD_SIZE_RATIO) * WORD_SIZE));
        }
      } else { // zero or sign extend
        size_t i = 0;
        // for each big word
        for (; i < BigIntOther::WORD_COUNT; ++i) {
          // split each big word into WORD_SIZE_RATIO small words
          for (size_t j = 0; j < WORD_SIZE_RATIO; ++j)
            val[(i * WORD_SIZE_RATIO) + j] =
                static_cast<WordType>(other[i] >> (j * WORD_SIZE));
        }
        extend(i * WORD_SIZE_RATIO, should_sign_extend);
      }
    }
  }

  // Construct a BigInt from a C array.
  template <size_t N> LIBC_INLINE constexpr BigInt(const WordType (&nums)[N]) {
    static_assert(N == WORD_COUNT);
    for (size_t i = 0; i < WORD_COUNT; ++i)
      val[i] = nums[i];
  }

  LIBC_INLINE constexpr explicit BigInt(
      const cpp::array<WordType, WORD_COUNT> &words) {
    val = words;
  }

  // Initialize the first word to |v| and the rest to 0.
  template <typename T, typename = cpp::enable_if_t<cpp::is_integral_v<T>>>
  LIBC_INLINE constexpr BigInt(T v) {
    constexpr size_t T_SIZE = sizeof(T) * CHAR_BIT;
    const bool is_neg = v < 0;
    for (size_t i = 0; i < WORD_COUNT; ++i) {
      if (v == 0) {
        extend(i, is_neg);
        return;
      }
      val[i] = static_cast<WordType>(v);
      if constexpr (T_SIZE > WORD_SIZE)
        v >>= WORD_SIZE;
      else
        v = 0;
    }
  }
  LIBC_INLINE constexpr BigInt &operator=(const BigInt &other) = default;

  // constants
  LIBC_INLINE static constexpr BigInt zero() { return BigInt(); }
  LIBC_INLINE static constexpr BigInt one() { return BigInt(1); }
  LIBC_INLINE static constexpr BigInt all_ones() { return ~zero(); }
  LIBC_INLINE static constexpr BigInt min() {
    BigInt out;
    if constexpr (SIGNED)
      out.set_msb();
    return out;
  }
  LIBC_INLINE static constexpr BigInt max() {
    BigInt out = all_ones();
    if constexpr (SIGNED)
      out.clear_msb();
    return out;
  }

  // TODO: Reuse the Sign type.
  LIBC_INLINE constexpr bool is_neg() const { return SIGNED && get_msb(); }

  template <size_t OtherBits, bool OtherSigned, typename OtherWordType>
  LIBC_INLINE constexpr explicit
  operator BigInt<OtherBits, OtherSigned, OtherWordType>() const {
    return BigInt<OtherBits, OtherSigned, OtherWordType>(this);
  }

  template <typename T> LIBC_INLINE constexpr explicit operator T() const {
    return to<T>();
  }

  template <typename T>
  LIBC_INLINE constexpr cpp::enable_if_t<
      cpp::is_integral_v<T> && !cpp::is_same_v<T, bool>, T>
  to() const {
    constexpr size_t T_SIZE = sizeof(T) * CHAR_BIT;
    T lo = static_cast<T>(val[0]);
    if constexpr (T_SIZE <= WORD_SIZE)
      return lo;
    constexpr size_t MAX_COUNT =
        T_SIZE > Bits ? WORD_COUNT : T_SIZE / WORD_SIZE;
    for (size_t i = 1; i < MAX_COUNT; ++i)
      lo += static_cast<T>(static_cast<T>(val[i]) << (WORD_SIZE * i));
    if constexpr (Signed && (T_SIZE > Bits)) {
      // Extend sign for negative numbers.
      constexpr T MASK = (~T(0) << Bits);
      if (is_neg())
        lo |= MASK;
    }
    return lo;
  }

  LIBC_INLINE constexpr explicit operator bool() const { return !is_zero(); }

  LIBC_INLINE constexpr bool is_zero() const {
    for (auto part : val)
      if (part != 0)
        return false;
    return true;
  }

  // Add 'rhs' to this number and store the result in this number.
  // Returns the carry value produced by the addition operation.
  LIBC_INLINE constexpr WordType add_overflow(const BigInt &rhs) {
    return multiword::add_with_carry(val, rhs.val);
  }

  LIBC_INLINE constexpr BigInt operator+(const BigInt &other) const {
    BigInt result = *this;
    result.add_overflow(other);
    return result;
  }

  // This will only apply when initializing a variable from constant values, so
  // it will always use the constexpr version of add_with_carry.
  LIBC_INLINE constexpr BigInt operator+(BigInt &&other) const {
    // We use addition commutativity to reuse 'other' and prevent allocation.
    other.add_overflow(*this); // Returned carry value is ignored.
    return other;
  }

  LIBC_INLINE constexpr BigInt &operator+=(const BigInt &other) {
    add_overflow(other); // Returned carry value is ignored.
    return *this;
  }

  // Subtract 'rhs' to this number and store the result in this number.
  // Returns the carry value produced by the subtraction operation.
  LIBC_INLINE constexpr WordType sub_overflow(const BigInt &rhs) {
    return multiword::sub_with_borrow(val, rhs.val);
  }

  LIBC_INLINE constexpr BigInt operator-(const BigInt &other) const {
    BigInt result = *this;
    result.sub_overflow(other); // Returned carry value is ignored.
    return result;
  }

  LIBC_INLINE constexpr BigInt operator-(BigInt &&other) const {
    BigInt result = *this;
    result.sub_overflow(other); // Returned carry value is ignored.
    return result;
  }

  LIBC_INLINE constexpr BigInt &operator-=(const BigInt &other) {
    // TODO(lntue): Set overflow flag / errno when carry is true.
    sub_overflow(other); // Returned carry value is ignored.
    return *this;
  }

  // Multiply this number with x and store the result in this number.
  LIBC_INLINE constexpr WordType mul(WordType x) {
    return multiword::scalar_multiply_with_carry(val, x);
  }

  // Return the full product.
  template <size_t OtherBits>
  LIBC_INLINE constexpr auto
  ful_mul(const BigInt<OtherBits, Signed, WordType> &other) const {
    BigInt<Bits + OtherBits, Signed, WordType> result;
    multiword::multiply_with_carry(result.val, val, other.val);
    return result;
  }

  LIBC_INLINE constexpr BigInt operator*(const BigInt &other) const {
    // Perform full mul and truncate.
    return BigInt(ful_mul(other));
  }

  // Fast hi part of the full product.  The normal product `operator*` returns
  // `Bits` least significant bits of the full product, while this function will
  // approximate `Bits` most significant bits of the full product with errors
  // bounded by:
  //   0 <= (a.full_mul(b) >> Bits) - a.quick_mul_hi(b)) <= WORD_COUNT - 1.
  //
  // An example usage of this is to quickly (but less accurately) compute the
  // product of (normalized) mantissas of floating point numbers:
  //   (mant_1, mant_2) -> quick_mul_hi -> normalize leading bit
  // is much more efficient than:
  //   (mant_1, mant_2) -> ful_mul -> normalize leading bit
  //                    -> convert back to same Bits width by shifting/rounding,
  // especially for higher precisions.
  //
  // Performance summary:
  //   Number of 64-bit x 64-bit -> 128-bit multiplications performed.
  //   Bits  WORD_COUNT  ful_mul  quick_mul_hi  Error bound
  //    128      2         4           3            1
  //    196      3         9           6            2
  //    256      4        16          10            3
  //    512      8        64          36            7
  LIBC_INLINE constexpr BigInt quick_mul_hi(const BigInt &other) const {
    BigInt result;
    multiword::quick_mul_hi(result.val, val, other.val);
    return result;
  }

  // BigInt(x).pow_n(n) computes x ^ n.
  // Note 0 ^ 0 == 1.
  LIBC_INLINE constexpr void pow_n(uint64_t power) {
    static_assert(!Signed);
    BigInt result = one();
    BigInt cur_power = *this;
    while (power > 0) {
      if ((power % 2) > 0)
        result *= cur_power;
      power >>= 1;
      cur_power *= cur_power;
    }
    *this = result;
  }

  // Performs inplace signed / unsigned division. Returns remainder if not
  // dividing by zero.
  // For signed numbers it behaves like C++ signed integer division.
  // That is by truncating the fractionnal part
  // https://stackoverflow.com/a/3602857
  LIBC_INLINE constexpr cpp::optional<BigInt> div(const BigInt &divider) {
    if (LIBC_UNLIKELY(divider.is_zero()))
      return cpp::nullopt;
    if (LIBC_UNLIKELY(divider == BigInt::one()))
      return BigInt::zero();
    Division result;
    if constexpr (SIGNED)
      result = divide_signed(*this, divider);
    else
      result = divide_unsigned(*this, divider);
    *this = result.quotient;
    return result.remainder;
  }

  // Efficiently perform BigInt / (x * 2^e), where x is a half-word-size
  // unsigned integer, and return the remainder. The main idea is as follow:
  //   Let q = y / (x * 2^e) be the quotient, and
  //       r = y % (x * 2^e) be the remainder.
  //   First, notice that:
  //     r % (2^e) = y % (2^e),
  // so we just need to focus on all the bits of y that is >= 2^e.
  //   To speed up the shift-and-add steps, we only use x as the divisor, and
  // performing 32-bit shiftings instead of bit-by-bit shiftings.
  //   Since the remainder of each division step < x < 2^(WORD_SIZE / 2), the
  // computation of each step is now properly contained within WordType.
  //   And finally we perform some extra alignment steps for the remaining bits.
  LIBC_INLINE constexpr cpp::optional<BigInt>
  div_uint_half_times_pow_2(multiword::half_width_t<WordType> x, size_t e) {
    BigInt remainder;
    if (x == 0)
      return cpp::nullopt;
    if (e >= Bits) {
      remainder = *this;
      *this = BigInt<Bits, false, WordType>();
      return remainder;
    }
    BigInt quotient;
    WordType x_word = static_cast<WordType>(x);
    constexpr size_t LOG2_WORD_SIZE =
        static_cast<size_t>(cpp::bit_width(WORD_SIZE) - 1);
    constexpr size_t HALF_WORD_SIZE = WORD_SIZE >> 1;
    constexpr WordType HALF_MASK = ((WordType(1) << HALF_WORD_SIZE) - 1);
    // lower = smallest multiple of WORD_SIZE that is >= e.
    size_t lower = ((e >> LOG2_WORD_SIZE) + ((e & (WORD_SIZE - 1)) != 0))
                   << LOG2_WORD_SIZE;
    // lower_pos is the index of the closest WORD_SIZE-bit chunk >= 2^e.
    size_t lower_pos = lower / WORD_SIZE;
    // Keep track of current remainder mod x * 2^(32*i)
    WordType rem = 0;
    // pos is the index of the current 64-bit chunk that we are processing.
    size_t pos = WORD_COUNT;

    // TODO: look into if constexpr(Bits > 256) skip leading zeroes.

    for (size_t q_pos = WORD_COUNT - lower_pos; q_pos > 0; --q_pos) {
      // q_pos is 1 + the index of the current WORD_SIZE-bit chunk of the
      // quotient being processed. Performing the division / modulus with
      // divisor:
      //   x * 2^(WORD_SIZE*q_pos - WORD_SIZE/2),
      // i.e. using the upper (WORD_SIZE/2)-bit of the current WORD_SIZE-bit
      // chunk.
      rem <<= HALF_WORD_SIZE;
      rem += val[--pos] >> HALF_WORD_SIZE;
      WordType q_tmp = rem / x_word;
      rem %= x_word;

      // Performing the division / modulus with divisor:
      //   x * 2^(WORD_SIZE*(q_pos - 1)),
      // i.e. using the lower (WORD_SIZE/2)-bit of the current WORD_SIZE-bit
      // chunk.
      rem <<= HALF_WORD_SIZE;
      rem += val[pos] & HALF_MASK;
      quotient.val[q_pos - 1] = (q_tmp << HALF_WORD_SIZE) + rem / x_word;
      rem %= x_word;
    }

    // So far, what we have is:
    //   quotient = y / (x * 2^lower), and
    //        rem = (y % (x * 2^lower)) / 2^lower.
    // If (lower > e), we will need to perform an extra adjustment of the
    // quotient and remainder, namely:
    //   y / (x * 2^e) = [ y / (x * 2^lower) ] * 2^(lower - e) +
    //                   + (rem * 2^(lower - e)) / x
    //   (y % (x * 2^e)) / 2^e = (rem * 2^(lower - e)) % x
    size_t last_shift = lower - e;

    if (last_shift > 0) {
      // quotient * 2^(lower - e)
      quotient <<= last_shift;
      WordType q_tmp = 0;
      WordType d = val[--pos];
      if (last_shift >= HALF_WORD_SIZE) {
        // The shifting (rem * 2^(lower - e)) might overflow WordTyoe, so we
        // perform a HALF_WORD_SIZE-bit shift first.
        rem <<= HALF_WORD_SIZE;
        rem += d >> HALF_WORD_SIZE;
        d &= HALF_MASK;
        q_tmp = rem / x_word;
        rem %= x_word;
        last_shift -= HALF_WORD_SIZE;
      } else {
        // Only use the upper HALF_WORD_SIZE-bit of the current WORD_SIZE-bit
        // chunk.
        d >>= HALF_WORD_SIZE;
      }

      if (last_shift > 0) {
        rem <<= HALF_WORD_SIZE;
        rem += d;
        q_tmp <<= last_shift;
        x_word <<= HALF_WORD_SIZE - last_shift;
        q_tmp += rem / x_word;
        rem %= x_word;
      }

      quotient.val[0] += q_tmp;

      if (lower - e <= HALF_WORD_SIZE) {
        // The remainder rem * 2^(lower - e) might overflow to the higher
        // WORD_SIZE-bit chunk.
        if (pos < WORD_COUNT - 1) {
          remainder[pos + 1] = rem >> HALF_WORD_SIZE;
        }
        remainder[pos] = (rem << HALF_WORD_SIZE) + (val[pos] & HALF_MASK);
      } else {
        remainder[pos] = rem;
      }

    } else {
      remainder[pos] = rem;
    }

    // Set the remaining lower bits of the remainder.
    for (; pos > 0; --pos) {
      remainder[pos - 1] = val[pos - 1];
    }

    *this = quotient;
    return remainder;
  }

  LIBC_INLINE constexpr BigInt operator/(const BigInt &other) const {
    BigInt result(*this);
    result.div(other);
    return result;
  }

  LIBC_INLINE constexpr BigInt &operator/=(const BigInt &other) {
    div(other);
    return *this;
  }

  LIBC_INLINE constexpr BigInt operator%(const BigInt &other) const {
    BigInt result(*this);
    return *result.div(other);
  }

  LIBC_INLINE constexpr BigInt operator%=(const BigInt &other) {
    *this = *this % other;
    return *this;
  }

  LIBC_INLINE constexpr BigInt &operator*=(const BigInt &other) {
    *this = *this * other;
    return *this;
  }

  LIBC_INLINE constexpr BigInt &operator<<=(size_t s) {
    val = multiword::shift<multiword::LEFT, SIGNED>(val, s);
    return *this;
  }

  LIBC_INLINE constexpr BigInt operator<<(size_t s) const {
    return BigInt(multiword::shift<multiword::LEFT, SIGNED>(val, s));
  }

  LIBC_INLINE constexpr BigInt &operator>>=(size_t s) {
    val = multiword::shift<multiword::RIGHT, SIGNED>(val, s);
    return *this;
  }

  LIBC_INLINE constexpr BigInt operator>>(size_t s) const {
    return BigInt(multiword::shift<multiword::RIGHT, SIGNED>(val, s));
  }

#define DEFINE_BINOP(OP)                                                       \
  LIBC_INLINE friend constexpr BigInt operator OP(const BigInt &lhs,           \
                                                  const BigInt &rhs) {         \
    BigInt result;                                                             \
    for (size_t i = 0; i < WORD_COUNT; ++i)                                    \
      result[i] = lhs[i] OP rhs[i];                                            \
    return result;                                                             \
  }                                                                            \
  LIBC_INLINE friend constexpr BigInt operator OP##=(BigInt &lhs,              \
                                                     const BigInt &rhs) {      \
    for (size_t i = 0; i < WORD_COUNT; ++i)                                    \
      lhs[i] OP## = rhs[i];                                                    \
    return lhs;                                                                \
  }

  DEFINE_BINOP(&) // & and &=
  DEFINE_BINOP(|) // | and |=
  DEFINE_BINOP(^) // ^ and ^=
#undef DEFINE_BINOP

  LIBC_INLINE constexpr BigInt operator~() const {
    BigInt result;
    for (size_t i = 0; i < WORD_COUNT; ++i)
      result[i] = static_cast<WordType>(~val[i]);
    return result;
  }

  LIBC_INLINE constexpr BigInt operator-() const {
    BigInt result(*this);
    result.negate();
    return result;
  }

  LIBC_INLINE friend constexpr bool operator==(const BigInt &lhs,
                                               const BigInt &rhs) {
    for (size_t i = 0; i < WORD_COUNT; ++i)
      if (lhs.val[i] != rhs.val[i])
        return false;
    return true;
  }

  LIBC_INLINE friend constexpr bool operator!=(const BigInt &lhs,
                                               const BigInt &rhs) {
    return !(lhs == rhs);
  }

  LIBC_INLINE friend constexpr bool operator>(const BigInt &lhs,
                                              const BigInt &rhs) {
    return cmp(lhs, rhs) > 0;
  }
  LIBC_INLINE friend constexpr bool operator>=(const BigInt &lhs,
                                               const BigInt &rhs) {
    return cmp(lhs, rhs) >= 0;
  }
  LIBC_INLINE friend constexpr bool operator<(const BigInt &lhs,
                                              const BigInt &rhs) {
    return cmp(lhs, rhs) < 0;
  }
  LIBC_INLINE friend constexpr bool operator<=(const BigInt &lhs,
                                               const BigInt &rhs) {
    return cmp(lhs, rhs) <= 0;
  }

  LIBC_INLINE constexpr BigInt &operator++() {
    increment();
    return *this;
  }

  LIBC_INLINE constexpr BigInt operator++(int) {
    BigInt oldval(*this);
    increment();
    return oldval;
  }

  LIBC_INLINE constexpr BigInt &operator--() {
    decrement();
    return *this;
  }

  LIBC_INLINE constexpr BigInt operator--(int) {
    BigInt oldval(*this);
    decrement();
    return oldval;
  }

  // Return the i-th word of the number.
  LIBC_INLINE constexpr const WordType &operator[](size_t i) const {
    return val[i];
  }

  // Return the i-th word of the number.
  LIBC_INLINE constexpr WordType &operator[](size_t i) { return val[i]; }

  // Return the i-th bit of the number.
  LIBC_INLINE constexpr bool get_bit(size_t i) const {
    const size_t word_index = i / WORD_SIZE;
    return 1 & (val[word_index] >> (i % WORD_SIZE));
  }

  // Set the i-th bit of the number.
  LIBC_INLINE constexpr void set_bit(size_t i) {
    const size_t word_index = i / WORD_SIZE;
    val[word_index] |= WordType(1) << (i % WORD_SIZE);
  }

private:
  LIBC_INLINE friend constexpr int cmp(const BigInt &lhs, const BigInt &rhs) {
    constexpr auto compare = [](WordType a, WordType b) {
      return a == b ? 0 : a > b ? 1 : -1;
    };
    if constexpr (Signed) {
      const bool lhs_is_neg = lhs.is_neg();
      const bool rhs_is_neg = rhs.is_neg();
      if (lhs_is_neg != rhs_is_neg)
        return rhs_is_neg ? 1 : -1;
    }
    for (size_t i = WORD_COUNT; i-- > 0;)
      if (auto cmp = compare(lhs[i], rhs[i]); cmp != 0)
        return cmp;
    return 0;
  }

  LIBC_INLINE constexpr void bitwise_not() {
    for (auto &part : val)
      part = static_cast<WordType>(~part);
  }

  LIBC_INLINE constexpr void negate() {
    bitwise_not();
    increment();
  }

  LIBC_INLINE constexpr void increment() {
    multiword::add_with_carry(val, cpp::array<WordType, 1>{1});
  }

  LIBC_INLINE constexpr void decrement() {
    multiword::sub_with_borrow(val, cpp::array<WordType, 1>{1});
  }

  LIBC_INLINE constexpr void extend(size_t index, bool is_neg) {
    const WordType value = is_neg ? cpp::numeric_limits<WordType>::max()
                                  : cpp::numeric_limits<WordType>::min();
    for (size_t i = index; i < WORD_COUNT; ++i)
      val[i] = value;
  }

  LIBC_INLINE constexpr bool get_msb() const {
    return val.back() >> (WORD_SIZE - 1);
  }

  LIBC_INLINE constexpr void set_msb() {
    val.back() |= mask_leading_ones<WordType, 1>();
  }

  LIBC_INLINE constexpr void clear_msb() {
    val.back() &= mask_trailing_ones<WordType, WORD_SIZE - 1>();
  }
  LIBC_INLINE constexpr static Division divide_unsigned(const BigInt &dividend,
                                                        const BigInt &divider) {
    BigInt remainder = dividend;
    BigInt quotient;
    if (remainder >= divider) {
      BigInt subtractor = divider;
      int cur_bit = multiword::countl_zero(subtractor.val) -
                    multiword::countl_zero(remainder.val);
      subtractor <<= static_cast<size_t>(cur_bit);
      for (; cur_bit >= 0 && remainder > 0; --cur_bit, subtractor >>= 1) {
        if (remainder < subtractor)
          continue;
        remainder -= subtractor;
        quotient.set_bit(static_cast<size_t>(cur_bit));
      }
    }
    return Division{quotient, remainder};
  }

  LIBC_INLINE constexpr static Division divide_signed(const BigInt &dividend,
                                                      const BigInt &divider) {
    // Special case because it is not possible to negate the min value of a
    // signed integer.
    if (dividend == min() && divider == min())
      return Division{one(), zero()};
    // 1. Convert the dividend and divisor to unsigned representation.
    unsigned_type udividend(dividend);
    unsigned_type udivider(divider);
    // 2. Negate the dividend if it's negative, and similarly for the divisor.
    const bool dividend_is_neg = dividend.is_neg();
    const bool divider_is_neg = divider.is_neg();
    if (dividend_is_neg)
      udividend.negate();
    if (divider_is_neg)
      udivider.negate();
    // 3. Use unsigned multiword division algorithm.
    const auto unsigned_result = divide_unsigned(udividend, udivider);
    // 4. Convert the quotient and remainder to signed representation.
    Division result;
    result.quotient = signed_type(unsigned_result.quotient);
    result.remainder = signed_type(unsigned_result.remainder);
    // 5. Negate the quotient if the dividend and divisor had opposite signs.
    if (dividend_is_neg != divider_is_neg)
      result.quotient.negate();
    // 6. Negate the remainder if the dividend was negative.
    if (dividend_is_neg)
      result.remainder.negate();
    return result;
  }

  friend signed_type;
  friend unsigned_type;
};

namespace internal {
// We default BigInt's WordType to 'uint64_t' or 'uint32_t' depending on type
// availability.
template <size_t Bits>
struct WordTypeSelector : cpp::type_identity<
#ifdef LIBC_TYPES_HAS_INT64
                              uint64_t
#else
                              uint32_t
#endif // LIBC_TYPES_HAS_INT64
                              > {
};
// Except if we request 16 or 32 bits explicitly.
template <> struct WordTypeSelector<16> : cpp::type_identity<uint16_t> {};
template <> struct WordTypeSelector<32> : cpp::type_identity<uint32_t> {};
template <> struct WordTypeSelector<96> : cpp::type_identity<uint32_t> {};

template <size_t Bits>
using WordTypeSelectorT = typename WordTypeSelector<Bits>::type;
} // namespace internal

template <size_t Bits>
using UInt = BigInt<Bits, false, internal::WordTypeSelectorT<Bits>>;

template <size_t Bits>
using Int = BigInt<Bits, true, internal::WordTypeSelectorT<Bits>>;

// Provides limits of BigInt.
template <size_t Bits, bool Signed, typename T>
struct cpp::numeric_limits<BigInt<Bits, Signed, T>> {
  LIBC_INLINE static constexpr BigInt<Bits, Signed, T> max() {
    return BigInt<Bits, Signed, T>::max();
  }
  LIBC_INLINE static constexpr BigInt<Bits, Signed, T> min() {
    return BigInt<Bits, Signed, T>::min();
  }
  // Meant to match std::numeric_limits interface.
  // NOLINTNEXTLINE(readability-identifier-naming)
  LIBC_INLINE_VAR static constexpr int digits = Bits - Signed;
};

// type traits to determine whether a T is a BigInt.
template <typename T> struct is_big_int : cpp::false_type {};

template <size_t Bits, bool Signed, typename T>
struct is_big_int<BigInt<Bits, Signed, T>> : cpp::true_type {};

template <class T>
LIBC_INLINE_VAR constexpr bool is_big_int_v = is_big_int<T>::value;

// extensions of type traits to include BigInt

// is_integral_or_big_int
template <typename T>
struct is_integral_or_big_int
    : cpp::bool_constant<(cpp::is_integral_v<T> || is_big_int_v<T>)> {};

template <typename T>
LIBC_INLINE_VAR constexpr bool is_integral_or_big_int_v =
    is_integral_or_big_int<T>::value;

// make_big_int_unsigned
template <typename T> struct make_big_int_unsigned;

template <size_t Bits, bool Signed, typename T>
struct make_big_int_unsigned<BigInt<Bits, Signed, T>>
    : cpp::type_identity<BigInt<Bits, false, T>> {};

template <typename T>
using make_big_int_unsigned_t = typename make_big_int_unsigned<T>::type;

// make_big_int_signed
template <typename T> struct make_big_int_signed;

template <size_t Bits, bool Signed, typename T>
struct make_big_int_signed<BigInt<Bits, Signed, T>>
    : cpp::type_identity<BigInt<Bits, true, T>> {};

template <typename T>
using make_big_int_signed_t = typename make_big_int_signed<T>::type;

// make_integral_or_big_int_unsigned
template <typename T, class = void> struct make_integral_or_big_int_unsigned;

template <typename T>
struct make_integral_or_big_int_unsigned<
    T, cpp::enable_if_t<cpp::is_integral_v<T>>> : cpp::make_unsigned<T> {};

template <typename T>
struct make_integral_or_big_int_unsigned<T, cpp::enable_if_t<is_big_int_v<T>>>
    : make_big_int_unsigned<T> {};

template <typename T>
using make_integral_or_big_int_unsigned_t =
    typename make_integral_or_big_int_unsigned<T>::type;

// make_integral_or_big_int_signed
template <typename T, class = void> struct make_integral_or_big_int_signed;

template <typename T>
struct make_integral_or_big_int_signed<T,
                                       cpp::enable_if_t<cpp::is_integral_v<T>>>
    : cpp::make_signed<T> {};

template <typename T>
struct make_integral_or_big_int_signed<T, cpp::enable_if_t<is_big_int_v<T>>>
    : make_big_int_signed<T> {};

template <typename T>
using make_integral_or_big_int_signed_t =
    typename make_integral_or_big_int_signed<T>::type;

// is_unsigned_integral_or_big_int
template <typename T>
struct is_unsigned_integral_or_big_int
    : cpp::bool_constant<
          cpp::is_same_v<T, make_integral_or_big_int_unsigned_t<T>>> {};

template <typename T>
// Meant to look like <type_traits> helper variable templates.
// NOLINTNEXTLINE(readability-identifier-naming)
LIBC_INLINE_VAR constexpr bool is_unsigned_integral_or_big_int_v =
    is_unsigned_integral_or_big_int<T>::value;

namespace cpp {

// Specialization of cpp::bit_cast ('bit.h') from T to BigInt.
template <typename To, typename From>
LIBC_INLINE constexpr cpp::enable_if_t<
    (sizeof(To) == sizeof(From)) && cpp::is_trivially_copyable<To>::value &&
        cpp::is_trivially_copyable<From>::value && is_big_int<To>::value,
    To>
bit_cast(const From &from) {
  To out;
  using Storage = decltype(out.val);
  out.val = cpp::bit_cast<Storage>(from);
  return out;
}

// Specialization of cpp::bit_cast ('bit.h') from BigInt to T.
template <typename To, size_t Bits>
LIBC_INLINE constexpr cpp::enable_if_t<
    sizeof(To) == sizeof(UInt<Bits>) &&
        cpp::is_trivially_constructible<To>::value &&
        cpp::is_trivially_copyable<To>::value &&
        cpp::is_trivially_copyable<UInt<Bits>>::value,
    To>
bit_cast(const UInt<Bits> &from) {
  return cpp::bit_cast<To>(from.val);
}

// Specialization of cpp::popcount ('bit.h') for BigInt.
template <typename T>
[[nodiscard]] LIBC_INLINE constexpr cpp::enable_if_t<is_big_int_v<T>, int>
popcount(T value) {
  int bits = 0;
  for (auto word : value.val)
    if (word)
      bits += popcount(word);
  return bits;
}

// Specialization of cpp::has_single_bit ('bit.h') for BigInt.
template <typename T>
[[nodiscard]] LIBC_INLINE constexpr cpp::enable_if_t<is_big_int_v<T>, bool>
has_single_bit(T value) {
  int bits = 0;
  for (auto word : value.val) {
    if (word == 0)
      continue;
    bits += popcount(word);
    if (bits > 1)
      return false;
  }
  return bits == 1;
}

// Specialization of cpp::countr_zero ('bit.h') for BigInt.
template <typename T>
[[nodiscard]] LIBC_INLINE constexpr cpp::enable_if_t<is_big_int_v<T>, int>
countr_zero(const T &value) {
  return multiword::countr_zero(value.val);
}

// Specialization of cpp::countl_zero ('bit.h') for BigInt.
template <typename T>
[[nodiscard]] LIBC_INLINE constexpr cpp::enable_if_t<is_big_int_v<T>, int>
countl_zero(const T &value) {
  return multiword::countl_zero(value.val);
}

// Specialization of cpp::countl_one ('bit.h') for BigInt.
template <typename T>
[[nodiscard]] LIBC_INLINE constexpr cpp::enable_if_t<is_big_int_v<T>, int>
countl_one(T value) {
  return multiword::countl_one(value.val);
}

// Specialization of cpp::countr_one ('bit.h') for BigInt.
template <typename T>
[[nodiscard]] LIBC_INLINE constexpr cpp::enable_if_t<is_big_int_v<T>, int>
countr_one(T value) {
  return multiword::countr_one(value.val);
}

// Specialization of cpp::bit_width ('bit.h') for BigInt.
template <typename T>
[[nodiscard]] LIBC_INLINE constexpr cpp::enable_if_t<is_big_int_v<T>, int>
bit_width(T value) {
  return cpp::numeric_limits<T>::digits - cpp::countl_zero(value);
}

// Forward-declare rotr so that rotl can use it.
template <typename T>
[[nodiscard]] LIBC_INLINE constexpr cpp::enable_if_t<is_big_int_v<T>, T>
rotr(T value, int rotate);

// Specialization of cpp::rotl ('bit.h') for BigInt.
template <typename T>
[[nodiscard]] LIBC_INLINE constexpr cpp::enable_if_t<is_big_int_v<T>, T>
rotl(T value, int rotate) {
  constexpr int N = cpp::numeric_limits<T>::digits;
  rotate = rotate % N;
  if (!rotate)
    return value;
  if (rotate < 0)
    return cpp::rotr<T>(value, -rotate);
  return (value << static_cast<size_t>(rotate)) |
         (value >> (N - static_cast<size_t>(rotate)));
}

// Specialization of cpp::rotr ('bit.h') for BigInt.
template <typename T>
[[nodiscard]] LIBC_INLINE constexpr cpp::enable_if_t<is_big_int_v<T>, T>
rotr(T value, int rotate) {
  constexpr int N = cpp::numeric_limits<T>::digits;
  rotate = rotate % N;
  if (!rotate)
    return value;
  if (rotate < 0)
    return cpp::rotl<T>(value, -rotate);
  return (value >> static_cast<size_t>(rotate)) |
         (value << (N - static_cast<size_t>(rotate)));
}

} // namespace cpp

// Specialization of mask_trailing_ones ('math_extras.h') for BigInt.
template <typename T, size_t count>
LIBC_INLINE constexpr cpp::enable_if_t<is_big_int_v<T>, T>
mask_trailing_ones() {
  static_assert(!T::SIGNED && count <= T::BITS);
  if (count == T::BITS)
    return T::all_ones();
  constexpr size_t QUOTIENT = count / T::WORD_SIZE;
  constexpr size_t REMAINDER = count % T::WORD_SIZE;
  T out; // zero initialized
  for (size_t i = 0; i <= QUOTIENT; ++i)
    out[i] = i < QUOTIENT
                 ? cpp::numeric_limits<typename T::word_type>::max()
                 : mask_trailing_ones<typename T::word_type, REMAINDER>();
  return out;
}

// Specialization of mask_leading_ones ('math_extras.h') for BigInt.
template <typename T, size_t count>
LIBC_INLINE constexpr cpp::enable_if_t<is_big_int_v<T>, T> mask_leading_ones() {
  static_assert(!T::SIGNED && count <= T::BITS);
  if (count == T::BITS)
    return T::all_ones();
  constexpr size_t QUOTIENT = (T::BITS - count - 1U) / T::WORD_SIZE;
  constexpr size_t REMAINDER = count % T::WORD_SIZE;
  T out; // zero initialized
  for (size_t i = QUOTIENT; i < T::WORD_COUNT; ++i)
    out[i] = i > QUOTIENT
                 ? cpp::numeric_limits<typename T::word_type>::max()
                 : mask_leading_ones<typename T::word_type, REMAINDER>();
  return out;
}

// Specialization of mask_trailing_zeros ('math_extras.h') for BigInt.
template <typename T, size_t count>
LIBC_INLINE constexpr cpp::enable_if_t<is_big_int_v<T>, T>
mask_trailing_zeros() {
  return mask_leading_ones<T, T::BITS - count>();
}

// Specialization of mask_leading_zeros ('math_extras.h') for BigInt.
template <typename T, size_t count>
LIBC_INLINE constexpr cpp::enable_if_t<is_big_int_v<T>, T>
mask_leading_zeros() {
  return mask_trailing_ones<T, T::BITS - count>();
}

// Specialization of count_zeros ('math_extras.h') for BigInt.
template <typename T>
[[nodiscard]] LIBC_INLINE constexpr cpp::enable_if_t<is_big_int_v<T>, int>
count_zeros(T value) {
  return cpp::popcount(~value);
}

// Specialization of first_leading_zero ('math_extras.h') for BigInt.
template <typename T>
[[nodiscard]] LIBC_INLINE constexpr cpp::enable_if_t<is_big_int_v<T>, int>
first_leading_zero(T value) {
  return value == cpp::numeric_limits<T>::max() ? 0
                                                : cpp::countl_one(value) + 1;
}

// Specialization of first_leading_one ('math_extras.h') for BigInt.
template <typename T>
[[nodiscard]] LIBC_INLINE constexpr cpp::enable_if_t<is_big_int_v<T>, int>
first_leading_one(T value) {
  return first_leading_zero(~value);
}

// Specialization of first_trailing_zero ('math_extras.h') for BigInt.
template <typename T>
[[nodiscard]] LIBC_INLINE constexpr cpp::enable_if_t<is_big_int_v<T>, int>
first_trailing_zero(T value) {
  return value == cpp::numeric_limits<T>::max() ? 0
                                                : cpp::countr_zero(~value) + 1;
}

// Specialization of first_trailing_one ('math_extras.h') for BigInt.
template <typename T>
[[nodiscard]] LIBC_INLINE constexpr cpp::enable_if_t<is_big_int_v<T>, int>
first_trailing_one(T value) {
  return value == 0 ? 0 : cpp::countr_zero(value) + 1;
}

} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_BIG_INT_H
