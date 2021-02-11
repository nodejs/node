// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_MARKING_VISITOR_H_
#define V8_HEAP_MARKING_VISITOR_H_

#include "src/common/globals.h"
#include "src/heap/marking-worklist.h"
#include "src/heap/marking.h"
#include "src/heap/memory-chunk.h"
#include "src/heap/objects-visiting.h"
#include "src/heap/spaces.h"
#include "src/heap/weak-object-worklists.h"
#include "src/heap/worklist.h"

namespace v8 {
namespace internal {

struct EphemeronMarking {
  std::vector<HeapObject> newly_discovered;
  bool newly_discovered_overflowed;
  size_t newly_discovered_limit;
};

template <typename ConcreteState, AccessMode access_mode>
class MarkingStateBase {
 public:
  V8_INLINE MarkBit MarkBitFrom(HeapObject obj) {
    return MarkBitFrom(BasicMemoryChunk::FromHeapObject(obj), obj.ptr());
  }

  // {addr} may be tagged or aligned.
  V8_INLINE MarkBit MarkBitFrom(BasicMemoryChunk* p, Address addr) {
    return static_cast<ConcreteState*>(this)->bitmap(p)->MarkBitFromIndex(
        p->AddressToMarkbitIndex(addr));
  }

  Marking::ObjectColor Color(HeapObject obj) {
    return Marking::Color(MarkBitFrom(obj));
  }

  V8_INLINE bool IsImpossible(HeapObject obj) {
    return Marking::IsImpossible<access_mode>(MarkBitFrom(obj));
  }

  V8_INLINE bool IsBlack(HeapObject obj) {
    return Marking::IsBlack<access_mode>(MarkBitFrom(obj));
  }

  V8_INLINE bool IsWhite(HeapObject obj) {
    return Marking::IsWhite<access_mode>(MarkBitFrom(obj));
  }

  V8_INLINE bool IsGrey(HeapObject obj) {
    return Marking::IsGrey<access_mode>(MarkBitFrom(obj));
  }

  V8_INLINE bool IsBlackOrGrey(HeapObject obj) {
    return Marking::IsBlackOrGrey<access_mode>(MarkBitFrom(obj));
  }

  V8_INLINE bool WhiteToGrey(HeapObject obj) {
    return Marking::WhiteToGrey<access_mode>(MarkBitFrom(obj));
  }

  V8_INLINE bool WhiteToBlack(HeapObject obj) {
    return WhiteToGrey(obj) && GreyToBlack(obj);
  }

  V8_INLINE bool GreyToBlack(HeapObject obj) {
    BasicMemoryChunk* chunk = BasicMemoryChunk::FromHeapObject(obj);
    MarkBit markbit = MarkBitFrom(chunk, obj.address());
    if (!Marking::GreyToBlack<access_mode>(markbit)) return false;
    static_cast<ConcreteState*>(this)->IncrementLiveBytes(
        MemoryChunk::cast(chunk), obj.Size());
    return true;
  }

  void ClearLiveness(MemoryChunk* chunk) {
    static_cast<ConcreteState*>(this)->bitmap(chunk)->Clear();
    static_cast<ConcreteState*>(this)->SetLiveBytes(chunk, 0);
  }
};

// The base class for all marking visitors. It implements marking logic with
// support of bytecode flushing, embedder tracing, weak and references.
//
// Derived classes are expected to provide the following:
// - ConcreteVisitor::marking_state method,
// - ConcreteVisitor::retaining_path_mode method,
// - ConcreteVisitor::RecordSlot method,
// - ConcreteVisitor::RecordRelocSlot method,
// - ConcreteVisitor::SynchronizePageAccess method,
// - ConcreteVisitor::VisitJSObjectSubclass method,
// - ConcreteVisitor::VisitLeftTrimmableArray method.
// These methods capture the difference between the concurrent and main thread
// marking visitors. For example, the concurrent visitor has to use the
// snapshotting protocol to visit JSObject and left-trimmable FixedArrays.

template <typename ConcreteVisitor, typename MarkingState>
class MarkingVisitorBase : public HeapVisitor<int, ConcreteVisitor> {
 public:
  MarkingVisitorBase(int task_id,
                     MarkingWorklists::Local* local_marking_worklists,
                     WeakObjects* weak_objects, Heap* heap,
                     unsigned mark_compact_epoch,
                     BytecodeFlushMode bytecode_flush_mode,
                     bool is_embedder_tracing_enabled, bool is_forced_gc)
      : local_marking_worklists_(local_marking_worklists),
        weak_objects_(weak_objects),
        heap_(heap),
        task_id_(task_id),
        mark_compact_epoch_(mark_compact_epoch),
        bytecode_flush_mode_(bytecode_flush_mode),
        is_embedder_tracing_enabled_(is_embedder_tracing_enabled),
        is_forced_gc_(is_forced_gc) {}

