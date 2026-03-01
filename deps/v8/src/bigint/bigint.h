// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_BIGINT_BIGINT_H_
#define V8_BIGINT_BIGINT_H_

#include <stdint.h>

#include <algorithm>
#include <cstring>
#include <iostream>
#include <utility>

namespace v8 {
namespace bigint {

// To play nice with embedders' macros, we define our own DCHECK here.
// It's only used in this file, and undef'ed at the end.
#ifdef DEBUG
#define BIGINT_H_DCHECK(cond)                         \
  if (!(cond)) {                                      \
    std::cerr << __FILE__ << ":" << __LINE__ << ": "; \
    std::cerr << "Assertion failed: " #cond "\n";     \
    abort();                                          \
  }

extern bool kAdvancedAlgorithmsEnabledInLibrary;
#else
#define BIGINT_H_DCHECK(cond) (void(0))
#endif

// The type of a digit: a register-width unsigned integer.
using digit_t = uintptr_t;
using signed_digit_t = intptr_t;
#if UINTPTR_MAX == 0xFFFFFFFF
// 32-bit platform.
using twodigit_t = uint64_t;
#define HAVE_TWODIGIT_T 1
static constexpr int kLog2DigitBits = 5;
#elif UINTPTR_MAX == 0xFFFFFFFFFFFFFFFF
// 64-bit platform.
static constexpr int kLog2DigitBits = 6;
#if defined(__SIZEOF_INT128__)
using twodigit_t = __uint128_t;
#define HAVE_TWODIGIT_T 1
#endif  // defined(__SIZEOF_INT128__)
#else
#error Unsupported platform.
#endif
static constexpr int kDigitBits = 1 << kLog2DigitBits;
static_assert(kDigitBits == 8 * sizeof(digit_t), "inconsistent type sizes");

// Describes an array of digits, also known as a BigInt. Unsigned.
// Does not own the memory it points at, and only gives read-only access to it.
// Digits are stored in little-endian order.
class Digits {
 public:
  // This is the constructor intended for public consumption.
  Digits(const digit_t* mem, uint32_t len)
      // The const_cast here is ugly, but we need the digits field to be mutable
      // for the RWDigits subclass. We pinky swear to not mutate the memory with
      // this class.
      : Digits(const_cast<digit_t*>(mem), len) {}

  Digits(digit_t* mem, uint32_t len) : digits_(mem), len_(len) {
    // Require 4-byte alignment (even on 64-bit platforms).
    // TODO(jkummerow): See if we can tighten BigInt alignment in V8 to
    // system pointer size, and raise this requirement to that.
    BIGINT_H_DCHECK((reinterpret_cast<uintptr_t>(mem) & 3) == 0);
  }

  // Provides a "slice" view into another Digits object.
  Digits(Digits src, uint32_t offset, uint32_t len)
      : digits_(src.digits_ + offset),
        len_(std::min(len, src.len_ > offset ? src.len_ - offset : 0)) {
    // offset > src.len_ is fine, but if offset looks like an underflow just
    // happened, that's suspicious.
    BIGINT_H_DCHECK(offset <= INT32_MAX);
  }

  Digits() : Digits(static_cast<digit_t*>(nullptr), 0) {}

  // Alternative way to get a "slice" view into another Digits object.
  Digits operator+(uint32_t i) {
    BIGINT_H_DCHECK(i <= len_);
    return Digits(digits_ + i, len_ - i);
  }

  // Provides access to individual digits.
  digit_t operator[](uint32_t i) {
    BIGINT_H_DCHECK(i < len_);
    return read_4byte_aligned(i);
  }
  // Convenience accessor for the most significant digit.
  digit_t msd() {
    BIGINT_H_DCHECK(len_ > 0);
    return read_4byte_aligned(len_ - 1);
  }
  // Checks "pointer equality" (does not compare digits contents).
  bool operator==(const Digits& other) const {
    return digits_ == other.digits_ && len_ == other.len_;
  }

  // Decrements {len_} until there are no leading zero digits left.
  void Normalize() {
    while (len_ > 0 && msd() == 0) len_--;
  }
  // Unconditionally drops exactly one leading zero digit.
  void TrimOne() {
    BIGINT_H_DCHECK(len_ > 0 && msd() == 0);
    len_--;
  }

  uint32_t len() { return len_; }
  const digit_t* digits() const { return digits_; }

 protected:
  friend class ShiftedDigits;
  digit_t* digits_;
  uint32_t len_;

