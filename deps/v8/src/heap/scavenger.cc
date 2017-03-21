// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/scavenger.h"

#include "src/contexts.h"
#include "src/heap/heap.h"
#include "src/heap/objects-visiting-inl.h"
#include "src/heap/scavenger-inl.h"
#include "src/isolate.h"
#include "src/log.h"

namespace v8 {
namespace internal {

enum LoggingAndProfiling {
  LOGGING_AND_PROFILING_ENABLED,
  LOGGING_AND_PROFILING_DISABLED
};


enum MarksHandling { TRANSFER_MARKS, IGNORE_MARKS };

template <MarksHandling marks_handling,
          LoggingAndProfiling logging_and_profiling_mode>
class ScavengingVisitor : public StaticVisitorBase {
 public:
  static void Initialize() {
    table_.Register(kVisitSeqOneByteString, &EvacuateSeqOneByteString);
    table_.Register(kVisitSeqTwoByteString, &EvacuateSeqTwoByteString);
    table_.Register(kVisitShortcutCandidate, &EvacuateShortcutCandidate);
    table_.Register(kVisitByteArray, &EvacuateByteArray);
    table_.Register(kVisitFixedArray, &EvacuateFixedArray);
    table_.Register(kVisitFixedDoubleArray, &EvacuateFixedDoubleArray);
    table_.Register(kVisitFixedTypedArray, &EvacuateFixedTypedArray);
    table_.Register(kVisitFixedFloat64Array, &EvacuateFixedFloat64Array);
    table_.Register(kVisitJSArrayBuffer,
                    &ObjectEvacuationStrategy<POINTER_OBJECT>::Visit);

    table_.Register(
        kVisitNativeContext,
        &ObjectEvacuationStrategy<POINTER_OBJECT>::template VisitSpecialized<
            Context::kSize>);

    table_.Register(
        kVisitConsString,
        &ObjectEvacuationStrategy<POINTER_OBJECT>::template VisitSpecialized<
            ConsString::kSize>);

    table_.Register(
        kVisitSlicedString,
        &ObjectEvacuationStrategy<POINTER_OBJECT>::template VisitSpecialized<
            SlicedString::kSize>);

    table_.Register(
        kVisitSymbol,
        &ObjectEvacuationStrategy<POINTER_OBJECT>::template VisitSpecialized<
            Symbol::kSize>);

    table_.Register(
        kVisitSharedFunctionInfo,
        &ObjectEvacuationStrategy<POINTER_OBJECT>::template VisitSpecialized<
            SharedFunctionInfo::kSize>);

    table_.Register(kVisitJSWeakCollection,
                    &ObjectEvacuationStrategy<POINTER_OBJECT>::Visit);

    table_.Register(kVisitJSRegExp,
                    &ObjectEvacuationStrategy<POINTER_OBJECT>::Visit);

    table_.Register(kVisitJSFunction, &EvacuateJSFunction);

    table_.RegisterSpecializations<ObjectEvacuationStrategy<DATA_OBJECT>,
                                   kVisitDataObject, kVisitDataObjectGeneric>();

    table_.RegisterSpecializations<ObjectEvacuationStrategy<POINTER_OBJECT>,
                                   kVisitJSObject, kVisitJSObjectGeneric>();

    table_
        .RegisterSpecializations<ObjectEvacuationStrategy<POINTER_OBJECT>,
                                 kVisitJSApiObject, kVisitJSApiObjectGeneric>();

    table_.RegisterSpecializations<ObjectEvacuationStrategy<POINTER_OBJECT>,
                                   kVisitStruct, kVisitStructGeneric>();
  }

  static VisitorDispatchTable<ScavengingCallback>* GetTable() {
    return &table_;
  }

 private:
  enum ObjectContents { DATA_OBJECT, POINTER_OBJECT };

  static void RecordCopiedObject(Heap* heap, HeapObject* obj) {
    bool should_record = false;
#ifdef DEBUG
    should_record = FLAG_heap_stats;
#endif
    should_record = should_record || FLAG_log_gc;
    if (should_record) {
      if (heap->new_space()->Contains(obj)) {
        heap->new_space()->RecordAllocation(obj);
      } else {
        heap->new_space()->RecordPromotion(obj);
      }
    }
  }

