// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_STRESS_SCAVENGE_OBSERVER_H_
#define V8_HEAP_STRESS_SCAVENGE_OBSERVER_H_

#include "src/heap/heap.h"

namespace v8 {
namespace internal {

class StressScavengeObserver : public AllocationObserver {
 public:
  explicit StressScavengeObserver(Heap* heap);

  void Step(int bytes_allocated, Address soon_object, size_t size) override;

  bool HasRequestedGC() const;
  void RequestedGCDone();

  // The maximum percent of the newspace capacity reached. This is tracked when
  // specyfing --fuzzer-gc-analysis.
  double MaxNewSpaceSizeReached() const;

 private:
  Heap* heap_;
  int limit_percentage_;
  bool has_requested_gc_;

  double max_new_space_size_reached_;

  int NextLimit(int min = 0);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_STRESS_SCAVENGE_OBSERVER_H_
