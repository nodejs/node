// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_BIGINT_BIGINT_H_
#define V8_BIGINT_BIGINT_H_

#include <stdint.h>

#include <algorithm>
#include <cstring>
#include <iostream>

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
  Digits(digit_t* mem, int len) : digits_(mem), len_(len) {
    // Require 4-byte alignment (even on 64-bit platforms).
    // TODO(jkummerow): See if we can tighten BigInt alignment in V8 to
    // system pointer size, and raise this requirement to that.
    BIGINT_H_DCHECK((reinterpret_cast<uintptr_t>(mem) & 3) == 0);
  }
  // Provides a "slice" view into another Digits object.
  Digits(Digits src, int offset, int len)
      : digits_(src.digits_ + offset),
        len_(std::max(0, std::min(src.len_ - offset, len))) {
    BIGINT_H_DCHECK(offset >= 0);
  }
  // Alternative way to get a "slice" view into another Digits object.
  Digits operator+(int i) {
    BIGINT_H_DCHECK(i >= 0 && i <= len_);
    return Digits(digits_ + i, len_ - i);
  }

  // Provides access to individual digits.
  digit_t operator[](int i) {
    BIGINT_H_DCHECK(i >= 0 && i < len_);
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

  int len() { return len_; }
  const digit_t* digits() const { return digits_; }

 protected:
  friend class TemporaryLeftShift;
  digit_t* digits_;
  int len_;

 private:
  // We require externally-provided digits arrays to be 4-byte aligned, but
  // not necessarily 8-byte aligned; so on 64-bit platforms we use memcpy
  // to allow unaligned reads.
  digit_t read_4byte_aligned(int i) {
    if (sizeof(digit_t) == 4) {
      return digits_[i];
    } else {
      digit_t result;
      memcpy(&result, digits_ + i, sizeof(result));
      return result;
    }
  }
};

// Writable version of a Digits array.
// Does not own the memory it points at.
class RWDigits : public Digits {
 public:
  RWDigits(digit_t* mem, int len) : Digits(mem, len) {}
  RWDigits(RWDigits src, int offset, int len) : Digits(src, offset, len) {}
  RWDigits operator+(int i) {
    BIGINT_H_DCHECK(i >= 0 && i <= len_);
    return RWDigits(digits_ + i, len_ - i);
  }

#if UINTPTR_MAX == 0xFFFFFFFF
  digit_t& operator[](int i) {
    BIGINT_H_DCHECK(i >= 0 && i < len_);
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

  WritableDigitReference operator[](int i) {
    BIGINT_H_DCHECK(i >= 0 && i < len_);
    return WritableDigitReference(digits_ + i);
  }
#endif

  digit_t* digits() { return digits_; }
  void set_len(int len) { len_ = len; }

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
// operations are methods on a {BigIntProcessor} object, which provides
// support for interrupting execution via the {Platform}'s {InterruptRequested}
// mechanism when it takes too long. These functions return a {Status} value.

// Returns r such that r < 0 if A < B; r > 0 if A > B; r == 0 if A == B.
int Compare(Digits A, Digits B);

enum class Status { kOk, kInterrupted };

class Processor {
 public:
  // Takes ownership of {platform}.
  static Processor* New(Platform* platform);

  // Use this for any std::unique_ptr holding an instance of BigIntProcessor.
  class Destroyer {
   public:
    void operator()(Processor* proc) { proc->Destroy(); }
  };
  // When not using std::unique_ptr, call this to delete the instance.
  void Destroy();

  // Z := X * Y
  Status Multiply(RWDigits Z, Digits X, Digits Y);
};

inline int MultiplyResultLength(Digits X, Digits Y) {
  return X.len() + Y.len();
}

}  // namespace bigint
}  // namespace v8

#undef BIGINT_H_DCHECK

#endif  // V8_BIGINT_BIGINT_H_
