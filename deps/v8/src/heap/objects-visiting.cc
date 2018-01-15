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
Object* VisitWeakList(Heap* heap, Object* list, WeakObjectRetainer* retainer) {
  Object* undefined = heap->undefined_value();
  Object* head = undefined;
  T* tail = NULL;
  bool record_slots = MustRecordSlots(heap);

  while (list != undefined) {
    // Check whether to keep the candidate in the list.
    T* candidate = reinterpret_cast<T*>(list);

    Object* retained = retainer->RetainAs(list);
    if (retained != NULL) {
      if (head == undefined) {
        // First element in the list.
        head = retained;
      } else {
        // Subsequent elements in the list.
        DCHECK(tail != NULL);
        WeakListVisitor<T>::SetWeakNext(tail, retained);
        if (record_slots) {
          Object** next_slot =
              HeapObject::RawField(tail, WeakListVisitor<T>::WeakNextOffset());
          MarkCompactCollector::RecordSlot(tail, next_slot, retained);
        }
      }
      // Retained object is new tail.
      DCHECK(!retained->IsUndefined(heap->isolate()));
      candidate = reinterpret_cast<T*>(retained);
      tail = candidate;

      // tail is a live object, visit it.
      WeakListVisitor<T>::VisitLiveObject(heap, tail, retainer);

    } else {
      WeakListVisitor<T>::VisitPhantomObject(heap, candidate);
    }

    // Move to next element in the list.
    list = WeakListVisitor<T>::WeakNext(candidate);
  }

  // Terminate the list if there is one or more elements.
  if (tail != NULL) WeakListVisitor<T>::SetWeakNext(tail, undefined);
  return head;
}


template <class T>
static void ClearWeakList(Heap* heap, Object* list) {
  Object* undefined = heap->undefined_value();
  while (list != undefined) {
    T* candidate = reinterpret_cast<T*>(list);
    list = WeakListVisitor<T>::WeakNext(candidate);
    WeakListVisitor<T>::SetWeakNext(candidate, undefined);
  }
}


template <>
struct WeakListVisitor<JSFunction> {
  static void SetWeakNext(JSFunction* function, Object* next) {
    function->set_next_function_link(next, UPDATE_WEAK_WRITE_BARRIER);
  }

  static Object* WeakNext(JSFunction* function) {
    return function->next_function_link();
  }

  static int WeakNextOffset() { return JSFunction::kNextFunctionLinkOffset; }

  static void VisitLiveObject(Heap*, JSFunction*, WeakObjectRetainer*) {}

  static void VisitPhantomObject(Heap*, JSFunction*) {}
};


template <>
struct WeakListVisitor<Code> {
  static void SetWeakNext(Code* code, Object* next) {
    code->set_next_code_link(next, UPDATE_WEAK_WRITE_BARRIER);
  }

  static Object* WeakNext(Code* code) { return code->next_code_link(); }

  static int WeakNextOffset() { return Code::kNextCodeLinkOffset; }

  static void VisitLiveObject(Heap*, Code*, WeakObjectRetainer*) {}

  static void VisitPhantomObject(Heap*, Code*) {}
};


template <>
struct WeakListVisitor<Context> {
  static void SetWeakNext(Context* context, Object* next) {
    context->set(Context::NEXT_CONTEXT_LINK, next, UPDATE_WEAK_WRITE_BARRIER);
  }

  static Object* WeakNext(Context* context) {
    return context->next_context_link();
  }

  static int WeakNextOffset() {
    return FixedArray::SizeFor(Context::NEXT_CONTEXT_LINK);
  }

  static void VisitLiveObject(Heap* heap, Context* context,
                              WeakObjectRetainer* retainer) {
    // Process the three weak lists linked off the context.
    DoWeakList<JSFunction>(heap, context, retainer,
                           Context::OPTIMIZED_FUNCTIONS_LIST);

    if (heap->gc_state() == Heap::MARK_COMPACT) {
      // Record the slots of the weak entries in the native context.
      for (int idx = Context::FIRST_WEAK_SLOT;
           idx < Context::NATIVE_CONTEXT_SLOTS; ++idx) {
        Object** slot = Context::cast(context)->RawFieldOfElementAt(idx);
        MarkCompactCollector::RecordSlot(context, slot, *slot);
      }
      // Code objects are always allocated in Code space, we do not have to
      // visit
      // them during scavenges.
      DoWeakList<Code>(heap, context, retainer, Context::OPTIMIZED_CODE_LIST);
      DoWeakList<Code>(heap, context, retainer, Context::DEOPTIMIZED_CODE_LIST);
    }
  }

  template <class T>
  static void DoWeakList(Heap* heap, Context* context,
                         WeakObjectRetainer* retainer, int index) {
    // Visit the weak list, removing dead intermediate elements.
    Object* list_head = VisitWeakList<T>(heap, context->get(index), retainer);

    // Update the list head.
    context->set(index, list_head, UPDATE_WRITE_BARRIER);

    if (MustRecordSlots(heap)) {
      // Record the updated slot if necessary.
      Object** head_slot =
          HeapObject::RawField(context, FixedArray::SizeFor(index));
      heap->mark_compact_collector()->RecordSlot(context, head_slot, list_head);
    }
  }

  static void VisitPhantomObject(Heap* heap, Context* context) {
    ClearWeakList<JSFunction>(heap,
                              context->get(Context::OPTIMIZED_FUNCTIONS_LIST));
    ClearWeakList<Code>(heap, context->get(Context::OPTIMIZED_CODE_LIST));
    ClearWeakList<Code>(heap, context->get(Context::DEOPTIMIZED_CODE_LIST));
  }
};


template <>
struct WeakListVisitor<AllocationSite> {
  static void SetWeakNext(AllocationSite* obj, Object* next) {
    obj->set_weak_next(next, UPDATE_WEAK_WRITE_BARRIER);
  }

  static Object* WeakNext(AllocationSite* obj) { return obj->weak_next(); }

  static int WeakNextOffset() { return AllocationSite::kWeakNextOffset; }

  static void VisitLiveObject(Heap*, AllocationSite*, WeakObjectRetainer*) {}

  static void VisitPhantomObject(Heap*, AllocationSite*) {}
};


template Object* VisitWeakList<Context>(Heap* heap, Object* list,
                                        WeakObjectRetainer* retainer);

template Object* VisitWeakList<AllocationSite>(Heap* heap, Object* list,
                                               WeakObjectRetainer* retainer);
}  // namespace internal
}  // namespace v8
