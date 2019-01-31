// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_JS_WEAK_REFS_INL_H_
#define V8_OBJECTS_JS_WEAK_REFS_INL_H_

#include "src/objects/js-weak-refs.h"

#include "src/api-inl.h"
#include "src/heap/heap-write-barrier-inl.h"
#include "src/objects/microtask-inl.h"
#include "src/objects/smi-inl.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

OBJECT_CONSTRUCTORS_IMPL(JSWeakCell, JSObject)
OBJECT_CONSTRUCTORS_IMPL(JSWeakRef, JSObject)
OBJECT_CONSTRUCTORS_IMPL(JSWeakFactory, JSObject)
OBJECT_CONSTRUCTORS_IMPL(JSWeakFactoryCleanupIterator, JSObject)
OBJECT_CONSTRUCTORS_IMPL(WeakFactoryCleanupJobTask, Microtask)

ACCESSORS(JSWeakFactory, native_context, NativeContext, kNativeContextOffset)
ACCESSORS(JSWeakFactory, cleanup, Object, kCleanupOffset)
ACCESSORS(JSWeakFactory, active_cells, Object, kActiveCellsOffset)
ACCESSORS(JSWeakFactory, cleared_cells, Object, kClearedCellsOffset)
SMI_ACCESSORS(JSWeakFactory, flags, kFlagsOffset)
ACCESSORS(JSWeakFactory, next, Object, kNextOffset)
CAST_ACCESSOR(JSWeakFactory)

ACCESSORS(JSWeakCell, factory, Object, kFactoryOffset)
ACCESSORS(JSWeakCell, target, Object, kTargetOffset)
ACCESSORS(JSWeakCell, holdings, Object, kHoldingsOffset)
ACCESSORS(JSWeakCell, next, Object, kNextOffset)
ACCESSORS(JSWeakCell, prev, Object, kPrevOffset)
CAST_ACCESSOR(JSWeakCell)

CAST_ACCESSOR(JSWeakRef)
ACCESSORS(JSWeakRef, target, Object, kTargetOffset)

ACCESSORS(JSWeakFactoryCleanupIterator, factory, JSWeakFactory, kFactoryOffset)
CAST_ACCESSOR(JSWeakFactoryCleanupIterator)

ACCESSORS(WeakFactoryCleanupJobTask, factory, JSWeakFactory, kFactoryOffset)
CAST_ACCESSOR(WeakFactoryCleanupJobTask)

void JSWeakFactory::AddWeakCell(JSWeakCell weak_cell) {
  weak_cell->set_factory(*this);
  weak_cell->set_next(active_cells());
  if (active_cells()->IsJSWeakCell()) {
    JSWeakCell::cast(active_cells())->set_prev(weak_cell);
  }
  set_active_cells(weak_cell);
}

bool JSWeakFactory::NeedsCleanup() const {
  return cleared_cells()->IsJSWeakCell();
}

bool JSWeakFactory::scheduled_for_cleanup() const {
  return ScheduledForCleanupField::decode(flags());
}

void JSWeakFactory::set_scheduled_for_cleanup(bool scheduled_for_cleanup) {
  set_flags(ScheduledForCleanupField::update(flags(), scheduled_for_cleanup));
}

JSWeakCell JSWeakFactory::PopClearedCell(Isolate* isolate) {
  JSWeakCell weak_cell = JSWeakCell::cast(cleared_cells());
  DCHECK(weak_cell->prev()->IsUndefined(isolate));
  set_cleared_cells(weak_cell->next());
  weak_cell->set_next(ReadOnlyRoots(isolate).undefined_value());

  if (cleared_cells()->IsJSWeakCell()) {
    JSWeakCell cleared_cells_head = JSWeakCell::cast(cleared_cells());
    DCHECK_EQ(cleared_cells_head->prev(), weak_cell);
    cleared_cells_head->set_prev(ReadOnlyRoots(isolate).undefined_value());
  } else {
    DCHECK(cleared_cells()->IsUndefined(isolate));
  }
  return weak_cell;
}

