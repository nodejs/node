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
  // Declares that this marking state is not collecting retainers, so the
  // marking visitor may update the heap state to store information about
  // progress, and may avoid fully visiting an object if it is safe to do so.
  static constexpr bool kCollectRetainers = false;

  explicit MarkingStateBase(PtrComprCageBase cage_base)
#if V8_COMPRESS_POINTERS
      : cage_base_(cage_base)
#endif
  {
  }

  // The pointer compression cage base value used for decompression of all
  // tagged values except references to Code objects.
  V8_INLINE PtrComprCageBase cage_base() const {
#if V8_COMPRESS_POINTERS
    return cage_base_;
#else
    return PtrComprCageBase{};
#endif  // V8_COMPRESS_POINTERS
  }

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
        MemoryChunk::cast(chunk), obj.Size(cage_base()));
    return true;
  }

  V8_INLINE bool GreyToBlackUnaccounted(HeapObject obj) {
    return Marking::GreyToBlack<access_mode>(MarkBitFrom(obj));
  }

  void ClearLiveness(MemoryChunk* chunk) {
    static_cast<ConcreteState*>(this)->bitmap(chunk)->Clear();
    static_cast<ConcreteState*>(this)->SetLiveBytes(chunk, 0);
  }

  void AddStrongReferenceForReferenceSummarizer(HeapObject host,
                                                HeapObject obj) {
    // This is not a reference summarizer, so there is nothing to do here.
  }

  void AddWeakReferenceForReferenceSummarizer(HeapObject host, HeapObject obj) {
    // This is not a reference summarizer, so there is nothing to do here.
  }

 private:
#if V8_COMPRESS_POINTERS
  const PtrComprCageBase cage_base_;
#endif  // V8_COMPRESS_POINTERS
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
  MarkingVisitorBase(MarkingWorklists::Local* local_marking_worklists,
                     WeakObjects::Local* local_weak_objects, Heap* heap,
                     unsigned mark_compact_epoch,
                     base::EnumSet<CodeFlushMode> code_flush_mode,
                     bool is_embedder_tracing_enabled,
                     bool should_keep_ages_unchanged)
      : HeapVisitor<int, ConcreteVisitor>(heap),
        local_marking_worklists_(local_marking_worklists),
        local_weak_objects_(local_weak_objects),
        heap_(heap),
        mark_compact_epoch_(mark_compact_epoch),
        code_flush_mode_(code_flush_mode),
        is_embedder_tracing_enabled_(is_embedder_tracing_enabled),
        should_keep_ages_unchanged_(should_keep_ages_unchanged),
        is_shared_heap_(heap->IsShared())
#ifdef V8_SANDBOXED_EXTERNAL_POINTERS
        ,
        external_pointer_table_(&heap->isolate()->external_pointer_table())
#endif  // V8_SANDBOXED_EXTERNAL_POINTERS
  {
  }

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
  void VisitMapPointer(HeapObject host) final {
    Map map = host.map(ObjectVisitorWithCageBases::cage_base());
    MarkObject(host, map);
    concrete_visitor()->RecordSlot(host, host.map_slot(), map);
  }
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
  V8_INLINE void VisitCodePointer(HeapObject host, CodeObjectSlot slot) final {
    VisitCodePointerImpl(host, slot);
  }
  V8_INLINE void VisitEmbeddedPointer(Code host, RelocInfo* rinfo) final;
  V8_INLINE void VisitCodeTarget(Code host, RelocInfo* rinfo) final;
  void VisitCustomWeakPointers(HeapObject host, ObjectSlot start,
                               ObjectSlot end) final {
    // Weak list pointers should be ignored during marking. The lists are
    // reconstructed after GC.
  }

  V8_INLINE void VisitExternalPointer(HeapObject host,
                                      ExternalPointer_t ptr) final {
#ifdef V8_SANDBOXED_EXTERNAL_POINTERS
    uint32_t index = ptr >> kExternalPointerIndexShift;
    external_pointer_table_->Mark(index);
#endif  // V8_SANDBOXED_EXTERNAL_POINTERS
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

  // Similar to VisitPointersImpl() but using code cage base for loading from
  // the slot.
  V8_INLINE void VisitCodePointerImpl(HeapObject host, CodeObjectSlot slot);

  V8_INLINE void VisitDescriptors(DescriptorArray descriptors,
                                  int number_of_own_descriptors);

  V8_INLINE int VisitDescriptorsForMap(Map map);

  template <typename T>
  int VisitEmbedderTracingSubclass(Map map, T object);
  template <typename T>
  int VisitEmbedderTracingSubClassWithEmbedderTracing(Map map, T object);
  template <typename T>
  int VisitEmbedderTracingSubClassNoEmbedderTracing(Map map, T object);

  V8_INLINE int VisitFixedArrayWithProgressBar(Map map, FixedArray object,
                                               ProgressBar& progress_bar);
  // Marks the descriptor array black without pushing it on the marking work
  // list and visits its header. Returns the size of the descriptor array
  // if it was successully marked as black.
  V8_INLINE int MarkDescriptorArrayBlack(DescriptorArray descriptors);
  // Marks the object grey and pushes it on the marking work list.
  V8_INLINE void MarkObject(HeapObject host, HeapObject obj);

  V8_INLINE void AddStrongReferenceForReferenceSummarizer(HeapObject host,
                                                          HeapObject obj) {
    concrete_visitor()
        ->marking_state()
        ->AddStrongReferenceForReferenceSummarizer(host, obj);
  }

  V8_INLINE void AddWeakReferenceForReferenceSummarizer(HeapObject host,
                                                        HeapObject obj) {
    concrete_visitor()->marking_state()->AddWeakReferenceForReferenceSummarizer(
        host, obj);
  }

  constexpr bool CanUpdateValuesInHeap() {
    return !MarkingState::kCollectRetainers;
  }

  MarkingWorklists::Local* const local_marking_worklists_;
  WeakObjects::Local* const local_weak_objects_;
  Heap* const heap_;
  const unsigned mark_compact_epoch_;
  const base::EnumSet<CodeFlushMode> code_flush_mode_;
  const bool is_embedder_tracing_enabled_;
  const bool should_keep_ages_unchanged_;
  const bool is_shared_heap_;
#ifdef V8_SANDBOXED_EXTERNAL_POINTERS
  ExternalPointerTable* const external_pointer_table_;
#endif  // V8_SANDBOXED_EXTERNAL_POINTERS
};

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_MARKING_VISITOR_H_
