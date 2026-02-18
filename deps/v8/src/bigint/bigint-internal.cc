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

ProcessorImpl::ProcessorImpl(Platform* platform) : platform_(platform) {}

ProcessorImpl::~ProcessorImpl() { delete platform_; }

Status ProcessorImpl::get_and_clear_status() {
  Status result = status_;
  status_ = Status::kOk;
  return result;
}

Processor* Processor::New(Platform* platform) {
  ProcessorImpl* impl = new ProcessorImpl(platform);
  return static_cast<Processor*>(impl);
}

void Processor::Destroy() { delete static_cast<ProcessorImpl*>(this); }

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
    ScratchDigits R(B.len());
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
  ScratchDigits Q(q_len);
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

}  // namespace bigint
}  // namespace v8
