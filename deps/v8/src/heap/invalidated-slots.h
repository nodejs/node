// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_INVALIDATED_SLOTS_H_
#define V8_HEAP_INVALIDATED_SLOTS_H_

#include <set>
#include <stack>

#include "src/base/atomic-utils.h"
#include "src/heap/memory-chunk-layout.h"
#include "src/objects/heap-object.h"
#include "src/utils/allocation.h"
#include "src/utils/utils.h"

namespace v8 {
namespace internal {

// This data structure stores objects that went through object layout change
// that potentially invalidates slots recorded concurrently. The second part
// of each element is the size of the corresponding object before the layout
// change.
using InvalidatedSlots = std::set<HeapObject, Object::Comparer>;

// This class provides IsValid predicate that takes into account the set
// of invalidated objects in the given memory chunk.
// The sequence of queried slot must be non-decreasing. This allows fast
// implementation with complexity O(m*log(m) + n), where
// m is the number of invalidated objects in the memory chunk.
// n is the number of IsValid queries.
class V8_EXPORT_PRIVATE InvalidatedSlotsFilter {
 public:
  static InvalidatedSlotsFilter OldToOld(MemoryChunk* chunk);
  static InvalidatedSlotsFilter OldToNew(MemoryChunk* chunk);
  static InvalidatedSlotsFilter OldToShared(MemoryChunk* chunk);

  inline bool IsValid(Address slot);

 private:
  explicit InvalidatedSlotsFilter(MemoryChunk* chunk,
                                  InvalidatedSlots* invalidated_slots,
                                  RememberedSetType remembered_set_type);

  InvalidatedSlots::const_iterator iterator_;
  InvalidatedSlots::const_iterator iterator_end_;
  Address sentinel_;
  Address invalidated_start_;
  Address next_invalidated_start_;
  int invalidated_size_;
  InvalidatedSlots empty_;
#ifdef DEBUG
  Address last_slot_;
  RememberedSetType remembered_set_type_;
#endif

 private:
  inline void NextInvalidatedObject();
};

class V8_EXPORT_PRIVATE InvalidatedSlotsCleanup {
 public:
  static InvalidatedSlotsCleanup OldToNew(MemoryChunk* chunk);
  static InvalidatedSlotsCleanup OldToShared(MemoryChunk* chunk);
  static InvalidatedSlotsCleanup NoCleanup(MemoryChunk* chunk);

  explicit InvalidatedSlotsCleanup(MemoryChunk* chunk,
                                   InvalidatedSlots* invalidated_slots);

  inline void Free(Address free_start, Address free_end);

 private:
  InvalidatedSlots::iterator iterator_;
  InvalidatedSlots::iterator iterator_end_;
  InvalidatedSlots* invalidated_slots_;
  InvalidatedSlots empty_;

  Address sentinel_;
  Address invalidated_start_;

  inline void NextInvalidatedObject();
#ifdef DEBUG
  Address last_free_;
#endif
};

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_INVALIDATED_SLOTS_H_
