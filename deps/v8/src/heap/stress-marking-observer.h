// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_STRESS_MARKING_OBSERVER_H_
#define V8_HEAP_STRESS_MARKING_OBSERVER_H_

#include "src/heap/heap.h"

namespace v8 {
namespace internal {

class StressMarkingObserver : public AllocationObserver {
 public:
  StressMarkingObserver();

  void Step(int bytes_allocated, Address soon_object, size_t size) override;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_STRESS_MARKING_OBSERVER_H_
