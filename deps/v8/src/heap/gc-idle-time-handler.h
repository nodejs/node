// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_GC_IDLE_TIME_HANDLER_H_
#define V8_HEAP_GC_IDLE_TIME_HANDLER_H_

#include "src/common/globals.h"

namespace v8 {
namespace internal {

enum class GCIdleTimeAction : uint8_t {
  kDone,
  kIncrementalStep,
};

class GCIdleTimeHeapState {
 public:
  void Print();

  size_t size_of_objects;
  bool incremental_marking_stopped;
};


// The idle time handler makes decisions about which garbage collection
// operations are executing during IdleNotification.
class V8_EXPORT_PRIVATE GCIdleTimeHandler {
 public:
  GCIdleTimeHandler() = default;
  GCIdleTimeHandler(const GCIdleTimeHandler&) = delete;
  GCIdleTimeHandler& operator=(const GCIdleTimeHandler&) = delete;

  GCIdleTimeAction Compute(double idle_time_in_ms,
                           GCIdleTimeHeapState heap_state);

  bool Enabled();

  static double EstimateFinalIncrementalMarkCompactTime(
      size_t size_of_objects, double mark_compact_speed_in_bytes_per_ms);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_GC_IDLE_TIME_HANDLER_H_
