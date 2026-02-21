// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_JS_WEAK_REFS_INL_H_
#define V8_OBJECTS_JS_WEAK_REFS_INL_H_

#include "src/objects/js-weak-refs.h"
// Include the non-inl header before the rest of the headers.

#include "src/api/api-inl.h"
#include "src/heap/heap-layout-inl.h"
#include "src/heap/heap-write-barrier-inl.h"
#include "src/objects/casting-inl.h"
#include "src/objects/smi-inl.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

#include "torque-generated/src/objects/js-weak-refs-tq-inl.inc"

TQ_OBJECT_CONSTRUCTORS_IMPL(JSWeakRef)
TQ_OBJECT_CONSTRUCTORS_IMPL(JSFinalizationRegistry)

BIT_FIELD_ACCESSORS(JSFinalizationRegistry, flags, scheduled_for_cleanup,
                    JSFinalizationRegistry::ScheduledForCleanupBit)

void JSFinalizationRegistry::RegisterWeakCellWithUnregisterToken(
    DirectHandle<JSFinalizationRegistry> finalization_registry,
    DirectHandle<WeakCell> weak_cell, Isolate* isolate) {
  Handle<SimpleNumberDictionary> key_map;
  if (IsUndefined(finalization_registry->key_map(), isolate)) {
    key_map = SimpleNumberDictionary::New(isolate, 1);
  } else {
    key_map =
        handle(Cast<SimpleNumberDictionary>(finalization_registry->key_map()),
               isolate);
  }

  // Unregister tokens are held weakly as objects are often their own
  // unregister token. To avoid using an ephemeron map, the map for token
  // lookup is keyed on the token's identity hash instead of the token itself.
  uint32_t key =
      Object::GetOrCreateHash(weak_cell->unregister_token(), isolate).value();
  InternalIndex entry = key_map->FindEntry(isolate, key);
  if (entry.is_found()) {
    Tagged<Object> value = key_map->ValueAt(entry);
    Tagged<WeakCell> existing_weak_cell = Cast<WeakCell>(value);
    existing_weak_cell->set_key_list_prev(*weak_cell);
    weak_cell->set_key_list_next(existing_weak_cell);
  }
  key_map = SimpleNumberDictionary::Set(isolate, key_map, key, weak_cell);
  finalization_registry->set_key_map(*key_map);
}

bool JSFinalizationRegistry::Unregister(
    DirectHandle<JSFinalizationRegistry> finalization_registry,
    DirectHandle<HeapObject> unregister_token, Isolate* isolate) {
  // Iterate through the doubly linked list of WeakCells associated with the
  // key. Each WeakCell will be in the "active_cells" or "cleared_cells" list of
  // its FinalizationRegistry; remove it from there.
  return finalization_registry->RemoveUnregisterToken(
      *unregister_token, isolate, kRemoveMatchedCellsFromRegistry,
      [](Tagged<HeapObject>, ObjectSlot, Tagged<Object>) {});
}

