// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_TEST_CCTEST_TEST_TRANSITIONS_H_
#define V8_TEST_CCTEST_TEST_TRANSITIONS_H_

#include "src/transitions.h"

namespace v8 {
namespace internal {

class TestTransitionsAccessor : public TransitionsAccessor {
 public:
  TestTransitionsAccessor(Map* map, DisallowHeapAllocation* no_gc)
      : TransitionsAccessor(map, no_gc) {}
  explicit TestTransitionsAccessor(Handle<Map> map)
      : TransitionsAccessor(map) {}

  // Expose internals for tests.
  bool IsWeakCellEncoding() { return encoding() == kWeakCell; }

  bool IsFullTransitionArrayEncoding() {
    return encoding() == kFullTransitionArray;
  }
};

}  // namespace internal
}  // namespace v8

#endif  // V8_TEST_CCTEST_TEST_TRANSITIONS_H_
