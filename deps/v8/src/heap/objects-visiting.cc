// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/objects-visiting.h"

#include "src/heap/heap-inl.h"
#include "src/heap/mark-compact-inl.h"
#include "src/heap/objects-visiting-inl.h"

namespace v8 {
namespace internal {

// We don't record weak slots during marking or scavenges. Instead we do it
// once when we complete mark-compact cycle.  Note that write barrier has no
// effect if we are already in the middle of compacting mark-sweep cycle and we
// have to record slots manually.
static bool MustRecordSlots(Heap* heap) {
  return heap->gc_state() == Heap::MARK_COMPACT &&
         heap->mark_compact_collector()->is_compacting();
}


template <class T>
struct WeakListVisitor;

template <class T>
Object VisitWeakList(Heap* heap, Object list, WeakObjectRetainer* retainer) {
  Object undefined = ReadOnlyRoots(heap).undefined_value();
  Object head = undefined;
  T tail;
  bool record_slots = MustRecordSlots(heap);

  while (list != undefined) {
    // Check whether to keep the candidate in the list.
    T candidate = T::cast(list);

    Object retained = retainer->RetainAs(list);

    // Move to the next element before the WeakNext is cleared.
    list = WeakListVisitor<T>::WeakNext(candidate);

    if (retained != Object()) {
      if (head == undefined) {
        // First element in the list.
        head = retained;
      } else {
        // Subsequent elements in the list.
        DCHECK(!tail.is_null());
        WeakListVisitor<T>::SetWeakNext(tail, retained);
        if (record_slots) {
          HeapObject slot_holder = WeakListVisitor<T>::WeakNextHolder(tail);
          int slot_offset = WeakListVisitor<T>::WeakNextOffset();
          ObjectSlot slot = slot_holder.RawField(slot_offset);
          MarkCompactCollector::RecordSlot(slot_holder, slot,
                                           HeapObject::cast(retained));
        }
      }
      // Retained object is new tail.
      DCHECK(!retained.IsUndefined(heap->isolate()));
      candidate = T::cast(retained);
      tail = candidate;

      // tail is a live object, visit it.
      WeakListVisitor<T>::VisitLiveObject(heap, tail, retainer);

    } else {
      WeakListVisitor<T>::VisitPhantomObject(heap, candidate);
    }
  }

  // Terminate the list if there is one or more elements.
  if (!tail.is_null()) WeakListVisitor<T>::SetWeakNext(tail, undefined);
  return head;
}

template <class T>
static void ClearWeakList(Heap* heap, Object list) {
  Object undefined = ReadOnlyRoots(heap).undefined_value();
  while (list != undefined) {
    T candidate = T::cast(list);
    list = WeakListVisitor<T>::WeakNext(candidate);
    WeakListVisitor<T>::SetWeakNext(candidate, undefined);
  }
}

template <>
struct WeakListVisitor<CodeT> {
  static void SetWeakNext(CodeT code, Object next) {
    CodeDataContainerFromCodeT(code).set_next_code_link(
        next, UPDATE_WEAK_WRITE_BARRIER);
  }

  static Object WeakNext(CodeT code) {
    return CodeDataContainerFromCodeT(code).next_code_link();
  }

  static HeapObject WeakNextHolder(CodeT code) {
    return CodeDataContainerFromCodeT(code);
  }

  static int WeakNextOffset() { return CodeDataContainer::kNextCodeLinkOffset; }

  static void VisitLiveObject(Heap*, CodeT, WeakObjectRetainer*) {}

  static void VisitPhantomObject(Heap* heap, CodeT code) {
    // Even though the code is dying, its code_data_container can still be
    // alive. Clear the next_code_link slot to avoid a dangling pointer.
    SetWeakNext(code, ReadOnlyRoots(heap).undefined_value());
  }
};

template <>
struct WeakListVisitor<Context> {
  static void SetWeakNext(Context context, Object next) {
    context.set(Context::NEXT_CONTEXT_LINK, next, UPDATE_WEAK_WRITE_BARRIER);
  }

