// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/weak-object-worklists.h"

#include "src/heap/heap-inl.h"
#include "src/heap/heap.h"
#include "src/heap/worklist.h"
#include "src/objects/hash-table.h"
#include "src/objects/heap-object.h"
#include "src/objects/js-function.h"
#include "src/objects/js-weak-refs-inl.h"
#include "src/objects/js-weak-refs.h"
#include "src/objects/shared-function-info.h"
#include "src/objects/transitions.h"

namespace v8 {

namespace internal {

void WeakObjects::UpdateAfterScavenge() {
#define INVOKE_UPDATE(_, name, Name) Update##Name(name);
  WEAK_OBJECT_WORKLISTS(INVOKE_UPDATE)
#undef INVOKE_UPDATE
}

void WeakObjects::UpdateTransitionArrays(
    WeakObjectWorklist<TransitionArray>& transition_arrays) {
  DCHECK(!ContainsYoungObjects(transition_arrays));
}

void WeakObjects::UpdateEphemeronHashTables(
    WeakObjectWorklist<EphemeronHashTable>& ephemeron_hash_tables) {
  ephemeron_hash_tables.Update(
      [](EphemeronHashTable slot_in, EphemeronHashTable* slot_out) -> bool {
        EphemeronHashTable forwarded = ForwardingAddress(slot_in);

        if (!forwarded.is_null()) {
          *slot_out = forwarded;
          return true;
        }

        return false;
      });
}

namespace {
bool EphemeronUpdater(Ephemeron slot_in, Ephemeron* slot_out) {
  HeapObject key = slot_in.key;
  HeapObject value = slot_in.value;
  HeapObject forwarded_key = ForwardingAddress(key);
  HeapObject forwarded_value = ForwardingAddress(value);

  if (!forwarded_key.is_null() && !forwarded_value.is_null()) {
    *slot_out = Ephemeron{forwarded_key, forwarded_value};
    return true;
  }

  return false;
}
}  // anonymous namespace

void WeakObjects::UpdateCurrentEphemerons(
    WeakObjectWorklist<Ephemeron>& current_ephemerons) {
  current_ephemerons.Update(EphemeronUpdater);
}

void WeakObjects::UpdateNextEphemerons(
    WeakObjectWorklist<Ephemeron>& next_ephemerons) {
  next_ephemerons.Update(EphemeronUpdater);
}

void WeakObjects::UpdateDiscoveredEphemerons(
    WeakObjectWorklist<Ephemeron>& discovered_ephemerons) {
  discovered_ephemerons.Update(EphemeronUpdater);
}

void WeakObjects::UpdateWeakReferences(
    WeakObjectWorklist<HeapObjectAndSlot>& weak_references) {
  weak_references.Update(
      [](HeapObjectAndSlot slot_in, HeapObjectAndSlot* slot_out) -> bool {
        HeapObject heap_obj = slot_in.first;
        HeapObject forwarded = ForwardingAddress(heap_obj);

        if (!forwarded.is_null()) {
          ptrdiff_t distance_to_slot =
              slot_in.second.address() - slot_in.first.ptr();
          Address new_slot = forwarded.ptr() + distance_to_slot;
          slot_out->first = forwarded;
          slot_out->second = HeapObjectSlot(new_slot);
          return true;
        }

        return false;
      });
}

void WeakObjects::UpdateWeakObjectsInCode(
    WeakObjectWorklist<HeapObjectAndCode>& weak_objects_in_code) {
  weak_objects_in_code.Update(
      [](HeapObjectAndCode slot_in, HeapObjectAndCode* slot_out) -> bool {
        HeapObject heap_obj = slot_in.first;
        HeapObject forwarded = ForwardingAddress(heap_obj);

        if (!forwarded.is_null()) {
          slot_out->first = forwarded;
          slot_out->second = slot_in.second;
          return true;
        }

        return false;
      });
}

void WeakObjects::UpdateJSWeakRefs(
    WeakObjectWorklist<JSWeakRef>& js_weak_refs) {
  if (FLAG_harmony_weak_refs) {
    js_weak_refs.Update(
        [](JSWeakRef js_weak_ref_in, JSWeakRef* js_weak_ref_out) -> bool {
          JSWeakRef forwarded = ForwardingAddress(js_weak_ref_in);

          if (!forwarded.is_null()) {
            *js_weak_ref_out = forwarded;
            return true;
          }

          return false;
        });
  }
}

void WeakObjects::UpdateWeakCells(WeakObjectWorklist<WeakCell>& weak_cells) {
  // TODO(syg, marja): Support WeakCells in the young generation.
  DCHECK(!ContainsYoungObjects(weak_cells));
}

void WeakObjects::UpdateBytecodeFlushingCandidates(
    WeakObjectWorklist<SharedFunctionInfo>& bytecode_flushing_candidates) {
  DCHECK(!ContainsYoungObjects(bytecode_flushing_candidates));
}

void WeakObjects::UpdateFlushedJSFunctions(
    WeakObjectWorklist<JSFunction>& flushed_js_functions) {
  flushed_js_functions.Update(
      [](JSFunction slot_in, JSFunction* slot_out) -> bool {
        JSFunction forwarded = ForwardingAddress(slot_in);

        if (!forwarded.is_null()) {
          *slot_out = forwarded;
          return true;
        }

        return false;
      });
}

#ifdef DEBUG
template <typename Type>
bool WeakObjects::ContainsYoungObjects(WeakObjectWorklist<Type>& worklist) {
  bool result = false;
  worklist.Iterate([&result](Type candidate) {
    if (Heap::InYoungGeneration(candidate)) {
      result = true;
    }
  });
  return result;
}
#endif

}  // namespace internal
}  // namespace v8