template <typename GCNotifyUpdatedSlotCallback>
bool JSFinalizationRegistry::RemoveUnregisterToken(
    Tagged<HeapObject> unregister_token, Isolate* isolate,
    RemoveUnregisterTokenMode removal_mode,
    GCNotifyUpdatedSlotCallback gc_notify_updated_slot) {
  // This method is called from both FinalizationRegistry#unregister and for
  // removing weakly-held dead unregister tokens. The latter is during GC so
  // this function cannot GC.
  DisallowGarbageCollection no_gc;
  Tagged<HeapObject> maybe_key_map =
      Cast<HeapObject>(RawField(kKeyMapOffset).load());
  if (IsUndefined(maybe_key_map, isolate)) {
    return false;
  }

  const Heap* const heap = isolate->heap();
  Tagged<SimpleNumberDictionary> key_map =
      GCSafeCast<SimpleNumberDictionary>(maybe_key_map, heap);
  // If the token doesn't have a hash, it was not used as a key inside any hash
  // tables.
  Tagged<Object> hash = Object::GetHash(unregister_token);
  if (IsUndefined(hash, isolate)) {
    return false;
  }
  uint32_t key = Smi::ToInt(hash);
  InternalIndex entry = key_map->FindEntry(isolate, key);
  if (entry.is_not_found()) {
    return false;
  }

  Tagged<Object> value = key_map->ValueAt(entry);
  bool was_present = false;
  Tagged<Undefined> undefined = ReadOnlyRoots(isolate).undefined_value();
  Tagged<UnionOf<Undefined, WeakCell>> new_key_list_head = undefined;
  Tagged<UnionOf<Undefined, WeakCell>> new_key_list_prev = undefined;
  // Compute a new key list that doesn't have unregister_token. Because
  // unregister tokens are held weakly, key_map is keyed using the tokens'
  // identity hashes, and identity hashes may collide.
  while (!IsUndefined(value, isolate)) {
    Tagged<WeakCell> weak_cell = GCSafeCast<WeakCell>(value, heap);
    value = weak_cell->key_list_next();
    if (weak_cell->unregister_token() == unregister_token) {
      // weak_cell has the same unregister token; remove it from the key list.
      switch (removal_mode) {
        case kRemoveMatchedCellsFromRegistry:
          weak_cell->RemoveFromFinalizationRegistryCells(isolate);
          break;
        case kKeepMatchedCellsInRegistry:
          // Do nothing.
          break;
      }
      // Clear unregister token-related fields.
      weak_cell->set_unregister_token(undefined);
      weak_cell->set_key_list_prev(undefined);
      weak_cell->set_key_list_next(undefined);
      was_present = true;
    } else {
      // weak_cell has a different unregister token with the same key (hash
      // collision); fix up the list.
      weak_cell->set_key_list_prev(new_key_list_prev);
      gc_notify_updated_slot(weak_cell, ObjectSlot(&weak_cell->key_list_prev_),
                             new_key_list_prev);
      weak_cell->set_key_list_next(undefined);
      if (IsUndefined(new_key_list_prev, isolate)) {
        new_key_list_head = weak_cell;
      } else {
        Tagged<WeakCell> prev_cell =
            GCSafeCast<WeakCell>(new_key_list_prev, heap);
        prev_cell->set_key_list_next(weak_cell);
        gc_notify_updated_slot(
            prev_cell, ObjectSlot(&prev_cell->key_list_next_), weak_cell);
      }
      new_key_list_prev = weak_cell;
    }
  }
  if (IsUndefined(new_key_list_head, isolate)) {
    DCHECK(was_present);
    key_map->ClearEntry(entry);
    key_map->ElementRemoved();
  } else {
    key_map->ValueAtPut(entry, new_key_list_head);
    gc_notify_updated_slot(key_map, key_map->RawFieldOfValueAt(entry),
                           new_key_list_head);
  }
  return was_present;
}

bool JSFinalizationRegistry::NeedsCleanup() const {
  return IsWeakCell(cleared_cells());
}

void JSFinalizationRegistry::set_next_dirty_unchecked(
    Tagged<JSFinalizationRegistry> value, WriteBarrierMode mode) {
  WRITE_FIELD(*this, kNextDirtyOffset, value);
  CONDITIONAL_WRITE_BARRIER(*this, kNextDirtyOffset, value, mode);
}

void JSFinalizationRegistry::set_active_cells_unchecked(
    Tagged<Union<Undefined, WeakCell>> value, WriteBarrierMode mode) {
  WRITE_FIELD(*this, kActiveCellsOffset, value);
  CONDITIONAL_WRITE_BARRIER(*this, kActiveCellsOffset, value, mode);
}

void JSFinalizationRegistry::set_cleared_cells_unchecked(
    Tagged<WeakCell> value, WriteBarrierMode mode) {
  WRITE_FIELD(*this, kClearedCellsOffset, value);
  CONDITIONAL_WRITE_BARRIER(*this, kClearedCellsOffset, value, mode);
}

