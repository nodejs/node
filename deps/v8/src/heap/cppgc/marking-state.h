// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_CPPGC_MARKING_STATE_H_
#define V8_HEAP_CPPGC_MARKING_STATE_H_

#include <algorithm>

#include "include/cppgc/trace-trait.h"
#include "include/cppgc/visitor.h"
#include "src/heap/cppgc/compaction-worklists.h"
#include "src/heap/cppgc/globals.h"
#include "src/heap/cppgc/heap-object-header.h"
#include "src/heap/cppgc/heap-page.h"
#include "src/heap/cppgc/liveness-broker.h"
#include "src/heap/cppgc/marking-worklists.h"

namespace cppgc {
namespace internal {

// C++ marking implementation.
class MarkingStateBase {
 public:
  inline MarkingStateBase(HeapBase& heap, MarkingWorklists&,
                          CompactionWorklists*);

  MarkingStateBase(const MarkingStateBase&) = delete;
  MarkingStateBase& operator=(const MarkingStateBase&) = delete;

  inline void MarkAndPush(const void*, TraceDescriptor);
  inline void MarkAndPush(HeapObjectHeader&);

  inline void PushMarked(HeapObjectHeader&, TraceDescriptor desc);

  inline void RegisterWeakReferenceIfNeeded(const void*, TraceDescriptor,
                                            WeakCallback, const void*);
  inline void RegisterWeakCallback(WeakCallback, const void*);

  void RegisterMovableReference(const void** slot) {
    if (!movable_slots_worklist_) return;
    movable_slots_worklist_->Push(slot);
  }

  // Weak containers are special in that they may require re-tracing if
  // reachable through stack, even if the container was already traced before.
  // ProcessWeakContainer records which weak containers were already marked so
  // that conservative stack scanning knows to retrace them.
  inline void ProcessWeakContainer(const void*, TraceDescriptor, WeakCallback,
                                   const void*);

  inline void ProcessEphemeron(const void*, const void*, TraceDescriptor,
                               Visitor&);

  inline void AccountMarkedBytes(const HeapObjectHeader&);
  inline void AccountMarkedBytes(size_t);
  size_t marked_bytes() const { return marked_bytes_; }

  void Publish() {
    marking_worklist_.Publish();
    previously_not_fully_constructed_worklist_.Publish();
    weak_callback_worklist_.Publish();
    write_barrier_worklist_.Publish();
    concurrent_marking_bailout_worklist_.Publish();
    discovered_ephemeron_pairs_worklist_.Publish();
    ephemeron_pairs_for_processing_worklist_.Publish();
    if (IsCompactionEnabled()) movable_slots_worklist_->Publish();
  }

  MarkingWorklists::MarkingWorklist::Local& marking_worklist() {
    return marking_worklist_;
  }
  MarkingWorklists::NotFullyConstructedWorklist&
  not_fully_constructed_worklist() {
    return not_fully_constructed_worklist_;
  }
  MarkingWorklists::PreviouslyNotFullyConstructedWorklist::Local&
  previously_not_fully_constructed_worklist() {
    return previously_not_fully_constructed_worklist_;
  }
  MarkingWorklists::WeakCallbackWorklist::Local& weak_callback_worklist() {
    return weak_callback_worklist_;
  }
  MarkingWorklists::WriteBarrierWorklist::Local& write_barrier_worklist() {
    return write_barrier_worklist_;
  }
  MarkingWorklists::ConcurrentMarkingBailoutWorklist::Local&
  concurrent_marking_bailout_worklist() {
    return concurrent_marking_bailout_worklist_;
  }
  MarkingWorklists::EphemeronPairsWorklist::Local&
  discovered_ephemeron_pairs_worklist() {
    return discovered_ephemeron_pairs_worklist_;
  }
  MarkingWorklists::EphemeronPairsWorklist::Local&
  ephemeron_pairs_for_processing_worklist() {
    return ephemeron_pairs_for_processing_worklist_;
  }
  MarkingWorklists::WeakContainersWorklist& weak_containers_worklist() {
    return weak_containers_worklist_;
  }

  CompactionWorklists::MovableReferencesWorklist::Local*
  movable_slots_worklist() {
    return movable_slots_worklist_.get();
  }

  void NotifyCompactionCancelled() {
    DCHECK(IsCompactionEnabled());
    movable_slots_worklist_->Clear();
    movable_slots_worklist_.reset();
  }

 protected:
  inline void MarkAndPush(HeapObjectHeader&, TraceDescriptor);

  inline bool MarkNoPush(HeapObjectHeader&);

  inline void RegisterWeakContainer(HeapObjectHeader&);

