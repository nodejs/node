// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_BIGINT_BIGINT_INL_H_
#define V8_BIGINT_BIGINT_INL_H_

#include "src/bigint/bigint.h"
// Include the non-inl header before the rest of the headers.

#include <iostream>
#include <type_traits>
#include <vector>

namespace v8::bigint {

// Reuse the embedder's CHECK/DCHECK, or define our own as fallbacks.
#ifndef CHECK
#define CHECK(cond)                                   \
  if (!(cond)) {                                      \
    std::cerr << __FILE__ << ":" << __LINE__ << ": "; \
    std::cerr << "Assertion failed: " #cond "\n";     \
    abort();                                          \
  }
#endif  // CHECK

#ifndef DCHECK
#ifdef DEBUG
#define DCHECK(cond) CHECK(cond)
#else
#define DCHECK(cond) (void(0))
#endif  // DEBUG
#endif  // DCHECK

#if !defined(DEBUG) && (defined(__GNUC__) || defined(__clang__))
// Clang supports this since 3.9, GCC since 4.x.
#define ALWAYS_INLINE inline __attribute__((always_inline))
#elif !defined(DEBUG) && defined(_MSC_VER)
#define ALWAYS_INLINE __forceinline
#else
#define ALWAYS_INLINE inline
#endif

// Integer division, rounding up.
#define DIV_CEIL(x, y) (((x) - 1) / (y) + 1)

namespace config {

constexpr uint32_t kKaratsubaThreshold = 34;
constexpr uint32_t kToomThreshold = 193;
constexpr uint32_t kFftThreshold = 1500;
constexpr uint32_t kFftInnerThreshold = 200;

constexpr uint32_t kBurnikelThreshold = 57;
constexpr uint32_t kNewtonInversionThreshold = 50;
constexpr uint32_t kBarrettThreshold = 13310;

constexpr uint32_t kToStringFastThreshold = 43;
constexpr uint32_t kFromStringLargeThreshold = 300;

}  // namespace config

class ProcessorImpl;

// Support for parsing BigInts from Strings, using an Accumulator object
// for intermediate state.

static constexpr uint32_t kStackParts = 8;

// A container object for all metadata required for parsing a BigInt from
// a string.
// Aggressively optimized not to waste instructions for small cases, while
// also scaling transparently to huge cases.
// Defined here in the header so that it can be inlined.
class FromStringAccumulator {
 public:
  enum class Result { kOk, kMaxSizeExceeded };

  // Step 1: Create a FromStringAccumulator instance. For best performance,
  // stack allocation is recommended.
  // {max_digits} is only used for refusing to grow beyond a given size
  // (see "Step 2" below). It does not cause pre-allocation, so feel free to
  // specify a large maximum.
  // TODO(jkummerow): The limit applies to the number of intermediate chunks,
  // whereas the final result will be slightly smaller (depending on {radix}).
  // So for sufficiently large N, setting max_digits=N here will not actually
  // allow parsing BigInts with N digits. We can fix that if/when anyone cares.
  explicit FromStringAccumulator(uint32_t max_digits)
      : max_digits_(std::max(max_digits, kStackParts)) {}

  // Step 2: Call this method to read all characters.
  // {CharIt} should be a forward iterator and
  // std::iterator_traits<CharIt>::value_type shall be a character type, such as
  // uint8_t or uint16_t. {end} should be one past the last character (i.e.
  // {start == end} would indicate an empty string). Returns the current
  // position when an invalid character is encountered.
  template <class CharIt>
  ALWAYS_INLINE CharIt Parse(CharIt start, CharIt end, digit_t radix);

  // Step 3: Check if a result is available, and determine its required
  // allocation size (guaranteed to be <= max_digits passed to the constructor).
  Result result() { return result_; }
  uint32_t ResultLength() {
    return std::max(stack_parts_used_,
                    static_cast<uint32_t>(heap_parts_.size()));
  }

  // Step 4: Use BigIntProcessor::FromString() to retrieve the result into an
  // {RWDigits} struct allocated for the size returned by step 3.

 private:
  friend class ProcessorImpl;

  template <class CharIt>
  ALWAYS_INLINE CharIt ParsePowerTwo(CharIt start, CharIt end, digit_t radix);

  ALWAYS_INLINE bool AddPart(digit_t multiplier, digit_t part, bool is_last);
  ALWAYS_INLINE bool AddPart(digit_t part);

