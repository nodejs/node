// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_BIGINT_BIGINT_INTERNAL_H_
#define V8_BIGINT_BIGINT_INTERNAL_H_

#include <memory>

#include "src/bigint/bigint.h"

namespace v8 {
namespace bigint {

class ProcessorImpl : public Processor {
 public:
  explicit ProcessorImpl(Platform* platform);
  ~ProcessorImpl();

  Status get_and_clear_status();

  void Multiply(RWDigits Z, Digits X, Digits Y);
  void MultiplyLarge(RWDigits& Z, Digits& X, Digits& Y);

  void MultiplyKaratsuba(RWDigits Z, Digits X, Digits Y);
  void KaratsubaStart(RWDigits Z, Digits X, Digits Y, RWDigits scratch,
                      uint32_t k);
  void KaratsubaChunk(RWDigits Z, Digits X, Digits Y, RWDigits scratch);
  void KaratsubaMain(RWDigits Z, Digits X, Digits Y, RWDigits scratch,
                     uint32_t n);

  void DivideSchoolbook(RWDigits& Q, RWDigits& R, Digits& A, Digits& B);
  void DivideBurnikelZiegler(RWDigits Q, RWDigits R, Digits A, Digits B);

#if V8_ADVANCED_BIGINT_ALGORITHMS
  void MultiplyToomCook(RWDigits Z, Digits X, Digits Y);
  void Toom3Main(RWDigits Z, Digits X, Digits Y);

  void MultiplyFFT(RWDigits Z, Digits X, Digits Y);

  void DivideBarrett(RWDigits Q, RWDigits R, Digits A, Digits B);
  void DivideBarrett(RWDigits Q, RWDigits R, Digits A, Digits B, Digits I,
                     RWDigits scratch);

  void Invert(RWDigits Z, Digits V, RWDigits scratch);
  void InvertBasecase(RWDigits Z, Digits V, RWDigits scratch);
  void InvertNewton(RWDigits Z, Digits V, RWDigits scratch);
#endif  // V8_ADVANCED_BIGINT_ALGORITHMS

  // {out_length} initially contains the allocated capacity of {out}, and
  // upon return will be set to the actual length of the result string.
  void ToString(char* out, uint32_t* out_length, Digits& X, int radix,
                bool sign);
  void ToStringImpl(char* out, uint32_t* out_length, Digits& X, int radix,
                    bool sign, bool use_fast_algorithm);

  void FromString(RWDigits Z, FromStringAccumulator* accumulator);
  void FromStringClassic(RWDigits Z, FromStringAccumulator* accumulator);
  void FromStringLarge(RWDigits Z, FromStringAccumulator* accumulator);
  void FromStringBasePowerOfTwo(RWDigits Z, FromStringAccumulator* accumulator);

  bool should_terminate() { return status_ == Status::kInterrupted; }

  // Each unit is supposed to represent approximately one CPU {mul} instruction.
  // Doesn't need to be accurate; we just want to make sure to check for
  // interrupt requests every now and then (roughly every 10-100 ms; often
  // enough not to appear stuck, rarely enough not to cause noticeable
  // overhead).
  static const uintptr_t kWorkEstimateThreshold = 5000000;

  void AddWorkEstimate(uintptr_t estimate) {
    work_estimate_ += estimate;
    if (work_estimate_ >= kWorkEstimateThreshold) {
      work_estimate_ = 0;
      if (platform_->InterruptRequested()) {
        status_ = Status::kInterrupted;
      }
    }
  }

 private:
  // Number of digits to keep around, to reduce the number of allocations.
  // Arbitrarily chosen; should be large enough to hold a few scratch areas
  // for commonly-occurring BigInt sizes.
  static constexpr int kSmallScratchSize = 100;

  RWDigits GetSmallScratch() {
    if (!small_scratch_) {
      small_scratch_.reset(new digit_t[kSmallScratchSize]);
    }
    return RWDigits(small_scratch_.get(), kSmallScratchSize);
  }

  uintptr_t work_estimate_{0};
  Status status_{Status::kOk};
  Platform* platform_;
  std::unique_ptr<digit_t[]> small_scratch_;
};

// Prevent computations of scratch space and number of bits from overflowing.
constexpr uint32_t kMaxNumDigits = UINT32_MAX / kDigitBits;

#define USE(var) ((void)var)

// RAII memory for a Digits array.
class Storage {
 public:
  explicit Storage(uint32_t count) : ptr_(new digit_t[count]) {}

  digit_t* get() { return ptr_.get(); }

 private:
  std::unique_ptr<digit_t[]> ptr_;
};

// A writable Digits array with attached storage.
class ScratchDigits : public RWDigits {
 public:
  explicit ScratchDigits(uint32_t len) : RWDigits(nullptr, len), storage_(len) {
    digits_ = storage_.get();
  }

 private:
  Storage storage_;
};

}  // namespace bigint
}  // namespace v8

#endif  // V8_BIGINT_BIGINT_INTERNAL_H_
