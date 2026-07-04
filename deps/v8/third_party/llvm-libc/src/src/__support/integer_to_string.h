//===-- Utilities to convert integral values to string ----------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Converts an integer to a string.
//
// By default, the string is written as decimal to an internal buffer and
// accessed via the 'view' method.
//
//   IntegerToString<int> buffer(42);
//   cpp::string_view view = buffer.view();
//
// The buffer is allocated on the stack and its size is so that the conversion
// always succeeds.
//
// It is also possible to write the data to a preallocated buffer, but this may
// fail.
//
//   char buffer[8];
//   if (auto maybe_view = IntegerToString<int>::write_to_span(buffer, 42)) {
//     cpp::string_view view = *maybe_view;
//   }
//
// The first template parameter is the type of the integer.
// The second template parameter defines how the integer is formatted.
// Available default are 'radix::Bin', 'radix::Oct', 'radix::Dec' and
// 'radix::Hex'.
//
// For 'radix::Bin', 'radix::Oct' and 'radix::Hex' the value is always
// interpreted as a positive type but 'radix::Dec' will honor negative values.
// e.g.,
//
//   IntegerToString<int8_t>(-1)             // "-1"
//   IntegerToString<int8_t, radix::Dec>(-1) // "-1"
//   IntegerToString<int8_t, radix::Bin>(-1) // "11111111"
//   IntegerToString<int8_t, radix::Oct>(-1) // "377"
//   IntegerToString<int8_t, radix::Hex>(-1) // "ff"
//
// Additionnally, the format can be changed by navigating the subtypes:
//  - WithPrefix    : Adds "0b", "0", "0x" for binary, octal and hexadecimal
//  - WithWidth<XX> : Pad string to XX characters filling leading digits with 0
//  - Uppercase     : Use uppercase letters (only for HexString)
//  - WithSign      : Prepend '+' for positive values (only for DecString)
//
// Examples
// --------
//   IntegerToString<int8_t, radix::Dec::WithWidth<2>::WithSign>(0)     : "+00"
//   IntegerToString<int8_t, radix::Dec::WithWidth<2>::WithSign>(-1)    : "-01"
//   IntegerToString<uint8_t, radix::Hex::WithPrefix::Uppercase>(255)   : "0xFF"
//   IntegerToString<uint8_t, radix::Hex::WithWidth<4>::Uppercase>(255) : "00FF"
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_INTEGER_TO_STRING_H
#define LLVM_LIBC_SRC___SUPPORT_INTEGER_TO_STRING_H

#include "hdr/stdint_proxy.h"
#include "src/__support/CPP/algorithm.h" // max
#include "src/__support/CPP/array.h"
#include "src/__support/CPP/bit.h"
#include "src/__support/CPP/limits.h"
#include "src/__support/CPP/optional.h"
#include "src/__support/CPP/span.h"
#include "src/__support/CPP/string_view.h"
#include "src/__support/CPP/type_traits.h"
#include "src/__support/big_int.h" // make_integral_or_big_int_unsigned_t
#include "src/__support/common.h"
#include "src/__support/ctype_utils.h"
#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {

namespace details {

template <uint8_t base, bool prefix = false, bool force_sign = false,
          bool is_uppercase = false, size_t min_digits = 1>
struct Fmt {
  static constexpr uint8_t BASE = base;
  static constexpr size_t MIN_DIGITS = min_digits;
  static constexpr bool IS_UPPERCASE = is_uppercase;
  static constexpr bool PREFIX = prefix;
  static constexpr char FORCE_SIGN = force_sign;

  using WithPrefix = Fmt<BASE, true, FORCE_SIGN, IS_UPPERCASE, MIN_DIGITS>;
  using WithSign = Fmt<BASE, PREFIX, true, IS_UPPERCASE, MIN_DIGITS>;
  using Uppercase = Fmt<BASE, PREFIX, FORCE_SIGN, true, MIN_DIGITS>;
  template <size_t value>
  using WithWidth = Fmt<BASE, PREFIX, FORCE_SIGN, IS_UPPERCASE, value>;

