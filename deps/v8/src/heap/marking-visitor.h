// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_MARKING_VISITOR_H_
#define V8_HEAP_MARKING_VISITOR_H_

#include "src/common/globals.h"
#include "src/execution/isolate.h"
#include "src/heap/marking-state.h"
#include "src/heap/marking-worklist.h"
#include "src/heap/objects-visiting.h"
#include "src/heap/pretenuring-handler.h"
#include "src/heap/spaces.h"
#include "src/heap/weak-object-worklists.h"
#include "src/objects/string.h"

namespace v8 {
namespace internal {

struct EphemeronMarking {
  std::vector<HeapObject> newly_discovered;
  bool newly_discovered_overflowed;
  size_t newly_discovered_limit;
};

// The base class for all marking visitors. It implements marking logic with
// support of bytecode flushing, embedder tracing, weak and references.
//
// Derived classes are expected to provide the following:
// - ConcreteVisitor::marking_state method,
// - ConcreteVisitor::retaining_path_mode method,
// - ConcreteVisitor::RecordSlot method,
// - ConcreteVisitor::RecordRelocSlot method,
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
                     bool trace_embedder_fields,
                     bool should_keep_ages_unchanged)
      : HeapVisitor<int, ConcreteVisitor>(heap),
        local_marking_worklists_(local_marking_worklists),
        local_weak_objects_(local_weak_objects),
        heap_(heap),
        mark_compact_epoch_(mark_compact_epoch),
        code_flush_mode_(code_flush_mode),
        trace_embedder_fields_(trace_embedder_fields),
        should_keep_ages_unchanged_(should_keep_ages_unchanged),
        should_mark_shared_heap_(heap->isolate()->is_shared_space_isolate())
#ifdef V8_ENABLE_SANDBOX
        ,
        external_pointer_table_(&heap->isolate()->external_pointer_table()),
        shared_external_pointer_table_(
            &heap->isolate()->shared_external_pointer_table())
#endif  // V8_ENABLE_SANDBOX
  {
  }

  V8_INLINE int VisitBytecodeArray(Map map, BytecodeArray object);
  V8_INLINE int VisitDescriptorArrayStrongly(Map map, DescriptorArray object);
  V8_INLINE int VisitDescriptorArray(Map map, DescriptorArray object);
  V8_INLINE int VisitEphemeronHashTable(Map map, EphemeronHashTable object);
  V8_INLINE int VisitFixedArray(Map map, FixedArray object);
  V8_INLINE int VisitJSApiObject(Map map, JSObject object);
  V8_INLINE int VisitJSArrayBuffer(Map map, JSArrayBuffer object);
  V8_INLINE int VisitJSDataViewOrRabGsabDataView(
      Map map, JSDataViewOrRabGsabDataView object);
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
    ProcessStrongHeapObject(host, host.map_slot(), map);
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
  V8_INLINE void VisitCodePointer(Code host, CodeObjectSlot slot) final {
    VisitCodePointerImpl(host, slot);
  }
  V8_INLINE void VisitEmbeddedPointer(RelocInfo* rinfo) final;
  V8_INLINE void VisitCodeTarget(RelocInfo* rinfo) final;
  void VisitCustomWeakPointers(HeapObject host, ObjectSlot start,
                               ObjectSlot end) final {
    // Weak list pointers should be ignored during marking. The lists are
    // reconstructed after GC.
  }

  V8_INLINE void VisitExternalPointer(HeapObject host, ExternalPointerSlot slot,
                                      ExternalPointerTag tag) final;
  void SynchronizePageAccess(HeapObject heap_object) {
#ifdef THREAD_SANITIZER
    // This is needed because TSAN does not process the memory fence
    // emitted after page initialization.
    BasicMemoryChunk::FromHeapObject(heap_object)->SynchronizedHeapLoad();
#endif
  }

  bool ShouldMarkObject(HeapObject object) const {
    if (object.InReadOnlySpace()) return false;
    if (should_mark_shared_heap_) return true;
    return !object.InAnySharedSpace();
  }

  // Marks the object grey and pushes it on the marking work list.
  V8_INLINE void MarkObject(HeapObject host, HeapObject obj);

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
  V8_INLINE void VisitCodePointerImpl(Code host, CodeObjectSlot slot);

  V8_INLINE void VisitDescriptorsForMap(Map map);

