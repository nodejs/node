// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_PARALLEL_WORK_ITEM_H_
#define V8_HEAP_PARALLEL_WORK_ITEM_H_

#include <atomic>

namespace v8 {
namespace internal {

class ParallelWorkItem {
 public:
  ParallelWorkItem() = default;

  bool TryAcquire() {
    // memory_order_relaxed is sufficient as the work item's state itself hasn't
    // been modified since the beginning of its associated job. This is only
    // atomically acquiring the right to work on it.
    return reinterpret_cast<std::atomic<bool>*>(&acquire_)->exchange(
               true, std::memory_order_relaxed) == false;
  }

  bool IsAcquired() const {
    return reinterpret_cast<const std::atomic<bool>*>(&acquire_)->load(
        std::memory_order_relaxed);
  }

 private:
  bool acquire_{false};
};

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_PARALLEL_WORK_ITEM_H_