  // Invariants
  static constexpr uint8_t NUMERICAL_DIGITS = 10;
  static constexpr uint8_t ALPHA_DIGITS = 26;
  static constexpr uint8_t MAX_DIGIT = NUMERICAL_DIGITS + ALPHA_DIGITS;
  static_assert(BASE > 1 && BASE <= MAX_DIGIT);
  static_assert(!IS_UPPERCASE || BASE > 10, "Uppercase is only for radix > 10");
  static_assert(!FORCE_SIGN || BASE == 10, "WithSign is only for radix == 10");
  static_assert(!PREFIX || (BASE == 2 || BASE == 8 || BASE == 16),
                "WithPrefix is only for radix == 2, 8 or 16");
};

// Move this to a separate header since it might be useful elsewhere.
template <bool forward> class StringBufferWriterImpl {
  cpp::span<char> buffer;
  size_t index = 0;
  bool out_of_range = false;

  LIBC_INLINE size_t location() const {
    return forward ? index : buffer.size() - 1 - index;
  }

public:
  StringBufferWriterImpl(const StringBufferWriterImpl &) = delete;
  StringBufferWriterImpl(cpp::span<char> buffer) : buffer(buffer) {}

  LIBC_INLINE size_t size() const { return index; }
  LIBC_INLINE size_t remainder_size() const { return buffer.size() - size(); }
  LIBC_INLINE bool empty() const { return size() == 0; }
  LIBC_INLINE bool full() const { return size() == buffer.size(); }
  LIBC_INLINE bool ok() const { return !out_of_range; }

  LIBC_INLINE StringBufferWriterImpl &push(char c) {
    if (ok()) {
      if (!full()) {
        buffer[location()] = c;
        ++index;
      } else {
        out_of_range = true;
      }
    }
    return *this;
  }

  LIBC_INLINE cpp::span<char> remainder_span() const {
    return forward ? buffer.last(remainder_size())
                   : buffer.first(remainder_size());
  }

  LIBC_INLINE cpp::span<char> buffer_span() const {
    return forward ? buffer.first(size()) : buffer.last(size());
  }

  LIBC_INLINE cpp::string_view buffer_view() const {
    const auto s = buffer_span();
    return {s.data(), s.size()};
  }
};

using StringBufferWriter = StringBufferWriterImpl<true>;
using BackwardStringBufferWriter = StringBufferWriterImpl<false>;

} // namespace details

namespace radix {

using Bin = details::Fmt<2>;
using Oct = details::Fmt<8>;
using Dec = details::Fmt<10>;
using Hex = details::Fmt<16>;
template <size_t radix> using Custom = details::Fmt<radix>;

} // namespace radix

// Extract the low-order decimal digit from a value of integer type T. The
// returned value is the digit itself, from 0 to 9. The input value is passed
// by reference, and modified by dividing by 10, so that iterating this
// function extracts all the digits of the original number one at a time from
// low to high.
template <typename T>
LIBC_INLINE cpp::enable_if_t<cpp::is_integral_v<T>, uint8_t>
extract_decimal_digit(T &value) {
  const uint8_t digit(static_cast<uint8_t>(value % 10));
  // For built-in integer types, we assume that an adequately fast division is
  // available. If hardware division isn't implemented, then with a divisor
  // known at compile time the compiler might be able to generate an optimized
  // sequence instead.
  value /= 10;
  return digit;
}

