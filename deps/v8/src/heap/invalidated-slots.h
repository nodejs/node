// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_INVALIDATED_SLOTS_H
#define V8_INVALIDATED_SLOTS_H

#include <map>
#include <stack>

#include "src/allocation.h"
#include "src/base/atomic-utils.h"
#include "src/utils.h"

namespace v8 {
namespace internal {

class HeapObject;

// This data structure stores objects that went through object layout change
// that potentially invalidates slots recorded concurrently. The second part
// of each element is the size of the corresponding object before the layout
// change.
using InvalidatedSlots = std::map<HeapObject*, int>;

// This class provides IsValid predicate that takes into account the set
// of invalidated objects in the given memory chunk.
// The sequence of queried slot must be non-decreasing. This allows fast
// implementation with complexity O(m*log(m) + n), where
// m is the number of invalidated objects in the memory chunk.
// n is the number of IsValid queries.
class InvalidatedSlotsFilter {
 public:
  explicit InvalidatedSlotsFilter(MemoryChunk* chunk);
  inline bool IsValid(Address slot);

 private:
  InvalidatedSlots::const_iterator iterator_;
  InvalidatedSlots::const_iterator iterator_end_;
  Address sentinel_;
  Address invalidated_start_;
  Address invalidated_end_;
  HeapObject* invalidated_object_;
  int invalidated_object_size_;
  InvalidatedSlots empty_;
#ifdef DEBUG
  Address last_slot_;
#endif
};

}  // namespace internal
}  // namespace v8

#endif  // V8_INVALIDATED_SLOTS_H
