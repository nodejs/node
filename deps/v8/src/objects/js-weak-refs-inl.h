// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_JS_WEAK_REFS_INL_H_
#define V8_OBJECTS_JS_WEAK_REFS_INL_H_

#include "src/objects/js-weak-refs.h"

#include "src/api/api-inl.h"
#include "src/heap/heap-write-barrier-inl.h"
#include "src/objects/smi-inl.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

#include "torque-generated/src/objects/js-weak-refs-tq-inl.inc"

TQ_OBJECT_CONSTRUCTORS_IMPL(WeakCell)
TQ_OBJECT_CONSTRUCTORS_IMPL(JSWeakRef)
OBJECT_CONSTRUCTORS_IMPL(JSFinalizationRegistry, JSObject)

ACCESSORS(JSFinalizationRegistry, native_context, NativeContext,
          kNativeContextOffset)
ACCESSORS(JSFinalizationRegistry, cleanup, Object, kCleanupOffset)
ACCESSORS(JSFinalizationRegistry, active_cells, HeapObject, kActiveCellsOffset)
ACCESSORS(JSFinalizationRegistry, cleared_cells, HeapObject,
          kClearedCellsOffset)
ACCESSORS(JSFinalizationRegistry, key_map, Object, kKeyMapOffset)
SMI_ACCESSORS(JSFinalizationRegistry, flags, kFlagsOffset)
ACCESSORS(JSFinalizationRegistry, next_dirty, Object, kNextDirtyOffset)
CAST_ACCESSOR(JSFinalizationRegistry)

BIT_FIELD_ACCESSORS(JSFinalizationRegistry, flags, scheduled_for_cleanup,
                    JSFinalizationRegistry::ScheduledForCleanupBit)

void JSFinalizationRegistry::RegisterWeakCellWithUnregisterToken(
    Handle<JSFinalizationRegistry> finalization_registry,
    Handle<WeakCell> weak_cell, Isolate* isolate) {
  Handle<SimpleNumberDictionary> key_map;
  if (finalization_registry->key_map().IsUndefined(isolate)) {
    key_map = SimpleNumberDictionary::New(isolate, 1);
  } else {
    key_map =
        handle(SimpleNumberDictionary::cast(finalization_registry->key_map()),
               isolate);
  }

  // Unregister tokens are held weakly as objects are often their own
  // unregister token. To avoid using an ephemeron map, the map for token
  // lookup is keyed on the token's identity hash instead of the token itself.
  uint32_t key = weak_cell->unregister_token().GetOrCreateHash(isolate).value();
  InternalIndex entry = key_map->FindEntry(isolate, key);
  if (entry.is_found()) {
    Object value = key_map->ValueAt(entry);
    WeakCell existing_weak_cell = WeakCell::cast(value);
    existing_weak_cell.set_key_list_prev(*weak_cell);
    weak_cell->set_key_list_next(existing_weak_cell);
  }
  key_map = SimpleNumberDictionary::Set(isolate, key_map, key, weak_cell);
  finalization_registry->set_key_map(*key_map);
}

bool JSFinalizationRegistry::Unregister(
    Handle<JSFinalizationRegistry> finalization_registry,
    Handle<JSReceiver> unregister_token, Isolate* isolate) {
  // Iterate through the doubly linked list of WeakCells associated with the
  // key. Each WeakCell will be in the "active_cells" or "cleared_cells" list of
  // its FinalizationRegistry; remove it from there.
  return finalization_registry->RemoveUnregisterToken(
      *unregister_token, isolate,
      [isolate](WeakCell matched_cell) {
        matched_cell.RemoveFromFinalizationRegistryCells(isolate);
      },
      [](HeapObject, ObjectSlot, Object) {});
}

template <typename MatchCallback, typename GCNotifyUpdatedSlotCallback>
bool JSFinalizationRegistry::RemoveUnregisterToken(
    JSReceiver unregister_token, Isolate* isolate, MatchCallback match_callback,
    GCNotifyUpdatedSlotCallback gc_notify_updated_slot) {
  // This method is called from both FinalizationRegistry#unregister and for
  // removing weakly-held dead unregister tokens. The latter is during GC so
  // this function cannot GC.
  DisallowGarbageCollection no_gc;
  if (key_map().IsUndefined(isolate)) {
    return false;
  }

  SimpleNumberDictionary key_map =
      SimpleNumberDictionary::cast(this->key_map());
  // If the token doesn't have a hash, it was not used as a key inside any hash
  // tables.
  Object hash = unregister_token.GetHash();
  if (hash.IsUndefined(isolate)) {
    return false;
  }
  uint32_t key = Smi::ToInt(hash);
  InternalIndex entry = key_map.FindEntry(isolate, key);
  if (entry.is_not_found()) {
    return false;
  }

  Object value = key_map.ValueAt(entry);
  bool was_present = false;
  HeapObject undefined = ReadOnlyRoots(isolate).undefined_value();
  HeapObject new_key_list_head = undefined;
  HeapObject new_key_list_prev = undefined;
  // Compute a new key list that doesn't have unregister_token. Because
  // unregister tokens are held weakly, key_map is keyed using the tokens'
  // identity hashes, and identity hashes may collide.
  while (!value.IsUndefined(isolate)) {
    WeakCell weak_cell = WeakCell::cast(value);
    DCHECK(!ObjectInYoungGeneration(weak_cell));
    value = weak_cell.key_list_next();
    if (weak_cell.unregister_token() == unregister_token) {
      // weak_cell has the same unregister token; remove it from the key list.
      match_callback(weak_cell);
      weak_cell.set_key_list_prev(undefined);
      weak_cell.set_key_list_next(undefined);
      was_present = true;
    } else {
      // weak_cell has a different unregister token with the same key (hash
      // collision); fix up the list.
      weak_cell.set_key_list_prev(new_key_list_prev);
      gc_notify_updated_slot(weak_cell,
                             weak_cell.RawField(WeakCell::kKeyListPrevOffset),
                             new_key_list_prev);
      weak_cell.set_key_list_next(undefined);
      if (new_key_list_prev.IsUndefined(isolate)) {
        new_key_list_head = weak_cell;
      } else {
        DCHECK(new_key_list_head.IsWeakCell());
        WeakCell prev_cell = WeakCell::cast(new_key_list_prev);
        prev_cell.set_key_list_next(weak_cell);
        gc_notify_updated_slot(prev_cell,
                               prev_cell.RawField(WeakCell::kKeyListNextOffset),
                               weak_cell);
      }
      new_key_list_prev = weak_cell;
    }
  }
  if (new_key_list_head.IsUndefined(isolate)) {
    DCHECK(was_present);
    key_map.ClearEntry(entry);
    key_map.ElementRemoved();
  } else {
    key_map.ValueAtPut(entry, new_key_list_head);
    gc_notify_updated_slot(key_map, key_map.RawFieldOfValueAt(entry),
                           new_key_list_head);
  }
  return was_present;
}