void JSWeakCell::Nullify(
    Isolate* isolate,
    std::function<void(HeapObject object, ObjectSlot slot, Object target)>
        gc_notify_updated_slot) {
  DCHECK(target()->IsJSReceiver());
  set_target(ReadOnlyRoots(isolate).undefined_value());

  JSWeakFactory weak_factory = JSWeakFactory::cast(factory());
  // Remove from the JSWeakCell from the "active_cells" list of its
  // JSWeakFactory and insert it into the "cleared" list.
  if (prev()->IsJSWeakCell()) {
    DCHECK_NE(weak_factory->active_cells(), *this);
    JSWeakCell prev_cell = JSWeakCell::cast(prev());
    prev_cell->set_next(next());
    gc_notify_updated_slot(prev_cell,
                           prev_cell.RawField(JSWeakCell::kNextOffset), next());
  } else {
    DCHECK_EQ(weak_factory->active_cells(), *this);
    weak_factory->set_active_cells(next());
    gc_notify_updated_slot(
        weak_factory, weak_factory.RawField(JSWeakFactory::kActiveCellsOffset),
        next());
  }
  if (next()->IsJSWeakCell()) {
    JSWeakCell next_cell = JSWeakCell::cast(next());
    next_cell->set_prev(prev());
    gc_notify_updated_slot(next_cell,
                           next_cell.RawField(JSWeakCell::kPrevOffset), prev());
  }

  set_prev(ReadOnlyRoots(isolate).undefined_value());
  Object cleared_head = weak_factory->cleared_cells();
  if (cleared_head->IsJSWeakCell()) {
    JSWeakCell cleared_head_cell = JSWeakCell::cast(cleared_head);
    cleared_head_cell->set_prev(*this);
    gc_notify_updated_slot(cleared_head_cell,
                           cleared_head_cell.RawField(JSWeakCell::kPrevOffset),
                           *this);
  }
  set_next(weak_factory->cleared_cells());
  gc_notify_updated_slot(*this, RawField(JSWeakCell::kNextOffset), next());
  weak_factory->set_cleared_cells(*this);
  gc_notify_updated_slot(
      weak_factory, weak_factory.RawField(JSWeakFactory::kClearedCellsOffset),
      *this);
}

void JSWeakCell::Clear(Isolate* isolate) {
  // Unlink the JSWeakCell from the list it's in (if any). The JSWeakCell can be
  // in its JSWeakFactory's active_cells list, cleared_cells list or neither (if
  // it has been already taken out).

  DCHECK(target()->IsUndefined() || target()->IsJSReceiver());
  set_target(ReadOnlyRoots(isolate).undefined_value());

  if (factory()->IsJSWeakFactory()) {
    JSWeakFactory weak_factory = JSWeakFactory::cast(factory());
    if (weak_factory->active_cells() == *this) {
      DCHECK(!prev()->IsJSWeakCell());
      weak_factory->set_active_cells(next());
    } else if (weak_factory->cleared_cells() == *this) {
      DCHECK(!prev()->IsJSWeakCell());
      weak_factory->set_cleared_cells(next());
    } else if (prev()->IsJSWeakCell()) {
      JSWeakCell prev_cell = JSWeakCell::cast(prev());
      prev_cell->set_next(next());
    }
    if (next()->IsJSWeakCell()) {
      JSWeakCell next_cell = JSWeakCell::cast(next());
      next_cell->set_prev(prev());
    }
    set_prev(ReadOnlyRoots(isolate).undefined_value());
    set_next(ReadOnlyRoots(isolate).undefined_value());

    set_holdings(ReadOnlyRoots(isolate).undefined_value());
    set_factory(ReadOnlyRoots(isolate).undefined_value());
  } else {
    // Already cleared.
    DCHECK(next()->IsUndefined(isolate));
    DCHECK(prev()->IsUndefined(isolate));
    DCHECK(holdings()->IsUndefined(isolate));
    DCHECK(factory()->IsUndefined(isolate));
  }
}

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_JS_WEAK_REFS_INL_H_
