// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_CPPGC_WRITE_BARRIER_H_
#define V8_HEAP_CPPGC_WRITE_BARRIER_H_

#include "include/cppgc/internal/write-barrier.h"

namespace cppgc {
namespace internal {

class WriteBarrier::IncrementalOrConcurrentMarkingFlagUpdater {
 public:
  static void Enter() { incremental_or_concurrent_marking_flag_.Enter(); }
  static void Exit() { incremental_or_concurrent_marking_flag_.Exit(); }
};

}  // namespace internal
}  // namespace cppgc

#endif  // V8_HEAP_CPPGC_WRITE_BARRIER_H_
