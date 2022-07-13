// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/tasks/operations-barrier.h"

namespace v8 {
namespace internal {

OperationsBarrier::Token OperationsBarrier::TryLock() {
  base::MutexGuard guard(&mutex_);
  if (cancelled_) return {};
  ++operations_count_;
  return Token(this);
}

void OperationsBarrier::CancelAndWait() {
  base::MutexGuard guard(&mutex_);
  DCHECK(!cancelled_);
  cancelled_ = true;
  while (operations_count_ > 0) {
    release_condition_.Wait(&mutex_);
  }
}

void OperationsBarrier::Release() {
  base::MutexGuard guard(&mutex_);
  if (--operations_count_ == 0 && cancelled_) {
    release_condition_.NotifyOne();
  }
}

}  // namespace internal
}  // namespace v8
