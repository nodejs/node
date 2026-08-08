// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/bigint/bigint-internal.h"

#include "src/bigint/bigint-inl.h"

namespace v8 {
namespace bigint {

// Used for checking consistency between library and public header.
#if DEBUG
#if V8_ADVANCED_BIGINT_ALGORITHMS
bool kAdvancedAlgorithmsEnabledInLibrary = true;
#else
bool kAdvancedAlgorithmsEnabledInLibrary = false;
#endif  // V8_ADVANCED_BIGINT_ALGORITHMS
#endif  // DEBUG

Status ProcessorImpl::get_and_clear_status() {
  Status result = status_;
  status_ = Status::kOk;
  return result;
}

void Processor::Destroy() { delete this; }

void ProcessorImpl::Multiply(RWDigits Z, Digits X, Digits Y) {
  X.Normalize();
  Y.Normalize();
  if (X.len() == 0 || Y.len() == 0) return Z.Clear();
  if (X.len() < Y.len()) std::swap(X, Y);
  if (Y.len() == 1) {
    MultiplySingle(Z, X, Y[0]);
    AddWorkEstimate(X.len());
    return;
  }
  if (Y.len() < config::kKaratsubaThreshold) {
    MultiplySchoolbook(Z, X, Y);
    AddWorkEstimate(X.len() * Y.len());
    return;
  }
  return MultiplyLarge(Z, X, Y);
}

void ProcessorImpl::MultiplyLarge(RWDigits& Z, Digits& X, Digits& Y) {
  DCHECK(IsDigitNormalized(X));
  DCHECK(IsDigitNormalized(Y));
  DCHECK(X.len() >= Y.len());
  CHECK(X.len() <= kMaxNumDigits);
#if !V8_ADVANCED_BIGINT_ALGORITHMS
  return MultiplyKaratsuba(Z, X, Y);
#else
  if (Y.len() < config::kToomThreshold) return MultiplyKaratsuba(Z, X, Y);
  if (Y.len() < config::kFftThreshold) return MultiplyToomCook(Z, X, Y);
  return MultiplyFFT(Z, X, Y);
#endif
}

Status Processor::MultiplyLarge(RWDigits& Z, Digits& X, Digits& Y) {
  ProcessorImpl* impl = static_cast<ProcessorImpl*>(this);
  impl->MultiplyLarge(Z, X, Y);
  return impl->get_and_clear_status();
}

Status Processor::DivideLarge(RWDigits& Q, Digits& A, Digits& B) {
  ProcessorImpl* impl = static_cast<ProcessorImpl*>(this);
  DCHECK(IsDigitNormalized(A));
  DCHECK(IsDigitNormalized(B));
  DCHECK(B.len() > 1);
  RWDigits R0(nullptr, 0);
  if (B.len() < config::kBurnikelThreshold) {
    impl->DivideSchoolbook(Q, R0, A, B);
    return impl->get_and_clear_status();
  }
  CHECK(A.len() <= kMaxNumDigits);
#if !V8_ADVANCED_BIGINT_ALGORITHMS
  impl->DivideBurnikelZiegler(Q, R0, A, B);
#else
  if (B.len() < config::kBarrettThreshold || A.len() == B.len()) {
    impl->DivideBurnikelZiegler(Q, R0, A, B);
  } else {
    ScratchDigits R(B.len(), platform());
    impl->DivideBarrett(Q, R, A, B);
  }
#endif
  return impl->get_and_clear_status();
}

Status Processor::ModuloLarge(RWDigits& R, Digits& A, Digits& B) {
  ProcessorImpl* impl = static_cast<ProcessorImpl*>(this);
  DCHECK(IsDigitNormalized(A));
  DCHECK(IsDigitNormalized(B));
  DCHECK(B.len() > 1);
  if (B.len() < config::kBurnikelThreshold) {
    RWDigits Q(nullptr, 0);
    impl->DivideSchoolbook(Q, R, A, B);
    return impl->get_and_clear_status();
  }
  CHECK(A.len() < kMaxNumDigits);
  uint32_t q_len = DivideResultLength(A, B);
  ScratchDigits Q(q_len, platform());
#if !V8_ADVANCED_BIGINT_ALGORITHMS
  impl->DivideBurnikelZiegler(Q, R, A, B);
#else
  if (B.len() < config::kBarrettThreshold || A.len() == B.len()) {
    impl->DivideBurnikelZiegler(Q, R, A, B);
  } else {
    impl->DivideBarrett(Q, R, A, B);
  }
#endif
  return impl->get_and_clear_status();
}

RWDigits& Processor::AllocateCachedInverseFor(Digits& divisor) {
  uint32_t divisor_len = divisor.len();
  uint32_t inverse_len = divisor_len + 1;
  uint32_t len = divisor_len + inverse_len;
  if (len > cached_inverse_allocated_length_) {
    cached_inverse_storage_.reset(platform_->Allocate(len));
    cached_inverse_allocated_length_ = len;
  }
  cached_divisor_ = RWDigits(cached_inverse_storage_.get(), divisor_len);
  for (uint32_t i = 0; i < divisor_len; i++) cached_divisor_[i] = divisor[i];
  cached_inverse_ =
      RWDigits(cached_inverse_storage_.get() + divisor_len, inverse_len);
  return cached_inverse_;
}

void ProcessorImpl::CachedMod_MakeInverse(Digits& B) {
  uint32_t n = B.len();
  RWDigits& Inv = AllocateCachedInverseFor(B);
  // Use the cached copy of B now, just to prevent any confusion arising
  // from concurrent mutation.
  Digits& cached_B = GetCachedDivisor();
  // We can't use {GetSmallScratch()} here, because {DivideSchoolbook} already
  // does that (usually). It's fine because we don't expect to call this
  // function very often.
  ScratchDigits A(n * 2, platform());
  // The idea is to set A = 1 << 2*n, and then compute Inv = A / B. However,
  // having that lone "1" bit in A's top digit is inefficient and far-reaching:
  // it requires us to make Inv bigger too. So we use a trick: first we
  // set A = (1 << 2*n) - (B << n), which eliminates the top digit. Then
  // after the division we undo the subtraction by adding back 1 << n.
  // We also approximate the subtraction by using bit negation instead, which
  // avoids the need for propagating borrows and is almost the same in two's
  // complement: -x == ~x + 1. We model the +1 by setting the lower part of
  // A to all 1-bits (effectively: -x ≈ ~x + 0.99999...).
  uint32_t i = 0;
  for (; i < n; i++) A[i] = ~digit_t{0};
  for (; i < A.len(); i++) A[i] = ~cached_B[i - n];
  RWDigits R(nullptr, 0);
  DivideSchoolbook(Inv, R, A, cached_B);
  Add((Inv + n), 1);

  // If we add 1 here, we increase our chances of getting lucky in the
  // corrective loop in {CachedDiv}. But don't do it when there's a risk
  // of overflowing {inv}.
  if (Inv[0] != ~digit_t{0} || Inv.msd() != ~digit_t{0}) Add(Inv, 1);

  // {CachedMod} relies on the "small scratch" having been allocated, so
  // make sure that has happened regardless of {DivideSchoolbook}'s internal
  // decisions.
  GetSmallScratch();
}

void Processor::CachedMod_MakeInverse(Digits& B) {
  return static_cast<ProcessorImpl*>(this)->CachedMod_MakeInverse(B);
}

}  // namespace bigint
}  // namespace v8
