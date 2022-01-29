// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMMON_HIGH_ALLOCATION_THROUGHPUT_SCOPE_H_
#define V8_COMMON_HIGH_ALLOCATION_THROUGHPUT_SCOPE_H_

#include "include/v8-platform.h"

namespace v8 {
namespace internal {

/**
 * Scope that notifies embedder's observer about entering sections with high
 * throughput of malloc/free operations.
 */
class HighAllocationThroughputScope final {
 public:
  explicit HighAllocationThroughputScope(Platform* platform)
      : observer_(platform->GetHighAllocationThroughputObserver()) {
    observer_->LeaveSection();
  }

  HighAllocationThroughputScope(const HighAllocationThroughputScope&) = delete;
  HighAllocationThroughputScope& operator=(
      const HighAllocationThroughputScope&) = delete;

  ~HighAllocationThroughputScope() { observer_->EnterSection(); }

 private:
  HighAllocationThroughputObserver* observer_;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_COMMON_HIGH_ALLOCATION_THROUGHPUT_SCOPE_H_