  // Helper function used by CopyObject to copy a source object to an
  // allocated target object and update the forwarding pointer in the source
  // object.  Returns the target object.
  INLINE(static void MigrateObject(Heap* heap, HeapObject* source,
                                   HeapObject* target, int size)) {
    // If we migrate into to-space, then the to-space top pointer should be
    // right after the target object. Incorporate double alignment
    // over-allocation.
    DCHECK(!heap->InToSpace(target) ||
           target->address() + size == heap->new_space()->top() ||
           target->address() + size + kPointerSize == heap->new_space()->top());

    // Make sure that we do not overwrite the promotion queue which is at
    // the end of to-space.
    DCHECK(!heap->InToSpace(target) ||
           heap->promotion_queue()->IsBelowPromotionQueue(
               heap->new_space()->top()));

    // Copy the content of source to target.
    heap->CopyBlock(target->address(), source->address(), size);

    // Set the forwarding address.
    source->set_map_word(MapWord::FromForwardingAddress(target));

    if (logging_and_profiling_mode == LOGGING_AND_PROFILING_ENABLED) {
      // Update NewSpace stats if necessary.
      RecordCopiedObject(heap, target);
      heap->OnMoveEvent(target, source, size);
    }

    if (marks_handling == TRANSFER_MARKS) {
      if (IncrementalMarking::TransferColor(source, target, size)) {
        MemoryChunk::IncrementLiveBytes(target, size);
      }
    }
  }

  template <AllocationAlignment alignment>
  static inline bool SemiSpaceCopyObject(Map* map, HeapObject** slot,
                                         HeapObject* object, int object_size) {
    Heap* heap = map->GetHeap();

    DCHECK(heap->AllowedToBeMigrated(object, NEW_SPACE));
    AllocationResult allocation =
        heap->new_space()->AllocateRaw(object_size, alignment);

    HeapObject* target = NULL;  // Initialization to please compiler.
    if (allocation.To(&target)) {
      // Order is important here: Set the promotion limit before storing a
      // filler for double alignment or migrating the object. Otherwise we
      // may end up overwriting promotion queue entries when we migrate the
      // object.
      heap->promotion_queue()->SetNewLimit(heap->new_space()->top());

      MigrateObject(heap, object, target, object_size);

      // Update slot to new target.
      *slot = target;

      heap->IncrementSemiSpaceCopiedObjectSize(object_size);
      return true;
    }
    return false;
  }


  template <ObjectContents object_contents, AllocationAlignment alignment>
  static inline bool PromoteObject(Map* map, HeapObject** slot,
                                   HeapObject* object, int object_size) {
    Heap* heap = map->GetHeap();

    AllocationResult allocation =
        heap->old_space()->AllocateRaw(object_size, alignment);

    HeapObject* target = NULL;  // Initialization to please compiler.
    if (allocation.To(&target)) {
      MigrateObject(heap, object, target, object_size);

      // Update slot to new target using CAS. A concurrent sweeper thread my
      // filter the slot concurrently.
      HeapObject* old = *slot;
      base::Release_CompareAndSwap(reinterpret_cast<base::AtomicWord*>(slot),
                                   reinterpret_cast<base::AtomicWord>(old),
                                   reinterpret_cast<base::AtomicWord>(target));

      if (object_contents == POINTER_OBJECT) {
        heap->promotion_queue()->insert(
            target, object_size,
            Marking::IsBlack(ObjectMarking::MarkBitFrom(object)));
      }
      heap->IncrementPromotedObjectsSize(object_size);
      return true;
    }
    return false;
  }

  template <ObjectContents object_contents, AllocationAlignment alignment>
  static inline void EvacuateObject(Map* map, HeapObject** slot,
                                    HeapObject* object, int object_size) {
    SLOW_DCHECK(object_size <= Page::kAllocatableMemory);
    SLOW_DCHECK(object->Size() == object_size);
    Heap* heap = map->GetHeap();

    if (!heap->ShouldBePromoted(object->address(), object_size)) {
      // A semi-space copy may fail due to fragmentation. In that case, we
      // try to promote the object.
      if (SemiSpaceCopyObject<alignment>(map, slot, object, object_size)) {
        return;
      }
    }

    if (PromoteObject<object_contents, alignment>(map, slot, object,
                                                  object_size)) {
      return;
    }

    // If promotion failed, we try to copy the object to the other semi-space
    if (SemiSpaceCopyObject<alignment>(map, slot, object, object_size)) return;

    FatalProcessOutOfMemory("Scavenger: semi-space copy\n");
  }