 private:
  // We require externally-provided digits arrays to be 4-byte aligned, but
  // not necessarily 8-byte aligned; so on 64-bit platforms we use memcpy
  // to allow unaligned reads.
  digit_t read_4byte_aligned(uint32_t i) {
    if (sizeof(digit_t) == 4) {
      return digits_[i];
    } else {
      digit_t result;
      memcpy(&result, static_cast<const void*>(digits_ + i), sizeof(result));
      return result;
    }
  }
};

// Writable version of a Digits array.
// Does not own the memory it points at.
class RWDigits : public Digits {
 public:
  RWDigits(digit_t* mem, uint32_t len) : Digits(mem, len) {}
  RWDigits(RWDigits src, uint32_t offset, uint32_t len)
      : Digits(src, offset, len) {}
  RWDigits operator+(uint32_t i) {
    BIGINT_H_DCHECK(i <= len_);
    return RWDigits(digits_ + i, len_ - i);
  }

#if UINTPTR_MAX == 0xFFFFFFFF
  digit_t& operator[](uint32_t i) {
    BIGINT_H_DCHECK(i < len_);
    return digits_[i];
  }
#else
  // 64-bit platform. We only require digits arrays to be 4-byte aligned,
  // so we use a wrapper class to allow regular array syntax while
  // performing unaligned memory accesses under the hood.
  class WritableDigitReference {
   public:
    // Support "X[i] = x" notation.
    void operator=(digit_t digit) { memcpy(ptr_, &digit, sizeof(digit)); }
    // Support "X[i] = Y[j]" notation.
    WritableDigitReference& operator=(const WritableDigitReference& src) {
      memcpy(ptr_, src.ptr_, sizeof(digit_t));
      return *this;
    }
    // Support "x = X[i]" notation.
    operator digit_t() {
      digit_t result;
      memcpy(&result, ptr_, sizeof(result));
      return result;
    }

   private:
    // This class is not for public consumption.
    friend class RWDigits;
    // Primary constructor.
    explicit WritableDigitReference(digit_t* ptr)
        : ptr_(reinterpret_cast<uint32_t*>(ptr)) {}
    // Required for returning WDR instances from "operator[]" below.
    WritableDigitReference(const WritableDigitReference& src) = default;

    uint32_t* ptr_;
  };

  WritableDigitReference operator[](uint32_t i) {
    BIGINT_H_DCHECK(i < len_);
    return WritableDigitReference(digits_ + i);
  }
#endif

  digit_t* digits() { return digits_; }
  void set_len(uint32_t len) { len_ = len; }

  void Clear() { memset(digits_, 0, len_ * sizeof(digit_t)); }
};

class Platform {
 public:
  virtual ~Platform() = default;

  // If you want the ability to interrupt long-running operations, implement
  // a Platform subclass that overrides this method. It will be queried
  // every now and then by long-running operations.
  virtual bool InterruptRequested() { return false; }
};

// These are the operations that this library supports.
// The signatures follow the convention:
//
//   void Operation(RWDigits results, Digits inputs);
//
// You must preallocate the result; use the respective {OperationResultLength}
// function to determine its minimum required length. The actual result may
// be smaller, so you should call result.Normalize() on the result.
//
// The operations are divided into two groups: "fast" (O(n) with small
// coefficient) operations are exposed directly as free functions, "slow"
// operations are split into a free function handling simple cases and
// a method on a {Processor} object for larger cases, which provides
// support for interrupting execution via the {Platform}'s {InterruptRequested}
// mechanism when it takes too long. These functions return a {Status} value.

// Returns r such that r < 0 if A < B; r > 0 if A > B; r == 0 if A == B.
inline int Compare(Digits A, Digits B);

// Z := X + Y. Returns Z's top digit.
inline digit_t Add(RWDigits Z, Digits X, Digits Y);
// Addition of signed integers. Returns true if the result is negative.
inline bool AddSigned(RWDigits Z, Digits X, bool x_negative, Digits Y,
                      bool y_negative);
// Z := X + 1
inline void AddOne(RWDigits Z, Digits X);

// Z := X - Y. Requires X >= Y. Returns Z's top digit.
inline digit_t Subtract(RWDigits Z, Digits X, Digits Y);
// Subtraction of signed integers. Returns true if the result is negative.
inline bool SubtractSigned(RWDigits Z, Digits X, bool x_negative, Digits Y,
                           bool y_negative);
// Z := X - 1
inline void SubtractOne(RWDigits Z, Digits X);

// The bitwise operations assume that negative BigInts are represented as
// sign+magnitude. Their behavior depends on the sign of the inputs: negative
// inputs perform an implicit conversion to two's complement representation.
// Z := X & Y
inline void BitwiseAnd_PosPos(RWDigits Z, Digits X, Digits Y);
// Call this for a BigInt x = (magnitude=X, negative=true).
inline void BitwiseAnd_NegNeg(RWDigits Z, Digits X, Digits Y);
// Positive X, negative Y. Callers must swap arguments as needed.
inline void BitwiseAnd_PosNeg(RWDigits Z, Digits X, Digits Y);
inline void BitwiseOr_PosPos(RWDigits Z, Digits X, Digits Y);
inline void BitwiseOr_NegNeg(RWDigits Z, Digits X, Digits Y);
inline void BitwiseOr_PosNeg(RWDigits Z, Digits X, Digits Y);
inline void BitwiseXor_PosPos(RWDigits Z, Digits X, Digits Y);
inline void BitwiseXor_NegNeg(RWDigits Z, Digits X, Digits Y);
inline void BitwiseXor_PosNeg(RWDigits Z, Digits X, Digits Y);
inline void LeftShift(RWDigits Z, Digits X, digit_t shift);
// RightShiftState is provided by RightShift_ResultLength and used by the actual
// RightShift to avoid some recomputation.
struct RightShiftState {
  bool must_round_down = false;
};
inline void RightShift(RWDigits Z, Digits X, digit_t shift,
                       const RightShiftState& state);

// Z := (least significant n bits of X, interpreted as a signed n-bit integer).
// Returns true if the result is negative; Z will hold the absolute value.
inline bool AsIntN(RWDigits Z, Digits X, bool x_negative, uint32_t n);
// Z := (least significant n bits of X).
inline void AsUintN_Pos(RWDigits Z, Digits X, uint32_t n);
// Same, but X is the absolute value of a negative BigInt.
inline void AsUintN_Neg(RWDigits Z, Digits X, uint32_t n);

// "Small" variants of "slow" operations. They return a pair: if they handled
// the given input, <true, Z[Z.len()-1]>; otherwise <false, undefined>.

// Z := X * Y
inline std::pair<bool, digit_t> MultiplySmall(RWDigits& Z, Digits& X,
                                              Digits& Y);

// Q := A / B
inline std::pair<bool, digit_t> DivideSmall(RWDigits& Q, Digits& A, Digits& B);

// R := A % B
inline std::pair<bool, digit_t> ModuloSmall(RWDigits& R, Digits& A, Digits& B);

enum class Status { kOk, kInterrupted };

class FromStringAccumulator;

class Processor {
 public:
  // Takes ownership of {platform}.
  static Processor* New(Platform* platform);