  template <typename T>
  int VisitEmbedderTracingSubclass(Map map, T object);
  template <typename T>
  int VisitEmbedderTracingSubClassWithEmbedderTracing(Map map, T object);
  template <typename T>
  int VisitEmbedderTracingSubClassNoEmbedderTracing(Map map, T object);

  V8_INLINE int VisitFixedArrayWithProgressBar(Map map, FixedArray object,
                                               ProgressBar& progress_bar);
  V8_INLINE int VisitFixedArrayRegularly(Map map, FixedArray object);

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
  const bool trace_embedder_fields_;
  const bool should_keep_ages_unchanged_;
  const bool should_mark_shared_heap_;
#ifdef V8_ENABLE_SANDBOX
  ExternalPointerTable* const external_pointer_table_;
  ExternalPointerTable* const shared_external_pointer_table_;
#endif  // V8_ENABLE_SANDBOX
};

template <typename ConcreteVisitor, typename MarkingState>
class YoungGenerationMarkingVisitorBase
    : public NewSpaceVisitor<ConcreteVisitor> {
 public:
  YoungGenerationMarkingVisitorBase(Isolate* isolate,
                                    MarkingWorklists::Local* worklists_local);

  ~YoungGenerationMarkingVisitorBase() override {
    DCHECK(local_pretenuring_feedback_.empty());
  }

  V8_INLINE void VisitPointers(HeapObject host, ObjectSlot start,
                               ObjectSlot end) final {
    VisitPointersImpl(host, start, end);
  }

  V8_INLINE void VisitPointers(HeapObject host, MaybeObjectSlot start,
                               MaybeObjectSlot end) final {
    VisitPointersImpl(host, start, end);
  }

  V8_INLINE void VisitCodePointer(Code host, CodeObjectSlot slot) override {
    CHECK(V8_EXTERNAL_CODE_SPACE_BOOL);
    // InstructionStream slots never appear in new space because
    // Code objects, the only object that can contain code pointers, are
    // always allocated in the old space.
    UNREACHABLE();
  }

  V8_INLINE void VisitPointer(HeapObject host, ObjectSlot slot) final {
    VisitPointerImpl(host, slot);
  }

  V8_INLINE void VisitPointer(HeapObject host, MaybeObjectSlot slot) final {
    VisitPointerImpl(host, slot);
  }

  V8_INLINE void VisitCodeTarget(RelocInfo* rinfo) final {
    // Code objects are not expected in new space.
    UNREACHABLE();
  }

  V8_INLINE void VisitEmbeddedPointer(RelocInfo* rinfo) final {
    // Code objects are not expected in new space.
    UNREACHABLE();
  }

  V8_INLINE int VisitJSApiObject(Map map, JSObject object);
  V8_INLINE int VisitJSArrayBuffer(Map map, JSArrayBuffer object);
  V8_INLINE int VisitJSDataViewOrRabGsabDataView(
      Map map, JSDataViewOrRabGsabDataView object);
  V8_INLINE int VisitJSTypedArray(Map map, JSTypedArray object);

  V8_INLINE int VisitJSObject(Map map, JSObject object);
  V8_INLINE int VisitJSObjectFast(Map map, JSObject object);
  template <typename T, typename TBodyDescriptor = typename T::BodyDescriptor>
  V8_INLINE int VisitJSObjectSubclass(Map map, T object);

  V8_INLINE void Finalize();

 protected:
  ConcreteVisitor* concrete_visitor() {
    return static_cast<ConcreteVisitor*>(this);
  }

  template <typename T>
  int VisitEmbedderTracingSubClassWithEmbedderTracing(Map map, T object);

  inline void MarkObjectViaMarkingWorklist(HeapObject object);

 private:
  template <typename TSlot>
  V8_INLINE void VisitPointersImpl(HeapObject host, TSlot start, TSlot end) {
    for (TSlot slot = start; slot < end; ++slot) {
      VisitPointer(host, slot);
    }
  }

  template <typename TSlot>
  void VisitPointerImpl(HeapObject host, TSlot slot);

  MarkingWorklists::Local* worklists_local_;
  PretenuringHandler* const pretenuring_handler_;
  PretenuringHandler::PretenuringFeedbackMap local_pretenuring_feedback_;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_MARKING_VISITOR_H_