Tagged<JSFinalizationRegistry> WeakCell::finalization_registry() const {
  return finalization_registry_.load();
}
void WeakCell::set_finalization_registry(Tagged<JSFinalizationRegistry> value,
                                         WriteBarrierMode mode) {
  // `finalization_registry_` is never updated after it is initialized.
  DCHECK_EQ(finalization_registry_.ptr(), kNullAddress);
  finalization_registry_.store(this, value, mode);
}

Tagged<JSAny> WeakCell::holdings() const { return holdings_.load(); }
void WeakCell::set_holdings(Tagged<JSAny> value, WriteBarrierMode mode) {
  // `holdings_` is never updated after it is initialized.
  DCHECK_EQ(holdings_.ptr(), kNullAddress);
  holdings_.store(this, value, mode);
}

Tagged<UnionOf<Symbol, JSReceiver, Undefined>> WeakCell::target() const {
  return target_.load();
}
void WeakCell::set_target(
    Tagged<UnionOf<Symbol, JSReceiver, Undefined>> value) {
  // `target_` is set once during initialization to a non-undefined object.
  // After that, it can only be set to Undefined or for updating pointers to a
  // forwarding address.
  DCHECK_IMPLIES(!IsUndefined(value),
                 HeapLayout::IsForwardedPointerTo(target(), value));
  target_.store_no_write_barrier(value);
}

Tagged<UnionOf<Symbol, JSReceiver, Undefined>> WeakCell::unregister_token()
    const {
  return unregister_token_.load();
}
void WeakCell::set_unregister_token(
    Tagged<UnionOf<Symbol, JSReceiver, Undefined>> value) {
  // `unregister_token_` is set once during initialization to a non-undefined
  // object. After that, it can only be set to Undefined or for updating
  // pointers to a forwarding address.
  DCHECK_IMPLIES(!IsUndefined(value),
                 HeapLayout::IsForwardedPointerTo(unregister_token(), value));
  unregister_token_.store_no_write_barrier(value);
}

Tagged<UnionOf<WeakCell, Undefined>> WeakCell::prev() const {
  return prev_.load();
}
void WeakCell::set_prev(Tagged<UnionOf<WeakCell, Undefined>> value,
                        WriteBarrierMode mode) {
  prev_.store(this, value, mode);
}

Tagged<UnionOf<WeakCell, Undefined>> WeakCell::next() const {
  return next_.load();
}
void WeakCell::set_next(Tagged<UnionOf<WeakCell, Undefined>> value,
                        WriteBarrierMode mode) {
  next_.store(this, value, mode);
}

Tagged<UnionOf<WeakCell, Undefined>> WeakCell::key_list_prev() const {
  return key_list_prev_.load();
}
void WeakCell::set_key_list_prev(Tagged<UnionOf<WeakCell, Undefined>> value,
                                 WriteBarrierMode mode) {
  key_list_prev_.store(this, value, mode);
}

Tagged<UnionOf<WeakCell, Undefined>> WeakCell::key_list_next() const {
  return key_list_next_.load();
}
void WeakCell::set_key_list_next(Tagged<UnionOf<WeakCell, Undefined>> value,
                                 WriteBarrierMode mode) {
  key_list_next_.store(this, value, mode);
}

Tagged<HeapObject> WeakCell::relaxed_target() const {
  return target_.Relaxed_Load();
}

Tagged<HeapObject> WeakCell::relaxed_unregister_token() const {
  return unregister_token_.Relaxed_Load();
}

