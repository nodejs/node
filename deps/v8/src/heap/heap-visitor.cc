// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/heap-visitor.h"

#include "src/heap/heap-inl.h"
#include "src/heap/heap-visitor-inl.h"
#include "src/heap/mark-compact-inl.h"
#include "src/objects/allocation-site.h"
#include "src/objects/casting-inl.h"
#include "src/objects/js-weak-refs.h"

namespace v8 {
namespace internal {

template <class T>
struct WeakListVisitor;

template <class T>
Tagged<Object> VisitWeakList(Heap* heap, Tagged<Object> list,
                             WeakObjectRetainer* retainer) {
  static constexpr int kSlotOffset = WeakListVisitor<T>::kWeakNextOffset;

  Tagged<HeapObject> undefined = ReadOnlyRoots(heap).undefined_value();
  Tagged<Object> head = undefined;
  Tagged<T> tail;
  const bool should_record_slots = retainer->ShouldRecordSlots();

  while (list != undefined) {
    // Check whether to keep the candidate in the list.
    Tagged<Object> retained = retainer->RetainAs(list);

    // Move to the next element before the WeakNext is cleared.
    list = Cast<HeapObject>(list)->RawField(kSlotOffset).load();

    if (retained != Tagged<Object>()) {
      if (head == undefined) {
        // First element in the list.
        head = retained;
      } else {
        // Subsequent elements in the list.
        DCHECK(!tail.is_null());
        Tagged<HeapObject> slot_holder = Cast<HeapObject>(tail);
        ObjectSlot slot = slot_holder->RawField(kSlotOffset);
        // Any needed write barriers are handled by
        // `WeakObjectRetainer::RecordSlot`.
        slot.store(retained);
        if (should_record_slots) {
          retainer->RecordSlot(slot_holder, slot, Cast<HeapObject>(retained));
        }
      }
      // Retained object is new tail.
      DCHECK(!IsUndefined(retained, heap->isolate()));
      tail = Cast<T>(retained);

      // tail is a live object, visit it.
      WeakListVisitor<T>::VisitLiveObject(heap, tail);
    }
  }

  // Terminate the list if there is one or more elements.
  if (!tail.is_null()) {
    Cast<HeapObject>(tail)->RawField(kSlotOffset).store(undefined);
  }
  return head;
}

template <>
struct WeakListVisitor<Context> {
  static constexpr int kWeakNextOffset =
      FixedArray::SizeFor(Context::NEXT_CONTEXT_LINK);

  static void VisitLiveObject(Heap* heap, Tagged<Context> context) {
    DCHECK_EQ(Heap::MARK_COMPACT, heap->gc_state());
    // Record the slots of the weak entries in the native context.
    for (int idx = Context::FIRST_WEAK_SLOT;
         idx < Context::NATIVE_CONTEXT_SLOTS; ++idx) {
      ObjectSlot slot = context->RawField(Context::OffsetOfElementAt(idx));
      MarkCompactCollector::RecordSlot(context, slot, Cast<HeapObject>(*slot));
    }
  }
};

template <>
struct WeakListVisitor<AllocationSiteWithWeakNext> {
  static constexpr int kWeakNextOffset =
      offsetof(AllocationSiteWithWeakNext, weak_next_);

  static void VisitLiveObject(Heap*, Tagged<AllocationSite>) {}
};

template <>
struct WeakListVisitor<JSFinalizationRegistry> {
  static constexpr int kWeakNextOffset =
      JSFinalizationRegistry::kNextDirtyOffset;

  static void VisitLiveObject(Heap* heap, Tagged<JSFinalizationRegistry> obj) {
    heap->set_dirty_js_finalization_registries_list_tail(obj);
  }
};

template Tagged<Object> VisitWeakList<Context>(Heap* heap, Tagged<Object> list,
                                               WeakObjectRetainer* retainer);

template Tagged<Object> VisitWeakList<AllocationSiteWithWeakNext>(
    Heap* heap, Tagged<Object> list, WeakObjectRetainer* retainer);

template Tagged<Object> VisitWeakList<JSFinalizationRegistry>(
    Heap* heap, Tagged<Object> list, WeakObjectRetainer* retainer);
}  // namespace internal
}  // namespace v8
