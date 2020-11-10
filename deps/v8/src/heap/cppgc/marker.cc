// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/cppgc/marker.h"

#include "src/heap/cppgc/heap-object-header-inl.h"
#include "src/heap/cppgc/heap.h"
#include "src/heap/cppgc/marking-visitor.h"

namespace cppgc {
namespace internal {

namespace {
template <typename Worklist, typename Callback>
bool DrainWorklistWithDeadline(v8::base::TimeTicks deadline, Worklist* worklist,
                               Callback callback, int task_id) {
  const size_t kDeadlineCheckInterval = 1250;

  size_t processed_callback_count = 0;
  typename Worklist::View view(worklist, task_id);
  typename Worklist::EntryType item;
  while (view.Pop(&item)) {
    callback(item);
    if (++processed_callback_count == kDeadlineCheckInterval) {
      if (deadline <= v8::base::TimeTicks::Now()) {
        return false;
      }
      processed_callback_count = 0;
    }
  }
  return true;
}
}  // namespace

constexpr int Marker::kMutatorThreadId;

Marker::Marker(Heap* heap)
    : heap_(heap), marking_visitor_(CreateMutatorThreadMarkingVisitor()) {}

Marker::~Marker() {
  // The fixed point iteration may have found not-fully-constructed objects.
  // Such objects should have already been found through the stack scan though
  // and should thus already be marked.
  if (!not_fully_constructed_worklist_.IsEmpty()) {
#if DEBUG
    DCHECK_NE(MarkingConfig::StackState::kNoHeapPointers, config_.stack_state_);
    NotFullyConstructedItem item;
    NotFullyConstructedWorklist::View view(&not_fully_constructed_worklist_,
                                           kMutatorThreadId);
    while (view.Pop(&item)) {
      // TODO(chromium:1056170): uncomment following check after implementing
      // FromInnerAddress.
      //
      // HeapObjectHeader* const header = HeapObjectHeader::FromInnerAddress(
      //     reinterpret_cast<Address>(const_cast<void*>(item)));
      // DCHECK(header->IsMarked())
    }
#else
    not_fully_constructed_worklist_.Clear();
#endif
  }
}

void Marker::StartMarking(MarkingConfig config) {
  config_ = config;
  VisitRoots();
}

void Marker::FinishMarking() {
  if (config_.stack_state_ == MarkingConfig::StackState::kNoHeapPointers) {
    FlushNotFullyConstructedObjects();
  }
  AdvanceMarkingWithDeadline(v8::base::TimeDelta::Max());
}

void Marker::ProcessWeakness() {
  heap_->GetWeakPersistentRegion().Trace(marking_visitor_.get());

  // Call weak callbacks on objects that may now be pointing to dead objects.
  WeakCallbackItem item;
  LivenessBroker broker = LivenessBrokerFactory::Create();
  WeakCallbackWorklist::View view(&weak_callback_worklist_, kMutatorThreadId);
  while (view.Pop(&item)) {
    item.callback(broker, item.parameter);
  }
  // Weak callbacks should not add any new objects for marking.
  DCHECK(marking_worklist_.IsEmpty());
}

void Marker::VisitRoots() {
  heap_->GetStrongPersistentRegion().Trace(marking_visitor_.get());
  if (config_.stack_state_ != MarkingConfig::StackState::kNoHeapPointers)
    heap_->stack()->IteratePointers(marking_visitor_.get());
}

std::unique_ptr<MutatorThreadMarkingVisitor>
Marker::CreateMutatorThreadMarkingVisitor() {
  return std::make_unique<MutatorThreadMarkingVisitor>(this);
}

bool Marker::AdvanceMarkingWithDeadline(v8::base::TimeDelta duration) {
  MutatorThreadMarkingVisitor* visitor = marking_visitor_.get();
  v8::base::TimeTicks deadline = v8::base::TimeTicks::Now() + duration;

  do {
    // Convert |previously_not_fully_constructed_worklist_| to
    // |marking_worklist_|. This merely re-adds items with the proper
    // callbacks.
    if (!DrainWorklistWithDeadline(
            deadline, &previously_not_fully_constructed_worklist_,
            [visitor](NotFullyConstructedItem& item) {
              visitor->DynamicallyMarkAddress(
                  reinterpret_cast<ConstAddress>(item));
            },
            kMutatorThreadId))
      return false;

    if (!DrainWorklistWithDeadline(
            deadline, &marking_worklist_,
            [visitor](const MarkingItem& item) {
              const HeapObjectHeader& header =
                  HeapObjectHeader::FromPayload(item.base_object_payload);
              DCHECK(!MutatorThreadMarkingVisitor::IsInConstruction(header));
              item.callback(visitor, item.base_object_payload);
              visitor->AccountMarkedBytes(header);
            },
            kMutatorThreadId))
      return false;
  } while (!marking_worklist_.IsLocalViewEmpty(kMutatorThreadId));

  return true;
}

void Marker::FlushNotFullyConstructedObjects() {
  if (!not_fully_constructed_worklist_.IsLocalViewEmpty(kMutatorThreadId)) {
    not_fully_constructed_worklist_.FlushToGlobal(kMutatorThreadId);
    previously_not_fully_constructed_worklist_.MergeGlobalPool(
        &not_fully_constructed_worklist_);
  }
  DCHECK(not_fully_constructed_worklist_.IsLocalViewEmpty(kMutatorThreadId));
}

void Marker::ClearAllWorklistsForTesting() {
  marking_worklist_.Clear();
  not_fully_constructed_worklist_.Clear();
  previously_not_fully_constructed_worklist_.Clear();
  weak_callback_worklist_.Clear();
}

}  // namespace internal
}  // namespace cppgc