  static Object WeakNext(Context context) {
    return context.next_context_link();
  }

  static HeapObject WeakNextHolder(Context context) { return context; }

  static int WeakNextOffset() {
    return FixedArray::SizeFor(Context::NEXT_CONTEXT_LINK);
  }

  static void VisitLiveObject(Heap* heap, Context context,
                              WeakObjectRetainer* retainer) {
    if (heap->gc_state() == Heap::MARK_COMPACT) {
      // Record the slots of the weak entries in the native context.
      for (int idx = Context::FIRST_WEAK_SLOT;
           idx < Context::NATIVE_CONTEXT_SLOTS; ++idx) {
        ObjectSlot slot = context.RawField(Context::OffsetOfElementAt(idx));
        MarkCompactCollector::RecordSlot(context, slot,
                                         HeapObject::cast(*slot));
      }
      // Code objects are always allocated in Code space, we do not have to
      // visit them during scavenges.
      DoWeakList<CodeT>(heap, context, retainer, Context::OPTIMIZED_CODE_LIST);
      DoWeakList<CodeT>(heap, context, retainer,
                        Context::DEOPTIMIZED_CODE_LIST);
    }
  }

  template <class T>
  static void DoWeakList(Heap* heap, Context context,
                         WeakObjectRetainer* retainer, int index) {
    // Visit the weak list, removing dead intermediate elements.
    Object list_head = VisitWeakList<T>(heap, context.get(index), retainer);

    // Update the list head.
    context.set(index, list_head, UPDATE_WRITE_BARRIER);

    if (MustRecordSlots(heap)) {
      // Record the updated slot if necessary.
      ObjectSlot head_slot = context.RawField(FixedArray::SizeFor(index));
      heap->mark_compact_collector()->RecordSlot(context, head_slot,
                                                 HeapObject::cast(list_head));
    }
  }

  static void VisitPhantomObject(Heap* heap, Context context) {
    ClearWeakList<CodeT>(heap, context.get(Context::OPTIMIZED_CODE_LIST));
    ClearWeakList<CodeT>(heap, context.get(Context::DEOPTIMIZED_CODE_LIST));
  }
};


template <>
struct WeakListVisitor<AllocationSite> {
  static void SetWeakNext(AllocationSite obj, Object next) {
    obj.set_weak_next(next, UPDATE_WEAK_WRITE_BARRIER);
  }

  static Object WeakNext(AllocationSite obj) { return obj.weak_next(); }

  static HeapObject WeakNextHolder(AllocationSite obj) { return obj; }

  static int WeakNextOffset() { return AllocationSite::kWeakNextOffset; }

  static void VisitLiveObject(Heap*, AllocationSite, WeakObjectRetainer*) {}

  static void VisitPhantomObject(Heap*, AllocationSite) {}
};

template <>
struct WeakListVisitor<JSFinalizationRegistry> {
  static void SetWeakNext(JSFinalizationRegistry obj, Object next) {
    obj.set_next_dirty(next, UPDATE_WEAK_WRITE_BARRIER);
  }

  static Object WeakNext(JSFinalizationRegistry obj) {
    return obj.next_dirty();
  }

  static HeapObject WeakNextHolder(JSFinalizationRegistry obj) { return obj; }

  static int WeakNextOffset() {
    return JSFinalizationRegistry::kNextDirtyOffset;
  }

  static void VisitLiveObject(Heap* heap, JSFinalizationRegistry obj,
                              WeakObjectRetainer*) {
    heap->set_dirty_js_finalization_registries_list_tail(obj);
  }

  static void VisitPhantomObject(Heap*, JSFinalizationRegistry) {}
};

template Object VisitWeakList<Context>(Heap* heap, Object list,
                                       WeakObjectRetainer* retainer);

template Object VisitWeakList<AllocationSite>(Heap* heap, Object list,
                                              WeakObjectRetainer* retainer);

template Object VisitWeakList<JSFinalizationRegistry>(
    Heap* heap, Object list, WeakObjectRetainer* retainer);
}  // namespace internal
}  // namespace v8
