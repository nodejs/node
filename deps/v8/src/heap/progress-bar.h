// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_PROGRESS_BAR_H_
#define V8_HEAP_PROGRESS_BAR_H_

#include <atomic>
#include <cstdint>

#include "src/base/logging.h"

namespace v8 {
namespace internal {

// The progress bar allows for keeping track of the bytes processed of a single
// object. The progress bar itself must be enabled before it's used.
//
// Only large objects use the progress bar which is stored in their page header.
// These objects are scanned in increments and will be kept black while being
// scanned. Even if the mutator writes to them they will be kept black and a
// white to grey transition is performed in the value.
//
// The progress bar starts as disabled. After enabling (through `Enable()`), it
// can never be disabled again.
class ProgressBar final {
 public:
  ProgressBar() : value_(kDisabledSentinel) {}

  void Enable() { value_ = 0; }
  bool IsEnabled() const {
    return value_.load(std::memory_order_acquire) != kDisabledSentinel;
  }

  size_t Value() const {
    DCHECK(IsEnabled());
    return value_.load(std::memory_order_acquire);
  }

  bool TrySetNewValue(size_t old_value, size_t new_value) {
    DCHECK(IsEnabled());
    DCHECK_NE(kDisabledSentinel, new_value);
    return value_.compare_exchange_strong(old_value, new_value,
                                          std::memory_order_acq_rel);
  }

  void ResetIfEnabled() {
    if (IsEnabled()) {
      value_.store(0, std::memory_order_release);
    }
  }

 private:
  static constexpr size_t kDisabledSentinel = SIZE_MAX;

  std::atomic<size_t> value_;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_PROGRESS_BAR_H_