  inline bool IsCompactionEnabled() const {
    return movable_slots_worklist_.get();
  }

  HeapBase& heap_;

  MarkingWorklists::MarkingWorklist::Local marking_worklist_;
  MarkingWorklists::NotFullyConstructedWorklist&
      not_fully_constructed_worklist_;
  MarkingWorklists::PreviouslyNotFullyConstructedWorklist::Local
      previously_not_fully_constructed_worklist_;
  MarkingWorklists::WeakCallbackWorklist::Local weak_callback_worklist_;
  MarkingWorklists::WriteBarrierWorklist::Local write_barrier_worklist_;
  MarkingWorklists::ConcurrentMarkingBailoutWorklist::Local
      concurrent_marking_bailout_worklist_;
  MarkingWorklists::EphemeronPairsWorklist::Local
      discovered_ephemeron_pairs_worklist_;
  MarkingWorklists::EphemeronPairsWorklist::Local
      ephemeron_pairs_for_processing_worklist_;
  MarkingWorklists::WeakContainersWorklist& weak_containers_worklist_;
  // Existence of the worklist (|movable_slot_worklist_| != nullptr) denotes
  // that compaction is currently enabled and slots must be recorded.
  std::unique_ptr<CompactionWorklists::MovableReferencesWorklist::Local>
      movable_slots_worklist_;