// A specialization of extract_decimal_digit for the BigInt type in big_int.h,
// avoiding the use of general-purpose BigInt division which is very slow.
template <typename T>
LIBC_INLINE cpp::enable_if_t<is_big_int_v<T>, uint8_t>
extract_decimal_digit(T &value) {
  // There are two essential ways you can turn n into (n/10,n%10). One is
  // ordinary integer division. The other is a modular-arithmetic approach in
  // which you first compute n%10 by bit twiddling, then subtract it off to get
  // a value that is definitely a multiple of 10. Then you divide that by 10 in
  // two steps: shift right to divide off a factor of 2, and then divide off a
  // factor of 5 by multiplying by the modular inverse of 5 mod 2^BITS. (That
  // last step only works if you know there's no remainder, which is why you
  // had to subtract off the output digit first.)
  //
  // Either approach can be made to work in linear time. This code uses the
  // modular-arithmetic technique, because the other approach either does a lot
  // of integer divisions (requiring a fast hardware divider), or else uses a
  // "multiply by an approximation to the reciprocal" technique which depends
  // on careful error analysis which might go wrong in an untested edge case.

  using Word = typename T::word_type;

  // Find the remainder (value % 10). We do this by breaking up the input
  // integer into chunks of size WORD_SIZE/2, so that the sum of them doesn't
  // overflow a Word. Then we sum all the half-words times 6, except the bottom
  // one, which is added to that sum without scaling.
  //
  // Why 6? Because you can imagine that the original number had the form
  //
  //   halfwords[0] + K*halfwords[1] + K^2*halfwords[2] + ...
  //
  // where K = 2^(WORD_SIZE/2). Since WORD_SIZE is expected to be a multiple of
  // 8, that makes WORD_SIZE/2 a multiple of 4, so that K is a power of 16. And
  // all powers of 16 (larger than 1) are congruent to 6 mod 10, by induction:
  // 16 itself is, and 6^2=36 is also congruent to 6.
  Word acc_remainder = 0;
  constexpr Word HALFWORD_BITS = T::WORD_SIZE / 2;
  constexpr Word HALFWORD_MASK = ((Word(1) << HALFWORD_BITS) - 1);
  // Sum both halves of all words except the low one.
  for (size_t i = 1; i < T::WORD_COUNT; i++) {
    acc_remainder += value.val[i] >> HALFWORD_BITS;
    acc_remainder += value.val[i] & HALFWORD_MASK;
  }
  // Add the high half of the low word. Then we have everything that needs to
  // be multiplied by 6, so do that.
  acc_remainder += value.val[0] >> HALFWORD_BITS;
  acc_remainder *= 6;
  // Having multiplied it by 6, add the lowest half-word, and then reduce mod
  // 10 by normal integer division to finish.
  acc_remainder += value.val[0] & HALFWORD_MASK;
  uint8_t digit = static_cast<uint8_t>(acc_remainder % 10);

  // Now we have the output digit. Subtract it from the input value, and shift
  // right to divide by 2.
  value -= digit;
  value >>= 1;

  // Now all that's left is to multiply by the inverse of 5 mod 2^BITS. No
  // matter what the value of BITS, the inverse of 5 has the very convenient
  // form 0xCCCC...CCCD, with as many C hex digits in the middle as necessary.
  //
  // We could construct a second BigInt with all words 0xCCCCCCCCCCCCCCCC,
  // increment the bottom word, and call a general-purpose multiply function.
  // But we can do better, by taking advantage of the regularity: we can do
  // this particular operation in linear time, whereas a general multiplier
  // would take superlinear time (quadratic in small cases).
  //
  // To begin with, instead of computing n*0xCCCC...CCCD, we'll compute
  // n*0xCCCC...CCCC and then add it to the original n. Then all the words of
  // the multiplier have the same value 0xCCCCCCCCCCCCCCCC, which I'll just
  // denote as C. If we also write t = 2^WORD_SIZE, and imagine (as an example)
  // that the input number has three words x,y,z with x being the low word,
  // then we're computing
  //
  //   (x + y t + z t^2) * (C + C t + C t^2)
  //
  // = x C + y C t + z C t^2
  //       + x C t + y C t^2 + z C t^3
  //               + x C t^2 + y C t^3 + z C t^4
  //
  // but we're working mod t^3, so the high-order terms vanish and this becomes
  //
  //   x C + y C t + z C t^2
  //       + x C t + y C t^2
  //               + x C t^2
  //
  // = x C + (x+y) C t + (x+y+z) C t^2
  //
  // So all you have to do is to work from the low word of the integer upwards,
  // accumulating C times the sum of all the words you've seen so far to get
  // x*C, (x+y)*C, (x+y+z)*C and so on. In each step you add another product to
  // the accumulator, and add the accumulator to the corresponding word of the
  // original number (so that we end up with value*CCCD, not just value*CCCC).
  //
  // If you do that literally, then your accumulator has to be three words
  // wide, because the sum of words can overflow into a second word, and
  // multiplying by C adds another word. But we can do slightly better by
  // breaking each product word*C up into a bottom half and a top half. If we
  // write x*C = xl + xh*t, and similarly for y and z, then our sum becomes
  //
  //   (xl + xh t) + (yl + yh t) t + (zl + zh t) t^2
  //               + (xl + xh t) t + (yl + yh t) t^2
  //                               + (xl + xh t) t^2
  //
  // and if you expand out again, collect terms, and discard t^3 terms, you get
  //
  //   (xl)
  // + (xl + xh + yl) t
  // + (xl + xh + yl + yh + zl) t^2
  //
  // in which each coefficient is the sum of all the low words of the products
  // up to _and including_ the current word, plus all the high words up to but
  // _not_ including the current word. So now you only have to retain two words
  // of sum instead of three.
  //
  // We do this entire procedure in a single in-place pass over the input
  // number, reading each word to make its product with C and then adding the
  // low word of the accumulator to it.
  constexpr Word C = Word(-1) / 5 * 4; // calculate 0xCCCC as 4/5 of 0xFFFF
  Word acc_lo = 0, acc_hi = 0; // accumulator of all the half-products so far
  Word carry_bit, carry_word = 0;

  for (size_t i = 0; i < T::WORD_COUNT; i++) {
    // Make the two-word product of C with the current input word.
    multiword::DoubleWide<Word> product = multiword::mul2(C, value.val[i]);

    // Add the low half of the product to our accumulator, but not yet the high
    // half.
    acc_lo = add_with_carry<Word>(acc_lo, product[0], 0, carry_bit);
    acc_hi += carry_bit;

    // Now the accumulator contains exactly the value we need to add to the
    // current input word. Add it, plus any carries from lower words, and make
    // a new word of carry data to propagate into the next iteration.
    value.val[i] = add_with_carry<Word>(value.val[i], carry_word, 0, carry_bit);
    carry_word = acc_hi + carry_bit;
    value.val[i] = add_with_carry<Word>(value.val[i], acc_lo, 0, carry_bit);
    carry_word += carry_bit;

    // Now add the high half of the current product to our accumulator.
    acc_lo = add_with_carry<Word>(acc_lo, product[1], 0, carry_bit);
    acc_hi += carry_bit;
  }

  return digit;
}