  digit_t stack_parts_[kStackParts];
  std::vector<digit_t> heap_parts_;
  digit_t max_multiplier_{0};
  digit_t last_multiplier_;
  const uint32_t max_digits_;
  Result result_{Result::kOk};
  uint32_t stack_parts_used_{0};
  bool inline_everything_{false};
  uint8_t radix_{0};
};

// The rest of this file is the inlineable implementation of
// FromStringAccumulator methods.

#if defined(__GNUC__) || defined(__clang__)
// Clang supports this since 3.9, GCC since 5.x.
#define HAVE_BUILTIN_MUL_OVERFLOW 1
#else
#define HAVE_BUILTIN_MUL_OVERFLOW 0
#endif

// Numerical value of the first 127 ASCII characters, using 255 as sentinel
// for "invalid".
static constexpr uint8_t kCharValue[] = {
    255, 255, 255, 255, 255, 255, 255, 255,  // 0..7
    255, 255, 255, 255, 255, 255, 255, 255,  // 8..15
    255, 255, 255, 255, 255, 255, 255, 255,  // 16..23
    255, 255, 255, 255, 255, 255, 255, 255,  // 24..31
    255, 255, 255, 255, 255, 255, 255, 255,  // 32..39
    255, 255, 255, 255, 255, 255, 255, 255,  // 40..47
    0,   1,   2,   3,   4,   5,   6,   7,    // 48..55    '0' == 48
    8,   9,   255, 255, 255, 255, 255, 255,  // 56..63    '9' == 57
    255, 10,  11,  12,  13,  14,  15,  16,   // 64..71    'A' == 65
    17,  18,  19,  20,  21,  22,  23,  24,   // 72..79
    25,  26,  27,  28,  29,  30,  31,  32,   // 80..87
    33,  34,  35,  255, 255, 255, 255, 255,  // 88..95    'Z' == 90
    255, 10,  11,  12,  13,  14,  15,  16,   // 96..103   'a' == 97
    17,  18,  19,  20,  21,  22,  23,  24,   // 104..111
    25,  26,  27,  28,  29,  30,  31,  32,   // 112..119
    33,  34,  35,  255, 255, 255, 255, 255,  // 120..127  'z' == 122
};

// A space- and time-efficient way to map {2,4,8,16,32} to {1,2,3,4,5}.
static constexpr uint8_t kCharBits[] = {1, 2, 3, 0, 4, 0, 0, 0, 5};

template <class CharIt>
CharIt FromStringAccumulator::ParsePowerTwo(CharIt current, CharIt end,
                                            digit_t radix) {
  radix_ = static_cast<uint8_t>(radix);
  const int char_bits = kCharBits[radix >> 2];
  int bits_left;
  bool done = false;
  do {
    digit_t part = 0;
    bits_left = kDigitBits;
    while (true) {
      digit_t d;  // Numeric value of the current character {c}.
      uint32_t c = *current;
      if (c > 127 || (d = bigint::kCharValue[c]) >= radix) {
        done = true;
        break;
      }

      if (bits_left < char_bits) break;
      bits_left -= char_bits;
      part = (part << char_bits) | d;

      ++current;
      if (current == end) {
        done = true;
        break;
      }
    }
    if (!AddPart(part)) return current;
  } while (!done);
  // We use the unused {last_multiplier_} field to
  // communicate how many bits are unused in the last part.
  last_multiplier_ = bits_left;
  return current;
}

template <class CharIt>
CharIt FromStringAccumulator::Parse(CharIt start, CharIt end, digit_t radix) {
  DCHECK(2 <= radix && radix <= 36);
  CharIt current = start;
#if !HAVE_BUILTIN_MUL_OVERFLOW
  const digit_t kMaxMultiplier = (~digit_t{0}) / radix;
#endif
#if HAVE_TWODIGIT_T  // The inlined path requires twodigit_t availability.
  // The max supported radix is 36, and Math.log2(36) == 5.169..., so we
  // need at most 5.17 bits per char.
  static constexpr uint32_t kInlineThreshold =
      kStackParts * kDigitBits * 100 / 517;
  DCHECK(end >= start);
  inline_everything_ = static_cast<uint32_t>(end - start) <= kInlineThreshold;
#endif
  if (!inline_everything_ && (radix & (radix - 1)) == 0) {
    return ParsePowerTwo(start, end, radix);
  }
  bool done = false;
  do {
    digit_t multiplier = 1;
    digit_t part = 0;
    while (true) {
      digit_t d;  // Numeric value of the current character {c}.
      uint32_t c = *current;
      if (c > 127 || (d = bigint::kCharValue[c]) >= radix) {
        done = true;
        break;
      }

#if HAVE_BUILTIN_MUL_OVERFLOW
      digit_t new_multiplier;
      if (__builtin_mul_overflow(multiplier, radix, &new_multiplier)) break;
      multiplier = new_multiplier;
#else
      if (multiplier > kMaxMultiplier) break;
      multiplier *= radix;
#endif
      part = part * radix + d;

      ++current;
      if (current == end) {
        done = true;
        break;
      }
    }
    if (!AddPart(multiplier, part, done)) return current;
  } while (!done);
  return current;
}

bool FromStringAccumulator::AddPart(digit_t multiplier, digit_t part,
                                    bool is_last) {
#if HAVE_TWODIGIT_T
  if (inline_everything_) {
    // Inlined version of {MultiplySingle}.
    digit_t carry = part;
    digit_t high = 0;
    for (uint32_t i = 0; i < stack_parts_used_; i++) {
      twodigit_t result = twodigit_t{stack_parts_[i]} * multiplier;
      digit_t new_high = result >> bigint::kDigitBits;
      digit_t low = static_cast<digit_t>(result);
      result = twodigit_t{low} + high + carry;
      carry = result >> bigint::kDigitBits;
      stack_parts_[i] = static_cast<digit_t>(result);
      high = new_high;
    }
    high += carry;
    if (high != 0) stack_parts_[stack_parts_used_++] = high;
    DCHECK(stack_parts_used_ <= kStackParts);
    return true;
  }
#else
  DCHECK(!inline_everything_);
#endif
  if (is_last) {
    last_multiplier_ = multiplier;
  } else {
    DCHECK(max_multiplier_ == 0 || max_multiplier_ == multiplier);
    max_multiplier_ = multiplier;
  }
  return AddPart(part);
}

bool FromStringAccumulator::AddPart(digit_t part) {
  if (stack_parts_used_ < kStackParts) {
    stack_parts_[stack_parts_used_++] = part;
    return true;
  }
  if (heap_parts_.size() == 0) {
    // Initialize heap storage. Copy the stack part to make things easier later.
    heap_parts_.reserve(kStackParts * 2);
    for (uint32_t i = 0; i < kStackParts; i++) {
      heap_parts_.push_back(stack_parts_[i]);
    }
  }
  if (static_cast<uint32_t>(heap_parts_.size()) >= max_digits_) {
    result_ = Result::kMaxSizeExceeded;
    return false;
  }
  heap_parts_.push_back(part);
  return true;
}

// Inlineable internal helpers.

// Different environments disagree on how 64-bit uintptr_t and uint64_t are
// defined, so we have to use templates to be generic.
template <typename T>
constexpr int CountLeadingZeros(T value)
  requires(std::is_unsigned_v<T> && sizeof(T) == 8)
{
#if __GNUC__ || __clang__
  return value == 0 ? 64 : __builtin_clzll(value);
#elif _MSC_VER
  unsigned long index = 0;  // NOLINT(runtime/int). MSVC insists.
  return _BitScanReverse64(&index, value) ? 63 - index : 64;
#else
#error Unsupported compiler.
#endif
}

constexpr int CountLeadingZeros(uint32_t value) {
#if __GNUC__ || __clang__
  return value == 0 ? 32 : __builtin_clz(value);
#elif _MSC_VER
  unsigned long index = 0;  // NOLINT(runtime/int). MSVC insists.
  return _BitScanReverse(&index, value) ? 31 - index : 32;
#else
#error Unsupported compiler.
#endif
}

inline constexpr int BitLength(int n) {
  return 32 - CountLeadingZeros(static_cast<uint32_t>(n));
}

static constexpr int kHalfDigitBits = kDigitBits / 2;
static constexpr digit_t kHalfDigitBase = digit_t{1} << kHalfDigitBits;
static constexpr digit_t kHalfDigitMask = kHalfDigitBase - 1;

constexpr bool digit_ismax(digit_t x) { return static_cast<digit_t>(~x) == 0; }

// {carry} will be set to 0 or 1.
inline digit_t digit_add2(digit_t a, digit_t b, digit_t* carry) {
#if HAVE_TWODIGIT_T
  twodigit_t result = twodigit_t{a} + b;
  *carry = result >> kDigitBits;
  return static_cast<digit_t>(result);
#else
  digit_t result = a + b;
  *carry = (result < a) ? 1 : 0;
  return result;
#endif
}

// This compiles to slightly better machine code than repeated invocations
// of {digit_add2}.
inline digit_t digit_add3(digit_t a, digit_t b, digit_t c, digit_t* carry) {
#if HAVE_TWODIGIT_T
  twodigit_t result = twodigit_t{a} + b + c;
  *carry = result >> kDigitBits;
  return static_cast<digit_t>(result);
#else
  digit_t result = a + b;
  *carry = (result < a) ? 1 : 0;
  result += c;
  if (result < c) *carry += 1;
  return result;
#endif
}

// {borrow} will be set to 0 or 1.
inline digit_t digit_sub(digit_t a, digit_t b, digit_t* borrow) {
#if HAVE_TWODIGIT_T
  twodigit_t result = twodigit_t{a} - b;
  *borrow = (result >> kDigitBits) & 1;
  return static_cast<digit_t>(result);
#else
  digit_t result = a - b;
  *borrow = (result > a) ? 1 : 0;
  return result;
#endif
}

// {borrow_out} will be set to 0 or 1.
inline digit_t digit_sub2(digit_t a, digit_t b, digit_t borrow_in,
                          digit_t* borrow_out) {
#if HAVE_TWODIGIT_T
  twodigit_t subtrahend = twodigit_t{b} + borrow_in;
  twodigit_t result = twodigit_t{a} - subtrahend;
  *borrow_out = (result >> kDigitBits) & 1;
  return static_cast<digit_t>(result);
#else
  digit_t result = a - b;
  *borrow_out = (result > a) ? 1 : 0;
  if (result < borrow_in) *borrow_out += 1;
  result -= borrow_in;
  return result;
#endif
}

// Returns the low half of the result. High half is in {high}.
inline digit_t digit_mul(digit_t a, digit_t b, digit_t* high) {
#if HAVE_TWODIGIT_T
  twodigit_t result = twodigit_t{a} * b;
  *high = result >> kDigitBits;
  return static_cast<digit_t>(result);
#else
  // Multiply in half-pointer-sized chunks.
  // For inputs [AH AL]*[BH BL], the result is:
  //
  //            [AL*BL]  // r_low
  //    +    [AL*BH]     // r_mid1
  //    +    [AH*BL]     // r_mid2
  //    + [AH*BH]        // r_high
  //    = [R4 R3 R2 R1]  // high = [R4 R3], low = [R2 R1]
  //
  // Where of course we must be careful with carries between the columns.
  digit_t a_low = a & kHalfDigitMask;
  digit_t a_high = a >> kHalfDigitBits;
  digit_t b_low = b & kHalfDigitMask;
  digit_t b_high = b >> kHalfDigitBits;

  digit_t r_low = a_low * b_low;
  digit_t r_mid1 = a_low * b_high;
  digit_t r_mid2 = a_high * b_low;
  digit_t r_high = a_high * b_high;

  digit_t carry = 0;
  digit_t low = digit_add3(r_low, r_mid1 << kHalfDigitBits,
                           r_mid2 << kHalfDigitBits, &carry);
  *high =
      (r_mid1 >> kHalfDigitBits) + (r_mid2 >> kHalfDigitBits) + r_high + carry;
  return low;
#endif
}

// Returns the quotient.
// quotient = (high << kDigitBits + low - remainder) / divisor
static inline digit_t digit_div(digit_t high, digit_t low, digit_t divisor,
                                digit_t* remainder) {
#if defined(DCHECK)
  DCHECK(high < divisor);
  DCHECK(divisor != 0);
#endif
#if __x86_64__ && (__GNUC__ || __clang__)
  digit_t quotient;
  digit_t rem;
  __asm__("divq  %[divisor]"
          // Outputs: {quotient} will be in rax, {rem} in rdx.
          : "=a"(quotient), "=d"(rem)
          // Inputs: put {high} into rdx, {low} into rax, and {divisor} into
          // any register or stack slot.
          : "d"(high), "a"(low), [divisor] "rm"(divisor));
  *remainder = rem;
  return quotient;
#elif __i386__ && (__GNUC__ || __clang__)
  digit_t quotient;
  digit_t rem;
  __asm__("divl  %[divisor]"
          // Outputs: {quotient} will be in eax, {rem} in edx.
          : "=a"(quotient), "=d"(rem)
          // Inputs: put {high} into edx, {low} into eax, and {divisor} into
          // any register or stack slot.
          : "d"(high), "a"(low), [divisor] "rm"(divisor));
  *remainder = rem;
  return quotient;
#else
  // Adapted from Warren, Hacker's Delight, p. 152.
  int s = CountLeadingZeros(divisor);
#if defined(DCHECK)
  DCHECK(s != kDigitBits);  // {divisor} is not 0.
#endif
  divisor <<= s;

  digit_t vn1 = divisor >> kHalfDigitBits;
  digit_t vn0 = divisor & kHalfDigitMask;
  // {s} can be 0. {low >> kDigitBits} would be undefined behavior, so
  // we mask the shift amount with {kShiftMask}, and the result with
  // {s_zero_mask} which is 0 if s == 0 and all 1-bits otherwise.
  static_assert(sizeof(intptr_t) == sizeof(digit_t),
                "intptr_t and digit_t must have the same size");
  const int kShiftMask = kDigitBits - 1;
  digit_t s_zero_mask =
      static_cast<digit_t>(static_cast<intptr_t>(-s) >> (kDigitBits - 1));
  digit_t un32 =
      (high << s) | ((low >> ((kDigitBits - s) & kShiftMask)) & s_zero_mask);
  digit_t un10 = low << s;
  digit_t un1 = un10 >> kHalfDigitBits;
  digit_t un0 = un10 & kHalfDigitMask;
  digit_t q1 = un32 / vn1;
  digit_t rhat = un32 - q1 * vn1;

  while (q1 >= kHalfDigitBase || q1 * vn0 > rhat * kHalfDigitBase + un1) {
    q1--;
    rhat += vn1;
    if (rhat >= kHalfDigitBase) break;
  }

  digit_t un21 = un32 * kHalfDigitBase + un1 - q1 * divisor;
  digit_t q0 = un21 / vn1;
  rhat = un21 - q0 * vn1;

  while (q0 >= kHalfDigitBase || q0 * vn0 > rhat * kHalfDigitBase + un0) {
    q0--;
    rhat += vn1;
    if (rhat >= kHalfDigitBase) break;
  }

  *remainder = (un21 * kHalfDigitBase + un0 - q0 * divisor) >> s;
  return q1 * kHalfDigitBase + q0;
#endif
}

inline bool GreaterThanOrEqual(Digits A, Digits B) {
  return Compare(A, B) >= 0;
}

inline bool IsDigitNormalized(Digits X) { return X.len() == 0 || X.msd() != 0; }

// Inlineable implementations of public functions.

inline int CompareNoNormalize(Digits A, Digits B) {
  DCHECK(A.len() < INT32_MAX && B.len() < INT32_MAX);
  int diff = A.len() - B.len();
  if (diff != 0) return diff;
  int i = A.len() - 1;
  while (i >= 0 && A[i] == B[i]) i--;
  if (i < 0) return 0;
  return A[i] > B[i] ? 1 : -1;
}

inline int Compare(Digits A, Digits B) {
  A.Normalize();
  B.Normalize();
  return CompareNoNormalize(A, B);
}

inline digit_t Add(RWDigits Z, Digits X, Digits Y) {
  if (X.len() < Y.len()) std::swap(X, Y);  // Now X.len() >= Y.len().
  CHECK(Z.len() >= X.len());
  uint32_t i = 0;
  digit_t carry = 0;
  digit_t top = 0;
  for (; i < Y.len(); i++) {
    Z[i] = top = digit_add3(X[i], Y[i], carry, &carry);
  }
  for (; i < X.len(); i++) {
    Z[i] = top = digit_add2(X[i], carry, &carry);
  }
  for (; i < Z.len(); i++) {
    Z[i] = top = carry;
    carry = 0;
  }
  return top;
}

inline digit_t Subtract(RWDigits Z, Digits X, Digits Y) {
  DCHECK(IsDigitNormalized(X));
  DCHECK(IsDigitNormalized(Y));
  CHECK(Z.len() >= X.len() && X.len() >= Y.len());
  uint32_t i = 0;
  digit_t borrow = 0;
  digit_t top = 0;
  for (; i < Y.len(); i++) {
    Z[i] = top = digit_sub2(X[i], Y[i], borrow, &borrow);
  }
  for (; i < X.len(); i++) {
    Z[i] = top = digit_sub(X[i], borrow, &borrow);
  }
  DCHECK(borrow == 0);
  for (; i < Z.len(); i++) [[unlikely]] {
    Z[i] = top = 0;
  }
  return top;
}

inline void SubtractWithNormalization(RWDigits Z, Digits X, Digits Y) {
  X.Normalize();
  Y.Normalize();
  Subtract(Z, X, Y);
}

inline bool AddSigned(RWDigits Z, Digits X, bool x_negative, Digits Y,
                      bool y_negative) {
  if (x_negative == y_negative) {
    Add(Z, X, Y);
    return x_negative;
  }
  X.Normalize();
  Y.Normalize();
  if (CompareNoNormalize(X, Y) >= 0) {
    Subtract(Z, X, Y);
    return x_negative;
  }
  Subtract(Z, Y, X);
  return !x_negative;
}

inline bool SubtractSigned(RWDigits Z, Digits X, bool x_negative, Digits Y,
                           bool y_negative) {
  if (x_negative != y_negative) {
    Add(Z, X, Y);
    return x_negative;
  }
  X.Normalize();
  Y.Normalize();
  if (CompareNoNormalize(X, Y) >= 0) {
    Subtract(Z, X, Y);
    return x_negative;
  }
  Subtract(Z, Y, X);
  return !x_negative;
}

inline void AddOne(RWDigits Z, Digits X) {
  digit_t carry = 1;
  uint32_t i = 0;
  for (; carry > 0 && i < X.len(); i++) Z[i] = digit_add2(X[i], carry, &carry);
  if (carry > 0) Z[i++] = carry;
  for (; i < X.len(); i++) Z[i] = X[i];
  for (; i < Z.len(); i++) Z[i] = 0;
}

inline void SubtractOne(RWDigits Z, Digits X) {
  digit_t borrow = 1;
  uint32_t i = 0;
  for (; borrow > 0; i++) Z[i] = digit_sub(X[i], borrow, &borrow);
  for (; i < X.len(); i++) Z[i] = X[i];
  for (; i < Z.len(); i++) Z[i] = 0;
}

// X += y.
inline void Add(RWDigits X, digit_t y) {
  digit_t carry = y;
  uint32_t i = 0;
  do {
    X[i] = digit_add2(X[i], carry, &carry);
    i++;
  } while (carry != 0);
}

// Z := X * y, where y is a single digit. Returns Z's top digit.
inline digit_t MultiplySingle(RWDigits Z, Digits X, digit_t y) {
  DCHECK(y != 0);
  digit_t carry = 0;
  digit_t high = 0;
  for (uint32_t i = 0; i < X.len(); i++) {
    digit_t new_high;
    digit_t low = digit_mul(X[i], y, &new_high);
    Z[i] = digit_add3(low, high, carry, &carry);
    high = new_high;
  }
  digit_t top = carry + high;
  Z[X.len()] = top;
  for (uint32_t i = X.len() + 1; i < Z.len(); i++) [[unlikely]] {
    Z[i] = top = 0;
  }
  return top;
}

#define BODY(min, max)                              \
  for (uint32_t j = min; j <= max; j++) {           \
    digit_t high;                                   \
    digit_t low = digit_mul(X[j], Y[i - j], &high); \
    digit_t carrybit;                               \
    zi = digit_add2(zi, low, &carrybit);            \
    carry += carrybit;                              \
    next = digit_add2(next, high, &carrybit);       \
    next_carry += carrybit;                         \
  }                                                 \
  Z[i] = zi

inline digit_t MultiplySchoolbook(RWDigits Z, Digits X, Digits Y) {
  DCHECK(IsDigitNormalized(X));
  DCHECK(IsDigitNormalized(Y));
  DCHECK(X.len() >= Y.len());
  DCHECK(Z.len() >= X.len() + Y.len());
  if (X.len() == 0 || Y.len() == 0) {
    Z.Clear();
    return 0;
  }
  digit_t next, next_carry = 0, carry = 0;
  // Unrolled first iteration: it's trivial.
  Z[0] = digit_mul(X[0], Y[0], &next);
  uint32_t i = 1;
  // Unrolled second iteration: a little less setup.
  if (i < Y.len()) {
    digit_t zi = next;
    next = 0;
    BODY(0, 1);
    i++;
  }
  // Main part: since X.len() >= Y.len() > i, no bounds checks are needed.
  for (; i < Y.len(); i++) {
    digit_t zi = digit_add2(next, carry, &carry);
    next = next_carry + carry;
    carry = 0;
    next_carry = 0;
    BODY(0, i);
  }
  // Last part: i exceeds Y now, we have to be careful about bounds.
  uint32_t loop_end = X.len() + Y.len() - 2;
  for (; i <= loop_end; i++) {
    uint32_t max_x_index = std::min(i, X.len() - 1);
    uint32_t max_y_index = Y.len() - 1;
    uint32_t min_x_index = i - max_y_index;
    digit_t zi = digit_add2(next, carry, &carry);
    next = next_carry + carry;
    carry = 0;
    next_carry = 0;
    BODY(min_x_index, max_x_index);
  }
  // Write the last digit, and zero out any extra space in Z.
  digit_t top = digit_add2(next, carry, &carry);
  Z[i++] = top;
  DCHECK(carry == 0);
  for (; i < Z.len(); i++) [[unlikely]] {
    Z[i] = top = 0;
  }
  return top;
}
#undef BODY

inline std::pair<bool, digit_t> MultiplySmall(RWDigits& Z, Digits& X,
                                              Digits& Y) {
  DCHECK(IsDigitNormalized(X));
  DCHECK(IsDigitNormalized(Y));
  if (X.len() == 0 || Y.len() == 0) {
    Z.Clear();
    return {true, 0};
  }
  if (X.len() < Y.len()) std::swap(X, Y);
  if (Y.len() == 1) {
    digit_t top = MultiplySingle(Z, X, Y[0]);
    return {true, top};
  }
  if (Y.len() >= config::kKaratsubaThreshold) return {false, 0};
  digit_t top = MultiplySchoolbook(Z, X, Y);
  return {true, top};
}

// Computes Q(uotient) and remainder for A/b, such that
// Q = (A - remainder) / b, with 0 <= remainder < b.
// Returns Q's top digit.
// Q may be the same as A for an in-place division.
inline digit_t DivideSingle(RWDigits Q, digit_t* remainder, Digits A,
                            digit_t b) {
  DCHECK(b != 0);
  DCHECK(A.len() > 0);
  DCHECK(Q.len() >= A.len());
  digit_t rem = 0;
  digit_t top;
  uint32_t length = A.len();
  if (A[length - 1] >= b) {
    DCHECK(Q.len() >= A.len());
    uint32_t i = length - 1;
    Q[i] = top = digit_div(rem, A[i], b, &rem);
    while (i-- > 0) {
      Q[i] = digit_div(rem, A[i], b, &rem);
    }
    for (uint32_t j = length; j < Q.len(); j++) Q[j] = top = 0;
  } else {
    DCHECK(Q.len() >= A.len() - 1);
    rem = A[length - 1];
    top = 0;
    uint32_t i = length - 2;
    if (i >= 0) [[likely]] {
      Q[i] = top = digit_div(rem, A[i], b, &rem);
      while (i-- > 0) {
        Q[i] = digit_div(rem, A[i], b, &rem);
      }
    }
    for (uint32_t j = length - 1; j < Q.len(); j++) Q[j] = top = 0;
  }
  *remainder = rem;
  return top;
}

inline digit_t ModSingle(Digits A, digit_t b) {
  DCHECK(b != 0);
  DCHECK(A.len() > 0);
  digit_t rem = 0;
  uint32_t length = A.len();
  if (A[length - 1] >= b) {
    for (uint32_t i = length; i-- > 0;) {
      digit_div(rem, A[i], b, &rem);
    }
  } else {
    rem = A[length - 1];
    for (uint32_t i = length - 1; i-- > 0;) {
      digit_div(rem, A[i], b, &rem);
    }
  }
  return rem;
}

inline std::pair<bool, digit_t> DivideSmall(RWDigits& Q, Digits& A, Digits& B) {
  DCHECK(IsDigitNormalized(A));
  DCHECK(IsDigitNormalized(B));
  // This is a Release-mode check for security fuzzing purposes.
  CHECK(B.len() > 0);
  int cmp = CompareNoNormalize(A, B);
  if (cmp < 0) {
    Q.Clear();
    return {true, 0};
  }
  if (cmp == 0) {
    digit_t top = 1;
    Q[0] = 1;
    for (uint32_t i = 1; i < Q.len(); i++) Q[i] = top = 0;
    return {true, top};
  }
  if (B.len() == 1) {
    digit_t remainder;
    digit_t top = DivideSingle(Q, &remainder, A, B[0]);
    return {true, top};
  }
  return {false, 0};
}

inline std::pair<bool, digit_t> ModuloSmall(RWDigits& R, Digits& A, Digits& B) {
  DCHECK(IsDigitNormalized(A));
  DCHECK(IsDigitNormalized(B));
  // This is a Release-mode check for security fuzzing purposes.
  CHECK(B.len() > 0);
  int cmp = CompareNoNormalize(A, B);
  if (cmp < 0) {
    digit_t top;
    for (uint32_t i = 0; i < B.len(); i++) R[i] = top = B[i];
    for (uint32_t i = B.len(); i < R.len(); i++) R[i] = top = 0;
    return {true, top};
  }
  if (cmp == 0) {
    R.Clear();
    return {true, 0};
  }
  if (B.len() == 1) {
    digit_t top = ModSingle(A, B[0]);
    R[0] = top;
    for (uint32_t i = 1; i < R.len(); i++) [[unlikely]] {
      R[i] = top = 0;
    }
    return {true, top};
  }
  return {false, 0};
}

inline uint32_t DivideResultLength(Digits A, Digits B) {
#if V8_ADVANCED_BIGINT_ALGORITHMS
  DCHECK(kAdvancedAlgorithmsEnabledInLibrary);
  // The Barrett division algorithm needs one extra digit for temporary use.
  uint32_t kBarrettExtraScratch = B.len() >= config::kBarrettThreshold ? 1 : 0;
#else
  // If this fails, set -DV8_ADVANCED_BIGINT_ALGORITHMS in any compilation unit
  // that #includes this header.
  DCHECK(!kAdvancedAlgorithmsEnabledInLibrary);
  constexpr uint32_t kBarrettExtraScratch = 0;
#endif
  return A.len() - B.len() + 1 + kBarrettExtraScratch;
}

inline void BitwiseAnd_PosPos(RWDigits Z, Digits X, Digits Y) {
  uint32_t pairs = std::min(X.len(), Y.len());
  DCHECK(Z.len() >= pairs);
  uint32_t i = 0;
  for (; i < pairs; i++) Z[i] = X[i] & Y[i];
  for (; i < Z.len(); i++) Z[i] = 0;
}

inline void BitwiseAnd_NegNeg(RWDigits Z, Digits X, Digits Y) {
  // (-x) & (-y) == ~(x-1) & ~(y-1)
  //             == ~((x-1) | (y-1))
  //             == -(((x-1) | (y-1)) + 1)
  uint32_t pairs = std::min(X.len(), Y.len());
  digit_t x_borrow = 1;
  digit_t y_borrow = 1;
  uint32_t i = 0;
  for (; i < pairs; i++) {
    Z[i] = digit_sub(X[i], x_borrow, &x_borrow) |
           digit_sub(Y[i], y_borrow, &y_borrow);
  }
  // (At least) one of the next two loops will perform zero iterations:
  for (; i < X.len(); i++) Z[i] = digit_sub(X[i], x_borrow, &x_borrow);
  for (; i < Y.len(); i++) Z[i] = digit_sub(Y[i], y_borrow, &y_borrow);
  DCHECK(x_borrow == 0);
  DCHECK(y_borrow == 0);
  for (; i < Z.len(); i++) Z[i] = 0;
  Add(Z, 1);
}

inline void BitwiseAnd_PosNeg(RWDigits Z, Digits X, Digits Y) {
  // x & (-y) == x & ~(y-1)
  uint32_t pairs = std::min(X.len(), Y.len());
  digit_t borrow = 1;
  uint32_t i = 0;
  for (; i < pairs; i++) Z[i] = X[i] & ~digit_sub(Y[i], borrow, &borrow);
  for (; i < X.len(); i++) Z[i] = X[i];
  for (; i < Z.len(); i++) Z[i] = 0;
}

inline void BitwiseOr_PosPos(RWDigits Z, Digits X, Digits Y) {
  uint32_t pairs = std::min(X.len(), Y.len());
  uint32_t i = 0;
  for (; i < pairs; i++) Z[i] = X[i] | Y[i];
  // (At least) one of the next two loops will perform zero iterations:
  for (; i < X.len(); i++) Z[i] = X[i];
  for (; i < Y.len(); i++) Z[i] = Y[i];
  for (; i < Z.len(); i++) Z[i] = 0;
}

inline void BitwiseOr_NegNeg(RWDigits Z, Digits X, Digits Y) {
  // (-x) | (-y) == ~(x-1) | ~(y-1)
  //             == ~((x-1) & (y-1))
  //             == -(((x-1) & (y-1)) + 1)
  uint32_t pairs = std::min(X.len(), Y.len());
  digit_t x_borrow = 1;
  digit_t y_borrow = 1;
  uint32_t i = 0;
  for (; i < pairs; i++) {
    Z[i] = digit_sub(X[i], x_borrow, &x_borrow) &
           digit_sub(Y[i], y_borrow, &y_borrow);
  }
  // Any leftover borrows don't matter, the '&' would drop them anyway.
  for (; i < Z.len(); i++) Z[i] = 0;
  Add(Z, 1);
}

inline void BitwiseOr_PosNeg(RWDigits Z, Digits X, Digits Y) {
  // x | (-y) == x | ~(y-1) == ~((y-1) &~ x) == -(((y-1) &~ x) + 1)
  uint32_t pairs = std::min(X.len(), Y.len());
  digit_t borrow = 1;
  uint32_t i = 0;
  for (; i < pairs; i++) Z[i] = digit_sub(Y[i], borrow, &borrow) & ~X[i];
  for (; i < Y.len(); i++) Z[i] = digit_sub(Y[i], borrow, &borrow);
  DCHECK(borrow == 0);
  for (; i < Z.len(); i++) Z[i] = 0;
  Add(Z, 1);
}

inline void BitwiseXor_PosPos(RWDigits Z, Digits X, Digits Y) {
  uint32_t pairs = X.len();
  if (Y.len() < X.len()) {
    std::swap(X, Y);
    pairs = X.len();
  }
  DCHECK(X.len() <= Y.len());
  uint32_t i = 0;
  for (; i < pairs; i++) Z[i] = X[i] ^ Y[i];
  for (; i < Y.len(); i++) Z[i] = Y[i];
  for (; i < Z.len(); i++) Z[i] = 0;
}

inline void BitwiseXor_NegNeg(RWDigits Z, Digits X, Digits Y) {
  // (-x) ^ (-y) == ~(x-1) ^ ~(y-1) == (x-1) ^ (y-1)
  uint32_t pairs = std::min(X.len(), Y.len());
  digit_t x_borrow = 1;
  digit_t y_borrow = 1;
  uint32_t i = 0;
  for (; i < pairs; i++) {
    Z[i] = digit_sub(X[i], x_borrow, &x_borrow) ^
           digit_sub(Y[i], y_borrow, &y_borrow);
  }
  // (At least) one of the next two loops will perform zero iterations:
  for (; i < X.len(); i++) Z[i] = digit_sub(X[i], x_borrow, &x_borrow);
  for (; i < Y.len(); i++) Z[i] = digit_sub(Y[i], y_borrow, &y_borrow);
  DCHECK(x_borrow == 0);
  DCHECK(y_borrow == 0);
  for (; i < Z.len(); i++) Z[i] = 0;
}

inline void BitwiseXor_PosNeg(RWDigits Z, Digits X, Digits Y) {
  // x ^ (-y) == x ^ ~(y-1) == ~(x ^ (y-1)) == -((x ^ (y-1)) + 1)
  uint32_t pairs = std::min(X.len(), Y.len());
  digit_t borrow = 1;
  uint32_t i = 0;
  for (; i < pairs; i++) Z[i] = X[i] ^ digit_sub(Y[i], borrow, &borrow);
  // (At least) one of the next two loops will perform zero iterations:
  for (; i < X.len(); i++) Z[i] = X[i];
  for (; i < Y.len(); i++) Z[i] = digit_sub(Y[i], borrow, &borrow);
  DCHECK(borrow == 0);
  for (; i < Z.len(); i++) Z[i] = 0;
  Add(Z, 1);
}

inline void LeftShift(RWDigits Z, Digits X, digit_t shift) {
  uint32_t digit_shift = static_cast<uint32_t>(shift / kDigitBits);
  int bits_shift = static_cast<int>(shift % kDigitBits);

  uint32_t i = 0;
  for (; i < digit_shift; ++i) Z[i] = 0;
  if (bits_shift == 0) {
    for (; i < X.len() + digit_shift; ++i) Z[i] = X[i - digit_shift];
    for (; i < Z.len(); ++i) Z[i] = 0;
  } else {
    digit_t carry = 0;
    for (; i < X.len() + digit_shift; ++i) {
      digit_t d = X[i - digit_shift];
      Z[i] = (d << bits_shift) | carry;
      carry = d >> (kDigitBits - bits_shift);
    }
    if (carry != 0) Z[i++] = carry;
    for (; i < Z.len(); ++i) Z[i] = 0;
  }
}

inline void RightShift(RWDigits Z, Digits X, digit_t shift,
                       const RightShiftState& state) {
  uint32_t digit_shift = static_cast<uint32_t>(shift / kDigitBits);
  int bits_shift = static_cast<int>(shift % kDigitBits);

  uint32_t i = 0;
  if (bits_shift == 0) {
    for (; i < X.len() - digit_shift; ++i) Z[i] = X[i + digit_shift];
  } else {
    digit_t carry = X[digit_shift] >> bits_shift;
    for (; i < X.len() - digit_shift - 1; ++i) {
      digit_t d = X[i + digit_shift + 1];
      Z[i] = (d << (kDigitBits - bits_shift)) | carry;
      carry = d >> bits_shift;
    }
    Z[i++] = carry;
  }
  for (; i < Z.len(); ++i) Z[i] = 0;

  if (state.must_round_down) {
    // Rounding down (a negative value) means adding one to
    // its absolute value. This cannot overflow.
    Add(Z, 1);
  }
}

inline uint32_t RightShift_ResultLength(Digits X, bool x_sign, digit_t shift,
                                        RightShiftState* state) {
  uint32_t digit_shift = static_cast<uint32_t>(shift / kDigitBits);
  int bits_shift = static_cast<int>(shift % kDigitBits);
  if (X.len() <= digit_shift) return 0;
  uint32_t result_length = X.len() - digit_shift;

  // For negative numbers, round down if any bit was shifted out (so that e.g.
  // -5n >> 1n == -3n and not -2n). Check now whether this will happen and
  // whether it can cause overflow into a new digit.
  bool must_round_down = false;
  if (x_sign) {
    const digit_t mask = (digit_t{1} << bits_shift) - 1;
    if ((X[digit_shift] & mask) != 0) {
      must_round_down = true;
    } else {
      for (uint32_t i = 0; i < digit_shift; i++) {
        if (X[i] != 0) {
          must_round_down = true;
          break;
        }
      }
    }
  }
  // If bits_shift is non-zero, it frees up bits, preventing overflow.
  if (must_round_down && bits_shift == 0) {
    // Overflow cannot happen if the most significant digit has unset bits.
    const bool rounding_can_overflow = digit_ismax(X.msd());
    if (rounding_can_overflow) ++result_length;
  }

  if (state) {
    DCHECK(!must_round_down || x_sign);
    state->must_round_down = must_round_down;
  }
  return result_length;
}

// Z := (least significant n bits of X).
inline void TruncateToNBits(RWDigits Z, Digits X, uint32_t n) {
  uint32_t digits = DIV_CEIL(n, kDigitBits);
  int bits = n % kDigitBits;
  // Copy all digits except the MSD.
  uint32_t last = digits - 1;
  for (uint32_t i = 0; i < last; i++) {
    Z[i] = X[i];
  }
  // The MSD might contain extra bits that we don't want.
  digit_t msd = X[last];
  if (bits != 0) {
    int drop = kDigitBits - bits;
    msd = (msd << drop) >> drop;
  }
  Z[last] = msd;
}

// Z := 2**n - (least significant n bits of X).
inline void TruncateAndSubFromPowerOfTwo(RWDigits Z, Digits X, uint32_t n) {
  uint32_t digits = DIV_CEIL(n, kDigitBits);
  int bits = n % kDigitBits;
  // Process all digits except the MSD. Take X's digits, then simulate leading
  // zeroes.
  uint32_t last = digits - 1;
  uint32_t have_x = std::min(last, X.len());
  digit_t borrow = 0;
  uint32_t i = 0;
  for (; i < have_x; i++) Z[i] = digit_sub2(0, X[i], borrow, &borrow);
  for (; i < last; i++) Z[i] = digit_sub(0, borrow, &borrow);

  // The MSD might contain extra bits that we don't want.
  digit_t msd = last < X.len() ? X[last] : 0;
  if (bits == 0) {
    Z[last] = digit_sub2(0, msd, borrow, &borrow);
  } else {
    int drop = kDigitBits - bits;
    msd = (msd << drop) >> drop;
    digit_t minuend_msd = static_cast<digit_t>(1) << bits;
    digit_t result_msd = digit_sub2(minuend_msd, msd, borrow, &borrow);
    DCHECK(borrow == 0);  // result < 2^n.
    // If all subtracted bits were zero, we have to get rid of the
    // materialized minuend_msd again.
    Z[last] = result_msd & (minuend_msd - 1);
  }
}

inline bool AsIntN(RWDigits Z, Digits X, bool x_negative, uint32_t n) {
  DCHECK(X.len() > 0);
  DCHECK(n > 0);
  DCHECK(AsIntNResultLength(X, x_negative, n) > 0);
  uint32_t needed_digits = DIV_CEIL(n, kDigitBits);
  digit_t top_digit = X[needed_digits - 1];
  digit_t compare_digit = digit_t{1} << ((n - 1) % kDigitBits);
  // The canonical algorithm would be: convert negative numbers to two's
  // complement representation, truncate, convert back to sign+magnitude. To
  // avoid the conversions, we predict what the result would be:
  // When the (n-1)th bit is not set:
  //  - truncate the absolute value
  //  - preserve the sign.
  // When the (n-1)th bit is set:
  //  - subtract the truncated absolute value from 2**n to simulate two's
  //    complement representation
  //  - flip the sign, unless it's the special case where the input is negative
  //    and the result is the minimum n-bit integer. E.g. asIntN(3, -12) => -4.
  bool has_bit = (top_digit & compare_digit) == compare_digit;
  if (!has_bit) {
    TruncateToNBits(Z, X, n);
    return x_negative;
  }
  TruncateAndSubFromPowerOfTwo(Z, X, n);
  if (!x_negative) return true;  // Result is negative.
  // Scan for the special case (see above): if all bits below the (n-1)th
  // digit are zero, the result is negative.
  if ((top_digit & (compare_digit - 1)) != 0) return false;
  for (int i = needed_digits - 2; i >= 0; i--) {
    if (X[i] != 0) return false;
  }
  return true;
}

// Returns -1 when the operation would return X unchanged.
int AsIntNResultLength(Digits X, bool x_negative, uint32_t n) {
  uint32_t needed_digits = DIV_CEIL(n, kDigitBits);
  // Generally: decide based on number of digits, and bits in the top digit.
  if (X.len() < needed_digits) return -1;
  if (X.len() > needed_digits) return needed_digits;
  digit_t top_digit = X[needed_digits - 1];
  digit_t compare_digit = digit_t{1} << ((n - 1) % kDigitBits);
  if (top_digit < compare_digit) return -1;
  if (top_digit > compare_digit) return needed_digits;
  // Special case: if X == -2**(n-1), truncation is a no-op.
  if (!x_negative) return needed_digits;
  for (int i = needed_digits - 2; i >= 0; i--) {
    if (X[i] != 0) return needed_digits;
  }
  return -1;
}

// Returns -1 when the operation would return X unchanged.
int AsUintN_Pos_ResultLength(Digits X, uint32_t n) {
  uint32_t needed_digits = DIV_CEIL(n, kDigitBits);
  if (X.len() < needed_digits) return -1;
  if (X.len() > needed_digits) return needed_digits;
  int bits_in_top_digit = n % kDigitBits;
  if (bits_in_top_digit == 0) return -1;
  digit_t top_digit = X[needed_digits - 1];
  if ((top_digit >> bits_in_top_digit) == 0) return -1;
  return needed_digits;
}

inline void AsUintN_Pos(RWDigits Z, Digits X, uint32_t n) {
  DCHECK(AsUintN_Pos_ResultLength(X, n) > 0);
  TruncateToNBits(Z, X, n);
}

inline void AsUintN_Neg(RWDigits Z, Digits X, uint32_t n) {
  TruncateAndSubFromPowerOfTwo(Z, X, n);
}

#undef ALWAYS_INLINE
#undef HAVE_BUILTIN_MUL_OVERFLOW

}  // namespace v8::bigint

#endif  // V8_BIGINT_BIGINT_INL_H_