template <typename GCNotifyUpdatedSlotCallback>
void WeakCell::GCSafeNullify(
    Isolate* isolate, GCNotifyUpdatedSlotCallback gc_notify_updated_slot) {
  // Remove from the WeakCell from the "active_cells" list of its
  // JSFinalizationRegistry and insert it into the "cleared_cells" list. This is
  // only called for WeakCells which haven't been unregistered yet, so they will
  // be in the active_cells list. (The caller must guard against calling this
  // for unregistered WeakCells by checking that the target is not undefined.)
  DCHECK(Object::CanBeHeldWeakly(target()));
  set_target(ReadOnlyRoots(isolate).undefined_value());

  const Heap* const heap = isolate->heap();
  Tagged<JSFinalizationRegistry> fr =
      GCSafeCast<JSFinalizationRegistry>(finalization_registry(), heap);
#ifdef DEBUG
  Tagged<WeakCell> current_active_cells = GCSafeCast<WeakCell>(
      fr->RawField(JSFinalizationRegistry::kActiveCellsOffset).load(),
      isolate->heap());
#endif  // DEBUG
  if (!IsUndefined(prev())) {
    DCHECK_NE(current_active_cells, this);
    Tagged<WeakCell> prev_cell = GCSafeCast<WeakCell>(prev(), heap);
    prev_cell->set_next(next());
    gc_notify_updated_slot(prev_cell, ObjectSlot(&prev_cell->next_), next());
  } else {
    DCHECK_EQ(current_active_cells, this);
    fr->set_active_cells_unchecked(next());
    gc_notify_updated_slot(
        fr, fr->RawField(JSFinalizationRegistry::kActiveCellsOffset), next());
  }
  if (!IsUndefined(next())) {
    Tagged<WeakCell> next_cell = GCSafeCast<WeakCell>(next(), heap);
    next_cell->set_prev(prev());
    gc_notify_updated_slot(next_cell, ObjectSlot(&next_cell->prev_), prev());
  }

  set_prev(ReadOnlyRoots(isolate).undefined_value());
  Tagged<UnionOf<Undefined, WeakCell>> cleared_head = fr->cleared_cells();
  if (!IsUndefined(cleared_head)) {
    Tagged<WeakCell> cleared_head_cell =
        GCSafeCast<WeakCell>(cleared_head, heap);
    cleared_head_cell->set_prev(Tagged(this));
    gc_notify_updated_slot(cleared_head_cell,
                           ObjectSlot(&cleared_head_cell->prev_), this);
  }
  set_next(GCSafeCast<UnionOf<WeakCell, Undefined>>(
      fr->RawField(JSFinalizationRegistry::kClearedCellsOffset).load(),
      isolate->heap()));
  gc_notify_updated_slot(this, ObjectSlot(&next_), next());
  fr->set_cleared_cells_unchecked(Tagged(this));
  gc_notify_updated_slot(
      fr, fr->RawField(JSFinalizationRegistry::kClearedCellsOffset), this);
}

void WeakCell::RemoveFromFinalizationRegistryCells(Isolate* isolate) {
  // Remove the WeakCell from the list it's in (either "active_cells" or
  // "cleared_cells" of its JSFinalizationRegistry).

  // It's important to set_target to undefined here. This guards that we won't
  // call Nullify (which assumes that the WeakCell is in active_cells).
  DCHECK(IsUndefined(target()) || Object::CanBeHeldWeakly(target()));
  set_target(ReadOnlyRoots(isolate).undefined_value());

  Tagged<JSFinalizationRegistry> fr =
      Cast<JSFinalizationRegistry>(finalization_registry());
  if (fr->active_cells() == this) {
    DCHECK(IsUndefined(prev(), isolate));
    fr->set_active_cells(next());
  } else if (fr->cleared_cells() == this) {
    DCHECK(!IsWeakCell(prev()));
    fr->set_cleared_cells(next());
  } else {
    DCHECK(IsWeakCell(prev()));
    Tagged<WeakCell> prev_cell = Cast<WeakCell>(prev());
    prev_cell->set_next(next());
  }
  if (IsWeakCell(next())) {
    Tagged<WeakCell> next_cell = Cast<WeakCell>(next());
    next_cell->set_prev(prev());
  }
  set_prev(ReadOnlyRoots(isolate).undefined_value());
  set_next(ReadOnlyRoots(isolate).undefined_value());
}

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_JS_WEAK_REFS_INL_H_
