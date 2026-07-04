// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_HEAP_OBJECT_SET_MAP_INL_H_
#define V8_OBJECTS_HEAP_OBJECT_SET_MAP_INL_H_

#include "src/objects/heap-object.h"
// Include the non-inl header before the rest of the headers.

#include "src/common/code-memory-access-inl.h"
#include "src/execution/isolate.h"
#include "src/heap/heap-verifier.h"
#include "src/heap/heap-write-barrier-inl.h"
#include "src/heap/heap.h"
#include "src/heap/read-only-heap-inl.h"
#include "src/objects/instance-type-inl.h"
#include "src/objects/map-word-inl.h"
#include "src/objects/slots-inl.h"
#include "src/objects/tagged-field-inl.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8::internal {

void HeapObject::set_map(Isolate* isolate, Tagged<Map> value) {
  set_map<EmitWriteBarrier::kYes>(isolate, value, kRelaxedStore,
                                  VerificationMode::kPotentialLayoutChange);
}

template <typename IsolateT>
void HeapObject::set_map(IsolateT* isolate, Tagged<Map> value,
                         ReleaseStoreTag tag) {
  set_map<EmitWriteBarrier::kYes>(isolate, value, kReleaseStore,
                                  VerificationMode::kPotentialLayoutChange);
}

template <typename IsolateT>
void HeapObject::set_map_safe_transition(IsolateT* isolate, Tagged<Map> value) {
  set_map<EmitWriteBarrier::kYes>(isolate, value, kRelaxedStore,
                                  VerificationMode::kSafeMapTransition);
}

template <typename IsolateT>
void HeapObject::set_map_safe_transition(IsolateT* isolate, Tagged<Map> value,
                                         ReleaseStoreTag tag) {
  set_map<EmitWriteBarrier::kYes>(isolate, value, kReleaseStore,
                                  VerificationMode::kSafeMapTransition);
}

void HeapObject::set_map_safe_transition_no_write_barrier(Isolate* isolate,
                                                          Tagged<Map> value,
                                                          RelaxedStoreTag tag) {
  set_map<EmitWriteBarrier::kNo>(isolate, value, kRelaxedStore,
                                 VerificationMode::kSafeMapTransition);
}

void HeapObject::set_map_safe_transition_no_write_barrier(Isolate* isolate,
                                                          Tagged<Map> value,
                                                          ReleaseStoreTag tag) {
  set_map<EmitWriteBarrier::kNo>(isolate, value, kReleaseStore,
                                 VerificationMode::kSafeMapTransition);
}

// Unsafe accessor omitting write barrier.
void HeapObject::set_map_no_write_barrier(Isolate* isolate, Tagged<Map> value,
                                          RelaxedStoreTag tag) {
  set_map<EmitWriteBarrier::kNo>(isolate, value, kRelaxedStore,
                                 VerificationMode::kPotentialLayoutChange);
}

void HeapObject::set_map_no_write_barrier(Isolate* isolate, Tagged<Map> value,
                                          ReleaseStoreTag tag) {
  set_map<EmitWriteBarrier::kNo>(isolate, value, kReleaseStore,
                                 VerificationMode::kPotentialLayoutChange);
}

template <HeapObject::EmitWriteBarrier emit_write_barrier, typename MemoryOrder,
          typename IsolateT>
void HeapObject::set_map(IsolateT* isolate, Tagged<Map> value,
                         MemoryOrder order, VerificationMode mode) {
#if V8_ENABLE_WEBASSEMBLY
  // In {WasmGraphBuilder::SetMap} and {WasmGraphBuilder::LoadMap}, we treat
  // maps as immutable. Therefore we are not allowed to mutate them here.
  DCHECK(!IsWasmStructMap(value) && !IsWasmArrayMap(value));
#endif
  if (v8_flags.verify_heap) {
    if (mode == VerificationMode::kSafeMapTransition) {
      HeapVerifier::VerifySafeMapTransition(isolate->heap()->AsHeap(), this,
                                            value);
    } else {
      DCHECK_EQ(mode, VerificationMode::kPotentialLayoutChange);
      HeapVerifier::VerifyObjectLayoutChange(isolate->heap()->AsHeap(), this,
                                             value);
    }
  }
  set_map_word(value, order);
  Heap::NotifyObjectLayoutChangeDone(this);
#ifndef V8_DISABLE_WRITE_BARRIERS
  if (emit_write_barrier == EmitWriteBarrier::kYes) {
    WriteBarrier::ForValue(this, MaybeObjectSlot(map_slot()), value,
                           UPDATE_WRITE_BARRIER);
  } else {
    DCHECK_EQ(emit_write_barrier, EmitWriteBarrier::kNo);
#if V8_VERIFY_WRITE_BARRIERS
    DCHECK(!WriteBarrier::IsRequired(this, value));
#endif
  }
#endif
}

template <typename IsolateT>
void HeapObject::set_map_after_allocation(IsolateT* isolate, Tagged<Map> value,
                                          WriteBarrierMode mode) {
  set_map_word(value, kRelaxedStore);
#ifndef V8_DISABLE_WRITE_BARRIERS
  if (mode != SKIP_WRITE_BARRIER) {
    DCHECK(!value.is_null());
    WriteBarrier::ForValue(this, MaybeObjectSlot(map_slot()), value, mode);
  } else {
    SLOW_DCHECK(
        // We allow writes of a null map before root initialisation.
        value.is_null() ? !isolate->read_only_heap()->roots_init_complete()
                        : !WriteBarrier::IsRequired(this, value));
  }
#endif
}

// static
void HeapObject::SetFillerMap(const WritableFreeSpace& writable_space,
                              Tagged<Map> value) {
  writable_space.WriteHeaderSlot<Map, offsetof(HeapObject, map_)>(
      value, kRelaxedStore);
}

ObjectSlot HeapObject::map_slot() const {
  return ObjectSlot(MapField::address(this));
}

void HeapObject::set_map_word(Tagged<Map> map, RelaxedStoreTag) {
  MapField::Relaxed_Store_Map_Word(this, MapWord::FromMap(map));
}

void HeapObject::set_map_word_forwarded(Tagged<HeapObject> target_object,
                                        RelaxedStoreTag) {
  MapField::Relaxed_Store_Map_Word(
      this, MapWord::FromForwardingAddress(this, target_object));
}

void HeapObject::set_map_word(Tagged<Map> map, ReleaseStoreTag) {
  MapField::Release_Store_Map_Word(this, MapWord::FromMap(map));
}

void HeapObject::set_map_word_forwarded(Tagged<HeapObject> target_object,
                                        ReleaseStoreTag) {
  MapField::Release_Store_Map_Word(
      this, MapWord::FromForwardingAddress(this, target_object));
}

bool HeapObject::relaxed_compare_and_swap_map_word_forwarded(
    MapWord old_map_word, Tagged<HeapObject> new_target_object) {
  Tagged_t result = MapField::Relaxed_CompareAndSwap(
      this, old_map_word,
      MapWord::FromForwardingAddress(this, new_target_object));
  return result == static_cast<Tagged_t>(old_map_word.ptr());
}

}  // namespace v8::internal

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_HEAP_OBJECT_SET_MAP_INL_H_