  // Use this for any std::unique_ptr holding an instance of {Processor}.
  class Destroyer {
   public:
    void operator()(Processor* proc) { proc->Destroy(); }
  };
  // When not using std::unique_ptr, call this to delete the instance.
  void Destroy();

  // Z := X * Y
  Status MultiplyLarge(RWDigits& Z, Digits& X, Digits& Y);
  // Q := A / B
  Status DivideLarge(RWDigits& Q, Digits& A, Digits& B);
  // R := A % B
  Status ModuloLarge(RWDigits& R, Digits& A, Digits& B);

  // {out_length} initially contains the allocated capacity of {out}, and
  // upon return will be set to the actual length of the result string.
  Status ToString(char* out, uint32_t* out_length, Digits& X, int radix,
                  bool sign);

  // Z := the contents of {accumulator}.
  // Assume that this leaves {accumulator} in unusable state.
  Status FromString(RWDigits Z, FromStringAccumulator* accumulator);

 protected:
  // Use {Destroy} or {Destroyer} instead of the destructor directly.
  ~Processor() = default;
};

inline uint32_t AddResultLength(uint32_t x_length, uint32_t y_length) {
  return std::max(x_length, y_length) + 1;
}
inline uint32_t AddSignedResultLength(uint32_t x_length, uint32_t y_length,
                                      bool same_sign) {
  return same_sign ? AddResultLength(x_length, y_length)
                   : std::max(x_length, y_length);
}
inline uint32_t SubtractResultLength(uint32_t x_length, uint32_t y_length) {
  return x_length;
}
inline uint32_t SubtractSignedResultLength(uint32_t x_length, uint32_t y_length,
                                           bool same_sign) {
  return same_sign ? std::max(x_length, y_length)
                   : AddResultLength(x_length, y_length);
}
inline uint32_t MultiplyResultLength(Digits X, Digits Y) {
  BIGINT_H_DCHECK(X.len() <= UINT32_MAX - Y.len());
  return X.len() + Y.len();
}
inline uint32_t DivideResultLength(Digits A, Digits B);
inline uint32_t ModuloResultLength(Digits B) { return B.len(); }

uint32_t ToStringResultLength(Digits X, int radix, bool sign);
// In DEBUG builds, the result of {ToString} will be initialized to this value.
constexpr char kStringZapValue = '?';

inline uint32_t RightShift_ResultLength(Digits X, bool x_sign, digit_t shift,
                                        RightShiftState* state);

// Returns -1 if this "asIntN" operation would be a no-op.
inline int AsIntNResultLength(Digits X, bool x_negative, uint32_t n);
// Returns -1 if this "asUintN" operation would be a no-op.
inline int AsUintN_Pos_ResultLength(Digits X, uint32_t n);
inline uint32_t AsUintN_Neg_ResultLength(uint32_t n) {
  return ((n - 1) / kDigitBits) + 1;
}

}  // namespace bigint
}  // namespace v8

#undef BIGINT_H_DCHECK

#endif  // V8_BIGINT_BIGINT_H_
