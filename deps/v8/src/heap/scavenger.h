// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_SCAVENGER_H_
#define V8_HEAP_SCAVENGER_H_

#include "src/base/platform/condition-variable.h"
#include "src/heap/local-allocator.h"
#include "src/heap/objects-visiting.h"
#include "src/heap/slot-set.h"
#include "src/heap/worklist.h"

namespace v8 {
namespace internal {

class OneshotBarrier;

class Scavenger {
 public:
  static const int kCopiedListSegmentSize = 256;
  static const int kPromotionListSegmentSize = 256;

  using ObjectAndSize = std::pair<HeapObject*, int>;
  using CopiedList = Worklist<ObjectAndSize, kCopiedListSegmentSize>;
  using PromotionList = Worklist<ObjectAndSize, kPromotionListSegmentSize>;

  Scavenger(Heap* heap, bool is_logging, CopiedList* copied_list,
            PromotionList* promotion_list, int task_id);

  // Entry point for scavenging an old generation page. For scavenging single
  // objects see RootScavengingVisitor and ScavengeVisitor below.
  void ScavengePage(MemoryChunk* page);

  // Processes remaining work (=objects) after single objects have been
  // manually scavenged using ScavengeObject or CheckAndScavengeObject.
  void Process(OneshotBarrier* barrier = nullptr);

  // Finalize the Scavenger. Needs to be called from the main thread.
  void Finalize();

  size_t bytes_copied() const { return copied_size_; }
  size_t bytes_promoted() const { return promoted_size_; }

 private:
  // Number of objects to process before interrupting for potentially waking
  // up other tasks.
  static const int kInterruptThreshold = 128;
  static const int kInitialLocalPretenuringFeedbackCapacity = 256;

  inline Heap* heap() { return heap_; }

  inline void PageMemoryFence(Object* object);

  void AddPageToSweeperIfNecessary(MemoryChunk* page);

  // Potentially scavenges an object referenced from |slot_address| if it is
  // indeed a HeapObject and resides in from space.
  inline SlotCallbackResult CheckAndScavengeObject(Heap* heap,
                                                   Address slot_address);

  // Scavenges an object |object| referenced from slot |p|. |object| is required
  // to be in from space.
  inline void ScavengeObject(HeapObject** p, HeapObject* object);

  // Copies |source| to |target| and sets the forwarding pointer in |source|.
  V8_INLINE bool MigrateObject(Map* map, HeapObject* source, HeapObject* target,
                               int size);

  V8_INLINE bool SemiSpaceCopyObject(Map* map, HeapObject** slot,
                                     HeapObject* object, int object_size);

  V8_INLINE bool PromoteObject(Map* map, HeapObject** slot, HeapObject* object,
                               int object_size);

  V8_INLINE void EvacuateObject(HeapObject** slot, Map* map,
                                HeapObject* source);

  // Different cases for object evacuation.

  V8_INLINE void EvacuateObjectDefault(Map* map, HeapObject** slot,
                                       HeapObject* object, int object_size);

  V8_INLINE void EvacuateJSFunction(Map* map, HeapObject** slot,
                                    JSFunction* object, int object_size);

  inline void EvacuateThinString(Map* map, HeapObject** slot,
                                 ThinString* object, int object_size);

  inline void EvacuateShortcutCandidate(Map* map, HeapObject** slot,
                                        ConsString* object, int object_size);

  void IterateAndScavengePromotedObject(HeapObject* target, int size);

  void RecordCopiedObject(HeapObject* obj);

  static inline bool ContainsOnlyData(VisitorId visitor_id);

  Heap* const heap_;
  PromotionList::View promotion_list_;
  CopiedList::View copied_list_;
  Heap::PretenuringFeedbackMap local_pretenuring_feedback_;
  size_t copied_size_;
  size_t promoted_size_;
  LocalAllocator allocator_;
  const bool is_logging_;
  const bool is_incremental_marking_;
  const bool is_compacting_;

  friend class IterateAndScavengePromotedObjectsVisitor;
  friend class RootScavengeVisitor;
  friend class ScavengeVisitor;
};

// Helper class for turning the scavenger into an object visitor that is also
// filtering out non-HeapObjects and objects which do not reside in new space.
class RootScavengeVisitor final : public RootVisitor {
 public:
  RootScavengeVisitor(Heap* heap, Scavenger* scavenger)
      : heap_(heap), scavenger_(scavenger) {}

  void VisitRootPointer(Root root, Object** p) final;
  void VisitRootPointers(Root root, Object** start, Object** end) final;

 private:
  void ScavengePointer(Object** p);

  Heap* const heap_;
  Scavenger* const scavenger_;
};

class ScavengeVisitor final : public NewSpaceVisitor<ScavengeVisitor> {
 public:
  ScavengeVisitor(Heap* heap, Scavenger* scavenger)
      : heap_(heap), scavenger_(scavenger) {}

  V8_INLINE void VisitPointers(HeapObject* host, Object** start,
                               Object** end) final;

 private:
  Heap* const heap_;
  Scavenger* const scavenger_;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_SCAVENGER_H_
