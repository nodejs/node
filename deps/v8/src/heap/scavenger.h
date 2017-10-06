// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_SCAVENGER_H_
#define V8_HEAP_SCAVENGER_H_

#include "src/heap/local-allocator.h"
#include "src/heap/objects-visiting.h"
#include "src/heap/slot-set.h"
#include "src/heap/worklist.h"

namespace v8 {
namespace internal {

static const int kCopiedListSegmentSize = 64;
static const int kPromotionListSegmentSize = 64;

using AddressRange = std::pair<Address, Address>;
using CopiedList = Worklist<AddressRange, kCopiedListSegmentSize>;
using ObjectAndSize = std::pair<HeapObject*, int>;
using PromotionList = Worklist<ObjectAndSize, kPromotionListSegmentSize>;

// A list of copied ranges. Keeps the last consecutive range local and announces
// all other ranges to a global work list.
class CopiedRangesList {
 public:
  CopiedRangesList(CopiedList* copied_list, int task_id)
      : current_start_(nullptr),
        current_end_(nullptr),
        copied_list_(copied_list, task_id) {}

  ~CopiedRangesList() {
    CHECK_NULL(current_start_);
    CHECK_NULL(current_end_);
  }

  void Insert(HeapObject* object, int size) {
    const Address object_address = object->address();
    if (current_end_ != object_address) {
      if (current_start_ != nullptr) {
        copied_list_.Push(AddressRange(current_start_, current_end_));
      }
      current_start_ = object_address;
      current_end_ = current_start_ + size;
      return;
    }
    DCHECK_EQ(current_end_, object_address);
    current_end_ += size;
    return;
  }

  bool Pop(AddressRange* entry) {
    if (copied_list_.Pop(entry)) {
      return true;
    } else if (current_start_ != nullptr) {
      *entry = AddressRange(current_start_, current_end_);
      current_start_ = current_end_ = nullptr;
      return true;
    }
    return false;
  }

 private:
  Address current_start_;
  Address current_end_;
  CopiedList::View copied_list_;
};

class Scavenger {
 public:
  Scavenger(Heap* heap, bool is_logging, bool is_incremental_marking,
            CopiedList* copied_list, PromotionList* promotion_list, int task_id)
      : heap_(heap),
        promotion_list_(promotion_list, task_id),
        copied_list_(copied_list, task_id),
        local_pretenuring_feedback_(kInitialLocalPretenuringFeedbackCapacity),
        copied_size_(0),
        promoted_size_(0),
        allocator_(heap),
        is_logging_(is_logging),
        is_incremental_marking_(is_incremental_marking) {}

  // Scavenges an object |object| referenced from slot |p|. |object| is required
  // to be in from space.
  inline void ScavengeObject(HeapObject** p, HeapObject* object);

  // Potentially scavenges an object referenced from |slot_address| if it is
  // indeed a HeapObject and resides in from space.
  inline SlotCallbackResult CheckAndScavengeObject(Heap* heap,
                                                   Address slot_address);

  // Processes remaining work (=objects) after single objects have been
  // manually scavenged using ScavengeObject or CheckAndScavengeObject.
  void Process();

  // Finalize the Scavenger. Needs to be called from the main thread.
  void Finalize();

 private:
  static const int kInitialLocalPretenuringFeedbackCapacity = 256;

  inline Heap* heap() { return heap_; }

  // Copies |source| to |target| and sets the forwarding pointer in |source|.
  V8_INLINE void MigrateObject(Map* map, HeapObject* source, HeapObject* target,
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

  Heap* const heap_;
  PromotionList::View promotion_list_;
  CopiedRangesList copied_list_;
  base::HashMap local_pretenuring_feedback_;
  size_t copied_size_;
  size_t promoted_size_;
  LocalAllocator allocator_;
  bool is_logging_;
  bool is_incremental_marking_;
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