  static inline void EvacuateJSFunction(Map* map, HeapObject** slot,
                                        HeapObject* object) {
    ObjectEvacuationStrategy<POINTER_OBJECT>::Visit(map, slot, object);

    if (marks_handling == IGNORE_MARKS) return;

    MapWord map_word = object->map_word();
    DCHECK(map_word.IsForwardingAddress());
    HeapObject* target = map_word.ToForwardingAddress();

    MarkBit mark_bit = ObjectMarking::MarkBitFrom(target);
    if (Marking::IsBlack(mark_bit)) {
      // This object is black and it might not be rescanned by marker.
      // We should explicitly record code entry slot for compaction because
      // promotion queue processing (IteratePromotedObjectPointers) will
      // miss it as it is not HeapObject-tagged.
      Address code_entry_slot =
          target->address() + JSFunction::kCodeEntryOffset;
      Code* code = Code::cast(Code::GetObjectFromEntryAddress(code_entry_slot));
      map->GetHeap()->mark_compact_collector()->RecordCodeEntrySlot(
          target, code_entry_slot, code);
    }
  }

  static inline void EvacuateFixedArray(Map* map, HeapObject** slot,
                                        HeapObject* object) {
    int length = reinterpret_cast<FixedArray*>(object)->synchronized_length();
    int object_size = FixedArray::SizeFor(length);
    EvacuateObject<POINTER_OBJECT, kWordAligned>(map, slot, object,
                                                 object_size);
  }

  static inline void EvacuateFixedDoubleArray(Map* map, HeapObject** slot,
                                              HeapObject* object) {
    int length = reinterpret_cast<FixedDoubleArray*>(object)->length();
    int object_size = FixedDoubleArray::SizeFor(length);
    EvacuateObject<DATA_OBJECT, kDoubleAligned>(map, slot, object, object_size);
  }

  static inline void EvacuateFixedTypedArray(Map* map, HeapObject** slot,
                                             HeapObject* object) {
    int object_size = reinterpret_cast<FixedTypedArrayBase*>(object)->size();
    EvacuateObject<POINTER_OBJECT, kWordAligned>(map, slot, object,
                                                 object_size);
  }

  static inline void EvacuateFixedFloat64Array(Map* map, HeapObject** slot,
                                               HeapObject* object) {
    int object_size = reinterpret_cast<FixedFloat64Array*>(object)->size();
    EvacuateObject<POINTER_OBJECT, kDoubleAligned>(map, slot, object,
                                                   object_size);
  }

  static inline void EvacuateByteArray(Map* map, HeapObject** slot,
                                       HeapObject* object) {
    int object_size = reinterpret_cast<ByteArray*>(object)->ByteArraySize();
    EvacuateObject<DATA_OBJECT, kWordAligned>(map, slot, object, object_size);
  }

  static inline void EvacuateSeqOneByteString(Map* map, HeapObject** slot,
                                              HeapObject* object) {
    int object_size = SeqOneByteString::cast(object)
                          ->SeqOneByteStringSize(map->instance_type());
    EvacuateObject<DATA_OBJECT, kWordAligned>(map, slot, object, object_size);
  }

  static inline void EvacuateSeqTwoByteString(Map* map, HeapObject** slot,
                                              HeapObject* object) {
    int object_size = SeqTwoByteString::cast(object)
                          ->SeqTwoByteStringSize(map->instance_type());
    EvacuateObject<DATA_OBJECT, kWordAligned>(map, slot, object, object_size);
  }

  static inline void EvacuateShortcutCandidate(Map* map, HeapObject** slot,
                                               HeapObject* object) {
    DCHECK(IsShortcutCandidate(map->instance_type()));

    Heap* heap = map->GetHeap();

    if (marks_handling == IGNORE_MARKS &&
        ConsString::cast(object)->unchecked_second() == heap->empty_string()) {
      HeapObject* first =
          HeapObject::cast(ConsString::cast(object)->unchecked_first());

      *slot = first;

      if (!heap->InNewSpace(first)) {
        object->set_map_word(MapWord::FromForwardingAddress(first));
        return;
      }

      MapWord first_word = first->map_word();
      if (first_word.IsForwardingAddress()) {
        HeapObject* target = first_word.ToForwardingAddress();

        *slot = target;
        object->set_map_word(MapWord::FromForwardingAddress(target));
        return;
      }

      Scavenger::ScavengeObjectSlow(slot, first);
      object->set_map_word(MapWord::FromForwardingAddress(*slot));
      return;
    }

    int object_size = ConsString::kSize;
    EvacuateObject<POINTER_OBJECT, kWordAligned>(map, slot, object,
                                                 object_size);
  }