// See file header for documentation.
template <typename T, typename Fmt = radix::Dec> class IntegerToString {
  static_assert(cpp::is_integral_v<T> || is_big_int_v<T>);

  LIBC_INLINE static constexpr size_t compute_buffer_size() {
    constexpr auto MAX_DIGITS = []() -> size_t {
      // We size the string buffer for base 10 using an approximation algorithm:
      //
      //   size = ceil(sizeof(T) * 5 / 2)
      //
      // If sizeof(T) is 1, then size is 3 (actually need 3)
      // If sizeof(T) is 2, then size is 5 (actually need 5)
      // If sizeof(T) is 4, then size is 10 (actually need 10)
      // If sizeof(T) is 8, then size is 20 (actually need 20)
      // If sizeof(T) is 16, then size is 40 (actually need 39)
      //
      // NOTE: The ceil operation is actually implemented as
      //     floor(((sizeof(T) * 5) + 1) / 2)
      // where floor operation is just integer division.
      //
      // This estimation grows slightly faster than the actual value, but the
      // overhead is small enough to tolerate.
      if constexpr (Fmt::BASE == 10)
        return ((sizeof(T) * 5) + 1) / 2;
      // For other bases, we approximate by rounding down to the nearest power
      // of two base, since the space needed is easy to calculate and it won't
      // overestimate by too much.
      constexpr auto FLOOR_LOG_2 = [](size_t num) -> size_t {
        size_t i = 0;
        for (; num > 1; num /= 2)
          ++i;
        return i;
      };
      constexpr size_t BITS_PER_DIGIT = FLOOR_LOG_2(Fmt::BASE);
      return ((sizeof(T) * 8 + (BITS_PER_DIGIT - 1)) / BITS_PER_DIGIT);
    };
    constexpr size_t DIGIT_SIZE = cpp::max(MAX_DIGITS(), Fmt::MIN_DIGITS);
    constexpr size_t SIGN_SIZE = Fmt::BASE == 10 ? 1 : 0;
    constexpr size_t PREFIX_SIZE = Fmt::PREFIX ? 2 : 0;
    return DIGIT_SIZE + SIGN_SIZE + PREFIX_SIZE;
  }

  static constexpr size_t BUFFER_SIZE = compute_buffer_size();
  static_assert(BUFFER_SIZE > 0);

  // An internal stateless structure that handles the number formatting logic.
  struct IntegerWriter {
    static_assert(cpp::is_integral_v<T> || is_big_int_v<T>);
    using UNSIGNED_T = make_integral_or_big_int_unsigned_t<T>;

    LIBC_INLINE static char digit_char(uint8_t digit) {
      const char result = internal::int_to_b36_char(digit);
      return Fmt::IS_UPPERCASE ? internal::toupper(result) : result;
    }

    LIBC_INLINE static void
    write_unsigned_number(UNSIGNED_T value,
                          details::BackwardStringBufferWriter &sink) {
      for (; sink.ok() && value != 0; value /= Fmt::BASE) {
        const uint8_t digit(static_cast<uint8_t>(value % Fmt::BASE));
        sink.push(digit_char(digit));
      }
    }

    LIBC_INLINE static void
    write_unsigned_number_dec(UNSIGNED_T value,
                              details::BackwardStringBufferWriter &sink) {
      while (sink.ok() && value != 0) {
        const uint8_t digit = extract_decimal_digit(value);
        sink.push(digit_char(digit));
      }
    }

    // Returns the absolute value of 'value' as 'UNSIGNED_T'.
    LIBC_INLINE static UNSIGNED_T abs(T value) {
      if (cpp::is_unsigned_v<T> || value >= 0)
        return static_cast<UNSIGNED_T>(value); // already of the right sign.

      // Signed integers are asymmetric (e.g., int8_t ∈ [-128, 127]).
      // Thus negating the type's minimum value would overflow.
      // From C++20 on, signed types are guaranteed to be represented as 2's
      // complement. We take advantage of this representation and negate the
      // value by using the exact same bit representation, e.g.,
      // binary : 0b1000'0000
      // int8_t : -128
      // uint8_t:  128

      // Note: the compiler can completely optimize out the two branches and
      // replace them by a simple negate instruction.
      // https://godbolt.org/z/hE7zahT9W
      if (value == cpp::numeric_limits<T>::min()) {
        return cpp::bit_cast<UNSIGNED_T>(value);
      } else {
        return static_cast<UNSIGNED_T>(
            -value); // legal and representable both as T and UNSIGNED_T.`
      }
    }

    LIBC_INLINE static void write(T value,
                                  details::BackwardStringBufferWriter &sink) {
      if constexpr (Fmt::BASE == 10) {
        write_unsigned_number_dec(abs(value), sink);
      } else {
        write_unsigned_number(static_cast<UNSIGNED_T>(value), sink);
      }
      // width
      while (sink.ok() && sink.size() < Fmt::MIN_DIGITS)
        sink.push('0');
      // sign
      if constexpr (Fmt::BASE == 10) {
        if (value < 0)
          sink.push('-');
        else if (Fmt::FORCE_SIGN)
          sink.push('+');
      }
      // prefix
      if constexpr (Fmt::PREFIX) {
        if constexpr (Fmt::BASE == 2) {
          sink.push('b');
          sink.push('0');
        }
        if constexpr (Fmt::BASE == 16) {
          sink.push('x');
          sink.push('0');
        }
        if constexpr (Fmt::BASE == 8) {
          const cpp::string_view written = sink.buffer_view();
          if (written.empty() || written.front() != '0')
            sink.push('0');
        }
      }
    }
  };

  cpp::array<char, BUFFER_SIZE> array;
  size_t written = 0;

public:
  IntegerToString(const IntegerToString &) = delete;
  IntegerToString(T value) {
    details::BackwardStringBufferWriter writer(array);
    IntegerWriter::write(value, writer);
    written = writer.size();
  }

  [[nodiscard]] LIBC_INLINE static cpp::optional<cpp::string_view>
  format_to(cpp::span<char> buffer, T value) {
    details::BackwardStringBufferWriter writer(buffer);
    IntegerWriter::write(value, writer);
    if (writer.ok())
      return cpp::string_view(buffer.data() + buffer.size() - writer.size(),
                              writer.size());
    return cpp::nullopt;
  }

  LIBC_INLINE static constexpr size_t buffer_size() { return BUFFER_SIZE; }

  LIBC_INLINE size_t size() const { return written; }
  LIBC_INLINE cpp::string_view view() && = delete;
  LIBC_INLINE cpp::string_view view() const & {
    return cpp::string_view(array.data() + array.size() - size(), size());
  }
};

} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_INTEGER_TO_STRING_H