  size_t marked_bytes_ = 0;
};

MarkingStateBase::MarkingStateBase(HeapBase& heap,
                                   MarkingWorklists& marking_worklists,
                                   CompactionWorklists* compaction_worklists)
    :
      heap_(heap),
      marking_worklist_(marking_worklists.marking_worklist()),
      not_fully_constructed_worklist_(
          *marking_worklists.not_fully_constructed_worklist()),
      previously_not_fully_constructed_worklist_(
          marking_worklists.previously_not_fully_constructed_worklist()),
      weak_callback_worklist_(marking_worklists.weak_callback_worklist()),
      write_barrier_worklist_(marking_worklists.write_barrier_worklist()),
      concurrent_marking_bailout_worklist_(
          marking_worklists.concurrent_marking_bailout_worklist()),
      discovered_ephemeron_pairs_worklist_(
          marking_worklists.discovered_ephemeron_pairs_worklist()),
      ephemeron_pairs_for_processing_worklist_(
          marking_worklists.ephemeron_pairs_for_processing_worklist()),
      weak_containers_worklist_(*marking_worklists.weak_containers_worklist()) {
  if (compaction_worklists) {
    movable_slots_worklist_ =
        std::make_unique<CompactionWorklists::MovableReferencesWorklist::Local>(
            compaction_worklists->movable_slots_worklist());
  }
}

void MarkingStateBase::MarkAndPush(const void* object, TraceDescriptor desc) {
  DCHECK_NOT_NULL(object);
  MarkAndPush(HeapObjectHeader::FromPayload(
                  const_cast<void*>(desc.base_object_payload)),
              desc);
}

void MarkingStateBase::MarkAndPush(HeapObjectHeader& header,
                                   TraceDescriptor desc) {
  DCHECK_NOT_NULL(desc.callback);

  if (header.IsInConstruction<AccessMode::kAtomic>()) {
    not_fully_constructed_worklist_.Push<AccessMode::kAtomic>(&header);
  } else if (MarkNoPush(header)) {
    PushMarked(header, desc);
  }
}

bool MarkingStateBase::MarkNoPush(HeapObjectHeader& header) {
  // A GC should only mark the objects that belong in its heap.
  DCHECK_EQ(&heap_, BasePage::FromPayload(&header)->heap());
  // Never mark free space objects. This would e.g. hint to marking a promptly
  // freed backing store.
  DCHECK(!header.IsFree<AccessMode::kAtomic>());
  return header.TryMarkAtomic();
}

void MarkingStateBase::MarkAndPush(HeapObjectHeader& header) {
  MarkAndPush(
      header,
      {header.Payload(),
       GlobalGCInfoTable::GCInfoFromIndex(header.GetGCInfoIndex()).trace});
}

void MarkingStateBase::PushMarked(HeapObjectHeader& header,
                                  TraceDescriptor desc) {
  DCHECK(header.IsMarked<AccessMode::kAtomic>());
  DCHECK(!header.IsInConstruction<AccessMode::kAtomic>());
  DCHECK_NOT_NULL(desc.callback);

  marking_worklist_.Push(desc);
}

void MarkingStateBase::RegisterWeakReferenceIfNeeded(const void* object,
                                                     TraceDescriptor desc,
                                                     WeakCallback weak_callback,
                                                     const void* parameter) {
  // Filter out already marked values. The write barrier for WeakMember
  // ensures that any newly set value after this point is kept alive and does
  // not require the callback.
  if (HeapObjectHeader::FromPayload(desc.base_object_payload)
          .IsMarked<AccessMode::kAtomic>())
    return;
  RegisterWeakCallback(weak_callback, parameter);
}

void MarkingStateBase::RegisterWeakCallback(WeakCallback callback,
                                            const void* object) {
  DCHECK_NOT_NULL(callback);
  weak_callback_worklist_.Push({callback, object});
}

void MarkingStateBase::RegisterWeakContainer(HeapObjectHeader& header) {
  weak_containers_worklist_.Push<AccessMode::kAtomic>(&header);
}

void MarkingStateBase::ProcessWeakContainer(const void* object,
                                            TraceDescriptor desc,
                                            WeakCallback callback,
                                            const void* data) {
  DCHECK_NOT_NULL(object);

  HeapObjectHeader& header =
      HeapObjectHeader::FromPayload(const_cast<void*>(object));

  if (header.IsInConstruction<AccessMode::kAtomic>()) {
    not_fully_constructed_worklist_.Push<AccessMode::kAtomic>(&header);
    return;
  }

  // Only mark the container initially. Its buckets will be processed after
  // marking.
  if (!MarkNoPush(header)) return;
  RegisterWeakContainer(header);

  // Register final weak processing of the backing store.
  RegisterWeakCallback(callback, data);

  // Weak containers might not require tracing. In such cases the callback in
  // the TraceDescriptor will be nullptr. For ephemerons the callback will be
  // non-nullptr so that the container is traced and the ephemeron pairs are
  // processed.
  if (desc.callback) PushMarked(header, desc);
}

void MarkingStateBase::ProcessEphemeron(const void* key, const void* value,
                                        TraceDescriptor value_desc,
                                        Visitor& visitor) {
  // Filter out already marked keys. The write barrier for WeakMember
  // ensures that any newly set value after this point is kept alive and does
  // not require the callback.
  if (HeapObjectHeader::FromPayload(key).IsMarked<AccessMode::kAtomic>()) {
    if (value_desc.base_object_payload) {
      MarkAndPush(value_desc.base_object_payload, value_desc);
    } else {
      // If value_desc.base_object_payload is nullptr, the value is not GCed and
      // should be immediately traced.
      value_desc.callback(&visitor, value);
    }
    return;
  }
  discovered_ephemeron_pairs_worklist_.Push({key, value, value_desc});
}

void MarkingStateBase::AccountMarkedBytes(const HeapObjectHeader& header) {
  AccountMarkedBytes(
      header.IsLargeObject<AccessMode::kAtomic>()
          ? reinterpret_cast<const LargePage*>(BasePage::FromPayload(&header))
                ->PayloadSize()
          : header.GetSize<AccessMode::kAtomic>());
}

void MarkingStateBase::AccountMarkedBytes(size_t marked_bytes) {
  marked_bytes_ += marked_bytes;
}

class MutatorMarkingState : public MarkingStateBase {
 public:
  MutatorMarkingState(HeapBase& heap, MarkingWorklists& marking_worklists,
                      CompactionWorklists* compaction_worklists)
      : MarkingStateBase(heap, marking_worklists, compaction_worklists) {}

  inline bool MarkNoPush(HeapObjectHeader& header) {
    return MutatorMarkingState::MarkingStateBase::MarkNoPush(header);
  }

  inline void PushMarkedWeakContainer(HeapObjectHeader&);

  inline void DynamicallyMarkAddress(ConstAddress);

  // Moves objects in not_fully_constructed_worklist_ to
  // previously_not_full_constructed_worklists_.
  void FlushNotFullyConstructedObjects();

  // Moves ephemeron pairs in discovered_ephemeron_pairs_worklist_ to
  // ephemeron_pairs_for_processing_worklist_.
  void FlushDiscoveredEphemeronPairs();

  inline void InvokeWeakRootsCallbackIfNeeded(const void*, TraceDescriptor,
                                              WeakCallback, const void*);

  inline bool IsMarkedWeakContainer(HeapObjectHeader&);

 private:
  // Weak containers are strongly retraced during conservative stack scanning.
  // Stack scanning happens once per GC at the start of the atomic pause.
  // Because the visitor is not retained between GCs, there is no need to clear
  // the set at the end of GC.
  class RecentlyRetracedWeakContainers {
    static constexpr size_t kMaxCacheSize = 8;

   public:
    inline bool Contains(const HeapObjectHeader*);
    inline void Insert(const HeapObjectHeader*);