  template <ObjectContents object_contents>
  class ObjectEvacuationStrategy {
   public:
    template <int object_size>
    static inline void VisitSpecialized(Map* map, HeapObject** slot,
                                        HeapObject* object) {
      EvacuateObject<object_contents, kWordAligned>(map, slot, object,
                                                    object_size);
    }

    static inline void Visit(Map* map, HeapObject** slot, HeapObject* object) {
      int object_size = map->instance_size();
      EvacuateObject<object_contents, kWordAligned>(map, slot, object,
                                                    object_size);
    }
  };

  static VisitorDispatchTable<ScavengingCallback> table_;
};

template <MarksHandling marks_handling,
          LoggingAndProfiling logging_and_profiling_mode>
VisitorDispatchTable<ScavengingCallback>
    ScavengingVisitor<marks_handling, logging_and_profiling_mode>::table_;

// static
void Scavenger::Initialize() {
  ScavengingVisitor<TRANSFER_MARKS,
                    LOGGING_AND_PROFILING_DISABLED>::Initialize();
  ScavengingVisitor<IGNORE_MARKS, LOGGING_AND_PROFILING_DISABLED>::Initialize();
  ScavengingVisitor<TRANSFER_MARKS,
                    LOGGING_AND_PROFILING_ENABLED>::Initialize();
  ScavengingVisitor<IGNORE_MARKS, LOGGING_AND_PROFILING_ENABLED>::Initialize();
}


// static
void Scavenger::ScavengeObjectSlow(HeapObject** p, HeapObject* object) {
  SLOW_DCHECK(object->GetIsolate()->heap()->InFromSpace(object));
  MapWord first_word = object->map_word();
  SLOW_DCHECK(!first_word.IsForwardingAddress());
  Map* map = first_word.ToMap();
  Scavenger* scavenger = map->GetHeap()->scavenge_collector_;
  scavenger->scavenging_visitors_table_.GetVisitor(map)(map, p, object);
}


void Scavenger::SelectScavengingVisitorsTable() {
  bool logging_and_profiling =
      FLAG_verify_predictable || isolate()->logger()->is_logging() ||
      isolate()->is_profiling() ||
      (isolate()->heap_profiler() != NULL &&
       isolate()->heap_profiler()->is_tracking_object_moves());

  if (!heap()->incremental_marking()->IsMarking()) {
    if (!logging_and_profiling) {
      scavenging_visitors_table_.CopyFrom(
          ScavengingVisitor<IGNORE_MARKS,
                            LOGGING_AND_PROFILING_DISABLED>::GetTable());
    } else {
      scavenging_visitors_table_.CopyFrom(
          ScavengingVisitor<IGNORE_MARKS,
                            LOGGING_AND_PROFILING_ENABLED>::GetTable());
    }
  } else {
    if (!logging_and_profiling) {
      scavenging_visitors_table_.CopyFrom(
          ScavengingVisitor<TRANSFER_MARKS,
                            LOGGING_AND_PROFILING_DISABLED>::GetTable());
    } else {
      scavenging_visitors_table_.CopyFrom(
          ScavengingVisitor<TRANSFER_MARKS,
                            LOGGING_AND_PROFILING_ENABLED>::GetTable());
    }

    if (heap()->incremental_marking()->IsCompacting()) {
      // When compacting forbid short-circuiting of cons-strings.
      // Scavenging code relies on the fact that new space object
      // can't be evacuated into evacuation candidate but
      // short-circuiting violates this assumption.
      scavenging_visitors_table_.Register(
          StaticVisitorBase::kVisitShortcutCandidate,
          scavenging_visitors_table_.GetVisitorById(
              StaticVisitorBase::kVisitConsString));
    }
  }
}


Isolate* Scavenger::isolate() { return heap()->isolate(); }


void ScavengeVisitor::VisitPointer(Object** p) { ScavengePointer(p); }


void ScavengeVisitor::VisitPointers(Object** start, Object** end) {
  // Copy all HeapObject pointers in [start, end)
  for (Object** p = start; p < end; p++) ScavengePointer(p);
}


void ScavengeVisitor::ScavengePointer(Object** p) {
  Object* object = *p;
  if (!heap_->InNewSpace(object)) return;

  Scavenger::ScavengeObject(reinterpret_cast<HeapObject**>(p),
                            reinterpret_cast<HeapObject*>(object));
}

}  // namespace internal
}  // namespace v8
