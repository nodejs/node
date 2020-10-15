// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef INCLUDE_CPPGC_INTERNAL_PROCESS_HEAP_H_
#define INCLUDE_CPPGC_INTERNAL_PROCESS_HEAP_H_

#include "cppgc/internal/atomic-entry-flag.h"
#include "v8config.h"  // NOLINT(build/include_directory)

namespace cppgc {
namespace internal {

class V8_EXPORT ProcessHeap final {
 public:
  static void EnterIncrementalOrConcurrentMarking() {
    concurrent_marking_flag_.Enter();
  }
  static void ExitIncrementalOrConcurrentMarking() {
    concurrent_marking_flag_.Exit();
  }

  static bool IsAnyIncrementalOrConcurrentMarking() {
    return concurrent_marking_flag_.MightBeEntered();
  }

 private:
  static AtomicEntryFlag concurrent_marking_flag_;
};

}  // namespace internal
}  // namespace cppgc

#endif  // INCLUDE_CPPGC_INTERNAL_PROCESS_HEAP_H_
