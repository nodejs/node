// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_SCAVENGER_H_
#define V8_HEAP_SCAVENGER_H_

#include "src/heap/objects-visiting.h"
#include "src/heap/slot-set.h"

namespace v8 {
namespace internal {

typedef void (*ScavengingCallback)(Map* map, HeapObject** slot,
                                   HeapObject* object);

class Scavenger {
 public:
  explicit Scavenger(Heap* heap) : heap_(heap) {}

  // Initializes static visitor dispatch tables.
  static void Initialize();

  // Callback function passed to Heap::Iterate etc.  Copies an object if
  // necessary, the object might be promoted to an old space.  The caller must
  // ensure the precondition that the object is (a) a heap object and (b) in
  // the heap's from space.
  static inline void ScavengeObject(HeapObject** p, HeapObject* object);
  static inline SlotCallbackResult CheckAndScavengeObject(Heap* heap,
                                                          Address slot_address);

  // Slow part of {ScavengeObject} above.
  static void ScavengeObjectSlow(HeapObject** p, HeapObject* object);

  // Chooses an appropriate static visitor table depending on the current state
  // of the heap (i.e. incremental marking, logging and profiling).
  void SelectScavengingVisitorsTable();

  Isolate* isolate();
  Heap* heap() { return heap_; }

 private:
  Heap* heap_;
  VisitorDispatchTable<ScavengingCallback> scavenging_visitors_table_;
};


// Helper class for turning the scavenger into an object visitor that is also
// filtering out non-HeapObjects and objects which do not reside in new space.
class ScavengeVisitor : public ObjectVisitor {
 public:
  explicit ScavengeVisitor(Heap* heap) : heap_(heap) {}

  void VisitPointer(Object** p) override;
  void VisitPointers(Object** start, Object** end) override;

 private:
  inline void ScavengePointer(Object** p);

  Heap* heap_;
};


// Helper class for turning the scavenger into an object visitor that is also
// filtering out non-HeapObjects and objects which do not reside in new space.
template <PromotionMode promotion_mode>
class StaticScavengeVisitor
    : public StaticNewSpaceVisitor<StaticScavengeVisitor<promotion_mode>> {
 public:
  static inline void VisitPointer(Heap* heap, HeapObject* object, Object** p);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_SCAVENGER_H_
