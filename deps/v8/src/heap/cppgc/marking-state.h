// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_CPPGC_MARKING_STATE_H_
#define V8_HEAP_CPPGC_MARKING_STATE_H_

#include "include/cppgc/trace-trait.h"
#include "src/heap/cppgc/globals.h"
#include "src/heap/cppgc/heap-object-header.h"
#include "src/heap/cppgc/heap-page.h"
#include "src/heap/cppgc/liveness-broker.h"
#include "src/heap/cppgc/marking-worklists.h"

namespace cppgc {
namespace internal {

// C++ marking implementation.
class MarkingState {
 public:
  inline MarkingState(HeapBase& heap, MarkingWorklists&);

  MarkingState(const MarkingState&) = delete;
  MarkingState& operator=(const MarkingState&) = delete;

  inline void MarkAndPush(const void*, TraceDescriptor);
  inline void MarkAndPush(HeapObjectHeader&, TraceDescriptor);
  inline void MarkAndPush(HeapObjectHeader&);

  inline bool MarkNoPush(HeapObjectHeader&);

  template <
      HeapObjectHeader::AccessMode = HeapObjectHeader::AccessMode::kNonAtomic>
  inline void DynamicallyMarkAddress(ConstAddress);

  inline void RegisterWeakReferenceIfNeeded(const void*, TraceDescriptor,
                                            WeakCallback, const void*);
  inline void RegisterWeakCallback(WeakCallback, const void*);
  inline void InvokeWeakRootsCallbackIfNeeded(const void*, TraceDescriptor,
                                              WeakCallback, const void*);

  inline void AccountMarkedBytes(const HeapObjectHeader&);
  size_t marked_bytes() const { return marked_bytes_; }

  void Publish() {
    marking_worklist_.Publish();
    not_fully_constructed_worklist_.Publish();
    previously_not_fully_constructed_worklist_.Publish();
    weak_callback_worklist_.Publish();
    write_barrier_worklist_.Publish();
  }

  // Moves objects in not_fully_constructed_worklist_ to
  // previously_not_full_constructed_worklists_.
  void FlushNotFullyConstructedObjects();

  MarkingWorklists::MarkingWorklist::Local& marking_worklist() {
    return marking_worklist_;
  }
  MarkingWorklists::NotFullyConstructedWorklist::Local&
  not_fully_constructed_worklist() {
    return not_fully_constructed_worklist_;
  }
  MarkingWorklists::NotFullyConstructedWorklist::Local&
  previously_not_fully_constructed_worklist() {
    return previously_not_fully_constructed_worklist_;
  }
  MarkingWorklists::WeakCallbackWorklist::Local& weak_callback_worklist() {
    return weak_callback_worklist_;
  }
  MarkingWorklists::WriteBarrierWorklist::Local& write_barrier_worklist() {
    return write_barrier_worklist_;
  }

 private:
#ifdef DEBUG
  HeapBase& heap_;
#endif  // DEBUG

  MarkingWorklists::MarkingWorklist::Local marking_worklist_;
  MarkingWorklists::NotFullyConstructedWorklist::Local
      not_fully_constructed_worklist_;
  MarkingWorklists::NotFullyConstructedWorklist::Local
      previously_not_fully_constructed_worklist_;
  MarkingWorklists::WeakCallbackWorklist::Local weak_callback_worklist_;
  MarkingWorklists::WriteBarrierWorklist::Local write_barrier_worklist_;