   private:
    std::vector<const HeapObjectHeader*> recently_retraced_cache_;
    size_t last_used_index_ = -1;
  } recently_retraced_weak_containers_;
};

void MutatorMarkingState::PushMarkedWeakContainer(HeapObjectHeader& header) {
  DCHECK(weak_containers_worklist_.Contains(&header));
  recently_retraced_weak_containers_.Insert(&header);
  PushMarked(
      header,
      {header.Payload(),
       GlobalGCInfoTable::GCInfoFromIndex(header.GetGCInfoIndex()).trace});
}

void MutatorMarkingState::DynamicallyMarkAddress(ConstAddress address) {
  HeapObjectHeader& header =
      BasePage::FromPayload(address)->ObjectHeaderFromInnerAddress(
          const_cast<Address>(address));
  DCHECK(!header.IsInConstruction());
  if (MarkNoPush(header)) {
    marking_worklist_.Push(
        {reinterpret_cast<void*>(header.Payload()),
         GlobalGCInfoTable::GCInfoFromIndex(header.GetGCInfoIndex()).trace});
  }
}

void MutatorMarkingState::InvokeWeakRootsCallbackIfNeeded(
    const void* object, TraceDescriptor desc, WeakCallback weak_callback,
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

bool MutatorMarkingState::IsMarkedWeakContainer(HeapObjectHeader& header) {
  const bool result = weak_containers_worklist_.Contains(&header) &&
                      !recently_retraced_weak_containers_.Contains(&header);
  DCHECK_IMPLIES(result, header.IsMarked());
  DCHECK_IMPLIES(result, !header.IsInConstruction());
  return result;
}

bool MutatorMarkingState::RecentlyRetracedWeakContainers::Contains(
    const HeapObjectHeader* header) {
  return std::find(recently_retraced_cache_.begin(),
                   recently_retraced_cache_.end(),
                   header) != recently_retraced_cache_.end();
}

void MutatorMarkingState::RecentlyRetracedWeakContainers::Insert(
    const HeapObjectHeader* header) {
  last_used_index_ = (last_used_index_ + 1) % kMaxCacheSize;
  if (recently_retraced_cache_.size() <= last_used_index_)
    recently_retraced_cache_.push_back(header);
  else
    recently_retraced_cache_[last_used_index_] = header;
}

class ConcurrentMarkingState : public MarkingStateBase {
 public:
  ConcurrentMarkingState(HeapBase& heap, MarkingWorklists& marking_worklists,
                         CompactionWorklists* compaction_worklists)
      : MarkingStateBase(heap, marking_worklists, compaction_worklists) {}

  ~ConcurrentMarkingState() { DCHECK_EQ(last_marked_bytes_, marked_bytes_); }

  size_t RecentlyMarkedBytes() {
    return marked_bytes_ - std::exchange(last_marked_bytes_, marked_bytes_);
  }

  inline void AccountDeferredMarkedBytes(size_t deferred_bytes) {
    // AccountDeferredMarkedBytes is called from Trace methods, which are always
    // called after AccountMarkedBytes, so there should be no underflow here.
    DCHECK_LE(deferred_bytes, marked_bytes_);
    marked_bytes_ -= deferred_bytes;
  }

 private:
  size_t last_marked_bytes_ = 0;
};

template <size_t deadline_check_interval, typename WorklistLocal,
          typename Callback, typename Predicate>
bool DrainWorklistWithPredicate(Predicate should_yield,
                                WorklistLocal& worklist_local,
                                Callback callback) {
  if (worklist_local.IsLocalAndGlobalEmpty()) return true;
  // For concurrent markers, should_yield also reports marked bytes.
  if (should_yield()) return false;
  size_t processed_callback_count = deadline_check_interval;
  typename WorklistLocal::ItemType item;
  while (worklist_local.Pop(&item)) {
    callback(item);
    if (--processed_callback_count == 0) {
      if (should_yield()) {
        return false;
      }
      processed_callback_count = deadline_check_interval;
    }
  }
  return true;
}

template <AccessMode mode>
void DynamicallyTraceMarkedObject(Visitor& visitor,
                                  const HeapObjectHeader& header) {
  DCHECK(!header.IsInConstruction<mode>());
  DCHECK(header.IsMarked<mode>());
  const GCInfo& gcinfo =
      GlobalGCInfoTable::GCInfoFromIndex(header.GetGCInfoIndex<mode>());
  gcinfo.trace(&visitor, header.Payload());
}

}  // namespace internal
}  // namespace cppgc

#endif  // V8_HEAP_CPPGC_MARKING_STATE_H_