  V8_INLINE int VisitBytecodeArray(Map map, BytecodeArray object);
  V8_INLINE int VisitDescriptorArray(Map map, DescriptorArray object);
  V8_INLINE int VisitEphemeronHashTable(Map map, EphemeronHashTable object);
  V8_INLINE int VisitFixedArray(Map map, FixedArray object);
  V8_INLINE int VisitFixedDoubleArray(Map map, FixedDoubleArray object);
  V8_INLINE int VisitJSApiObject(Map map, JSObject object);
  V8_INLINE int VisitJSArrayBuffer(Map map, JSArrayBuffer object);
  V8_INLINE int VisitJSDataView(Map map, JSDataView object);
  V8_INLINE int VisitJSFunction(Map map, JSFunction object);
  V8_INLINE int VisitJSTypedArray(Map map, JSTypedArray object);
  V8_INLINE int VisitJSWeakRef(Map map, JSWeakRef object);
  V8_INLINE int VisitMap(Map map, Map object);
  V8_INLINE int VisitSharedFunctionInfo(Map map, SharedFunctionInfo object);
  V8_INLINE int VisitTransitionArray(Map map, TransitionArray object);
  V8_INLINE int VisitWeakCell(Map map, WeakCell object);

  // ObjectVisitor overrides.
  V8_INLINE void VisitPointer(HeapObject host, ObjectSlot p) final {
    VisitPointersImpl(host, p, p + 1);
  }
  V8_INLINE void VisitPointer(HeapObject host, MaybeObjectSlot p) final {
    VisitPointersImpl(host, p, p + 1);
  }
  V8_INLINE void VisitPointers(HeapObject host, ObjectSlot start,
                               ObjectSlot end) final {
    VisitPointersImpl(host, start, end);
  }
  V8_INLINE void VisitPointers(HeapObject host, MaybeObjectSlot start,
                               MaybeObjectSlot end) final {
    VisitPointersImpl(host, start, end);
  }
  V8_INLINE void VisitEmbeddedPointer(Code host, RelocInfo* rinfo) final;
  V8_INLINE void VisitCodeTarget(Code host, RelocInfo* rinfo) final;
  void VisitCustomWeakPointers(HeapObject host, ObjectSlot start,
                               ObjectSlot end) final {
    // Weak list pointers should be ignored during marking. The lists are
    // reconstructed after GC.
  }

 protected:
  ConcreteVisitor* concrete_visitor() {
    return static_cast<ConcreteVisitor*>(this);
  }
  template <typename THeapObjectSlot>
  void ProcessStrongHeapObject(HeapObject host, THeapObjectSlot slot,
                               HeapObject heap_object);
  template <typename THeapObjectSlot>
  void ProcessWeakHeapObject(HeapObject host, THeapObjectSlot slot,
                             HeapObject heap_object);

  template <typename TSlot>
  V8_INLINE void VisitPointerImpl(HeapObject host, TSlot p);

  template <typename TSlot>
  V8_INLINE void VisitPointersImpl(HeapObject host, TSlot start, TSlot end);

  V8_INLINE void VisitDescriptors(DescriptorArray descriptors,
                                  int number_of_own_descriptors);

  V8_INLINE int VisitDescriptorsForMap(Map map);

  template <typename T>
  int VisitEmbedderTracingSubclass(Map map, T object);
  V8_INLINE int VisitFixedArrayWithProgressBar(Map map, FixedArray object,
                                               MemoryChunk* chunk);
  // Marks the descriptor array black without pushing it on the marking work
  // list and visits its header. Returns the size of the descriptor array
  // if it was successully marked as black.
  V8_INLINE int MarkDescriptorArrayBlack(DescriptorArray descriptors);
  // Marks the object grey and pushes it on the marking work list.
  V8_INLINE void MarkObject(HeapObject host, HeapObject obj);

  MarkingWorklists::Local* const local_marking_worklists_;
  WeakObjects* const weak_objects_;
  Heap* const heap_;
  const int task_id_;
  const unsigned mark_compact_epoch_;
  const BytecodeFlushMode bytecode_flush_mode_;
  const bool is_embedder_tracing_enabled_;
  const bool is_forced_gc_;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_MARKING_VISITOR_H_