  size_t marked_bytes_ = 0;
};

MarkingState::MarkingState(HeapBase& heap, MarkingWorklists& marking_worklists)
    :
#ifdef DEBUG
      heap_(heap),
#endif  // DEBUG
      marking_worklist_(marking_worklists.marking_worklist()),
      not_fully_constructed_worklist_(
          marking_worklists.not_fully_constructed_worklist()),
      previously_not_fully_constructed_worklist_(
          marking_worklists.previously_not_fully_constructed_worklist()),
      weak_callback_worklist_(marking_worklists.weak_callback_worklist()),
      write_barrier_worklist_(marking_worklists.write_barrier_worklist()) {
}

void MarkingState::MarkAndPush(const void* object, TraceDescriptor desc) {
  DCHECK_NOT_NULL(object);
  MarkAndPush(HeapObjectHeader::FromPayload(
                  const_cast<void*>(desc.base_object_payload)),
              desc);
}

void MarkingState::MarkAndPush(HeapObjectHeader& header, TraceDescriptor desc) {
  DCHECK_NOT_NULL(desc.callback);

  if (!MarkNoPush(header)) return;

  if (header.IsInConstruction<HeapObjectHeader::AccessMode::kNonAtomic>()) {
    not_fully_constructed_worklist_.Push(&header);
  } else {
    marking_worklist_.Push(desc);
  }
}

bool MarkingState::MarkNoPush(HeapObjectHeader& header) {
  // A GC should only mark the objects that belong in its heap.
  DCHECK_EQ(&heap_, BasePage::FromPayload(&header)->heap());
  // Never mark free space objects. This would e.g. hint to marking a promptly
  // freed backing store.
  DCHECK(!header.IsFree());
  return header.TryMarkAtomic();
}

template <HeapObjectHeader::AccessMode mode>
void MarkingState::DynamicallyMarkAddress(ConstAddress address) {
  HeapObjectHeader& header =
      BasePage::FromPayload(address)->ObjectHeaderFromInnerAddress<mode>(
          const_cast<Address>(address));
  DCHECK(!header.IsInConstruction<mode>());
  if (MarkNoPush(header)) {
    marking_worklist_.Push(
        {reinterpret_cast<void*>(header.Payload()),
         GlobalGCInfoTable::GCInfoFromIndex(header.GetGCInfoIndex<mode>())
             .trace});
  }
}

void MarkingState::MarkAndPush(HeapObjectHeader& header) {
  MarkAndPush(
      header,
      {header.Payload(),
       GlobalGCInfoTable::GCInfoFromIndex(header.GetGCInfoIndex()).trace});
}

void MarkingState::RegisterWeakReferenceIfNeeded(const void* object,
                                                 TraceDescriptor desc,
                                                 WeakCallback weak_callback,
                                                 const void* parameter) {
  // Filter out already marked values. The write barrier for WeakMember
  // ensures that any newly set value after this point is kept alive and does
  // not require the callback.
  if (HeapObjectHeader::FromPayload(desc.base_object_payload)
          .IsMarked<HeapObjectHeader::AccessMode::kAtomic>())
    return;
  RegisterWeakCallback(weak_callback, parameter);
}

void MarkingState::InvokeWeakRootsCallbackIfNeeded(const void* object,
                                                   TraceDescriptor desc,
                                                   WeakCallback weak_callback,
                                                   const void* parameter) {
  // Since weak roots are only traced at the end of marking, we can execute
  // the callback instead of registering it.
#if DEBUG
  const HeapObjectHeader& header =
      HeapObjectHeader::FromPayload(desc.base_object_payload);
  DCHECK_IMPLIES(header.IsInConstruction(), header.IsMarked());
#endif  // DEBUG
  weak_callback(LivenessBrokerFactory::Create(), parameter);
}

void MarkingState::RegisterWeakCallback(WeakCallback callback,
                                        const void* object) {
  weak_callback_worklist_.Push({callback, object});
}

void MarkingState::AccountMarkedBytes(const HeapObjectHeader& header) {
  marked_bytes_ +=
      header.IsLargeObject()
          ? reinterpret_cast<const LargePage*>(BasePage::FromPayload(&header))
                ->PayloadSize()
          : header.GetSize();
}

}  // namespace internal
}  // namespace cppgc

#endif  // V8_HEAP_CPPGC_MARKING_STATE_H_