bool JSFinalizationRegistry::NeedsCleanup() const {
  return cleared_cells().IsWeakCell();
}

HeapObject WeakCell::relaxed_target() const {
  return TaggedField<HeapObject>::Relaxed_Load(*this, kTargetOffset);
}

template <typename GCNotifyUpdatedSlotCallback>
void WeakCell::Nullify(Isolate* isolate,
                       GCNotifyUpdatedSlotCallback gc_notify_updated_slot) {
  // Remove from the WeakCell from the "active_cells" list of its
  // JSFinalizationRegistry and insert it into the "cleared_cells" list. This is
  // only called for WeakCells which haven't been unregistered yet, so they will
  // be in the active_cells list. (The caller must guard against calling this
  // for unregistered WeakCells by checking that the target is not undefined.)
  DCHECK(target().IsJSReceiver());
  set_target(ReadOnlyRoots(isolate).undefined_value());

  JSFinalizationRegistry fr =
      JSFinalizationRegistry::cast(finalization_registry());
  if (prev().IsWeakCell()) {
    DCHECK_NE(fr.active_cells(), *this);
    WeakCell prev_cell = WeakCell::cast(prev());
    prev_cell.set_next(next());
    gc_notify_updated_slot(prev_cell, prev_cell.RawField(WeakCell::kNextOffset),
                           next());
  } else {
    DCHECK_EQ(fr.active_cells(), *this);
    fr.set_active_cells(next());
    gc_notify_updated_slot(
        fr, fr.RawField(JSFinalizationRegistry::kActiveCellsOffset), next());
  }
  if (next().IsWeakCell()) {
    WeakCell next_cell = WeakCell::cast(next());
    next_cell.set_prev(prev());
    gc_notify_updated_slot(next_cell, next_cell.RawField(WeakCell::kPrevOffset),
                           prev());
  }

  set_prev(ReadOnlyRoots(isolate).undefined_value());
  Object cleared_head = fr.cleared_cells();
  if (cleared_head.IsWeakCell()) {
    WeakCell cleared_head_cell = WeakCell::cast(cleared_head);
    cleared_head_cell.set_prev(*this);
    gc_notify_updated_slot(cleared_head_cell,
                           cleared_head_cell.RawField(WeakCell::kPrevOffset),
                           *this);
  }
  set_next(fr.cleared_cells());
  gc_notify_updated_slot(*this, RawField(WeakCell::kNextOffset), next());
  fr.set_cleared_cells(*this);
  gc_notify_updated_slot(
      fr, fr.RawField(JSFinalizationRegistry::kClearedCellsOffset), *this);
}

void WeakCell::RemoveFromFinalizationRegistryCells(Isolate* isolate) {
  // Remove the WeakCell from the list it's in (either "active_cells" or
  // "cleared_cells" of its JSFinalizationRegistry).

  // It's important to set_target to undefined here. This guards that we won't
  // call Nullify (which assumes that the WeakCell is in active_cells).
  DCHECK(target().IsUndefined() || target().IsJSReceiver());
  set_target(ReadOnlyRoots(isolate).undefined_value());

  JSFinalizationRegistry fr =
      JSFinalizationRegistry::cast(finalization_registry());
  if (fr.active_cells() == *this) {
    DCHECK(prev().IsUndefined(isolate));
    fr.set_active_cells(next());
  } else if (fr.cleared_cells() == *this) {
    DCHECK(!prev().IsWeakCell());
    fr.set_cleared_cells(next());
  } else {
    DCHECK(prev().IsWeakCell());
    WeakCell prev_cell = WeakCell::cast(prev());
    prev_cell.set_next(next());
  }
  if (next().IsWeakCell()) {
    WeakCell next_cell = WeakCell::cast(next());
    next_cell.set_prev(prev());
  }
  set_prev(ReadOnlyRoots(isolate).undefined_value());
  set_next(ReadOnlyRoots(isolate).undefined_value());
}

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_JS_WEAK_REFS_INL_H_
