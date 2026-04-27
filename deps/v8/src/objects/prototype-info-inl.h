// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_PROTOTYPE_INFO_INL_H_
#define V8_OBJECTS_PROTOTYPE_INFO_INL_H_

#include "src/objects/prototype-info.h"
// Include the non-inl header before the rest of the headers.

#include "src/heap/heap-write-barrier-inl.h"
#include "src/ic/handler-configuration.h"
#include "src/objects/fixed-array-inl.h"
#include "src/objects/map-inl.h"
#include "src/objects/maybe-object.h"
#include "src/objects/objects-inl.h"
#include "src/objects/struct-inl.h"
#include "src/torque/runtime-macro-shims.h"
#include "src/torque/runtime-support.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

#include "torque-generated/src/objects/prototype-info-tq-inl.inc"

Tagged<UnionOf<JSModuleNamespace, Undefined>> PrototypeInfo::module_namespace()
    const {
  return module_namespace_.load();
}
void PrototypeInfo::set_module_namespace(
    Tagged<UnionOf<JSModuleNamespace, Undefined>> value,
    WriteBarrierMode mode) {
  module_namespace_.store(this, value, mode);
}

Tagged<UnionOf<WeakArrayList, Zero>> PrototypeInfo::prototype_users() const {
  return prototype_users_.load();
}
void PrototypeInfo::set_prototype_users(
    Tagged<UnionOf<WeakArrayList, Zero>> value, WriteBarrierMode mode) {
  prototype_users_.store(this, value, mode);
}

Tagged<UnionOf<FixedArray, Zero, Undefined>>
PrototypeInfo::prototype_chain_enum_cache() const {
  return prototype_chain_enum_cache_.load();
}
void PrototypeInfo::set_prototype_chain_enum_cache(
    Tagged<UnionOf<FixedArray, Zero, Undefined>> value, WriteBarrierMode mode) {
  prototype_chain_enum_cache_.store(this, value, mode);
}

int PrototypeInfo::registry_slot() const {
  return registry_slot_.load().value();
}
void PrototypeInfo::set_registry_slot(int value) {
  registry_slot_.store(this, Smi::FromInt(value));
}

int PrototypeInfo::bit_field() const { return bit_field_.load().value(); }
void PrototypeInfo::set_bit_field(int value) {
  bit_field_.store(this, Smi::FromInt(value));
}

Tagged<UnionOf<WeakArrayList, Undefined>> PrototypeInfo::derived_maps() const {
  return derived_maps_.load();
}
Tagged<UnionOf<WeakArrayList, Undefined>> PrototypeInfo::derived_maps(
    AcquireLoadTag) const {
  return derived_maps_.Acquire_Load();
}
void PrototypeInfo::set_derived_maps(
    Tagged<UnionOf<WeakArrayList, Undefined>> value, WriteBarrierMode mode) {
  derived_maps_.store(this, value, mode);
}
void PrototypeInfo::set_derived_maps(
    Tagged<UnionOf<WeakArrayList, Undefined>> value, ReleaseStoreTag,
    WriteBarrierMode mode) {
  derived_maps_.Release_Store(this, value, mode);
}

Tagged<UnionOf<PrototypeSharedClosureInfo, Undefined>>
PrototypeInfo::prototype_shared_closure_info() const {
  return prototype_shared_closure_info_.load();
}
void PrototypeInfo::set_prototype_shared_closure_info(
    Tagged<UnionOf<PrototypeSharedClosureInfo, Undefined>> value,
    WriteBarrierMode mode) {
  prototype_shared_closure_info_.store(this, value, mode);
}

Tagged<UnionOf<LoadHandler, Zero>> PrototypeInfo::cached_handler(
    int index) const {
  DCHECK_GE(index, 0);
  DCHECK_LT(index, kCachedHandlerCount);
  return cached_handler_[index].load();
}
void PrototypeInfo::set_cached_handler(int index,
                                       Tagged<UnionOf<LoadHandler, Zero>> value,
                                       WriteBarrierMode mode) {
  DCHECK_GE(index, 0);
  DCHECK_LT(index, kCachedHandlerCount);
  cached_handler_[index].store(this, value, mode);
}

Tagged<MaybeObject> PrototypeInfo::ObjectCreateMap() {
  auto derived = derived_maps();
  if (IsUndefined(derived)) {
    return Tagged<MaybeObject>();
  }
  // Index 0 is the map for object create
  Tagged<WeakArrayList> derived_list = Cast<WeakArrayList>(derived);
  DCHECK_GT(derived_list->length().value(), 0);
  Tagged<MaybeObject> el = derived_list->Get(0);
  DCHECK(el.IsWeakOrCleared());
  return el;
}

Tagged<MaybeObject> PrototypeInfo::ObjectCreateMap(AcquireLoadTag tag) {
  auto derived = derived_maps(tag);
  if (IsUndefined(derived)) {
    return Tagged<MaybeObject>();
  }
  // Index 0 is the map for object create
  Tagged<WeakArrayList> derived_list = Cast<WeakArrayList>(derived);
  DCHECK_GT(derived_list->length().value(), 0);
  Tagged<MaybeObject> el = derived_list->Get(0);
  DCHECK(el.IsWeakOrCleared());
  return el;
}

// static
void PrototypeInfo::SetObjectCreateMap(DirectHandle<PrototypeInfo> info,
                                       DirectHandle<Map> map,
                                       Isolate* isolate) {
  if (IsUndefined(info->derived_maps())) {
    Tagged<WeakArrayList> derived = *isolate->factory()->NewWeakArrayList(1);
    derived->Set(0, MakeWeak(*map));
    derived->set_length(1);
    info->set_derived_maps(derived, kReleaseStore);
  } else {
    Tagged<WeakArrayList> derived = Cast<WeakArrayList>(info->derived_maps());
    DCHECK(derived->Get(0).IsCleared());
    DCHECK_GT(derived->length().value(), 0);
    derived->Set(0, MakeWeak(*map));
  }
}

Tagged<MaybeObject> PrototypeInfo::GetDerivedMap(DirectHandle<Map> from) {
  if (IsUndefined(derived_maps())) {
    return Tagged<MaybeObject>();
  }
  auto derived = Cast<WeakArrayList>(derived_maps());
  const uint32_t derived_length = derived->length().value();
  // Index 0 is the map for object create
  for (uint32_t i = 1; i < derived_length; ++i) {
    Tagged<MaybeObject> el = derived->Get(i);
    Tagged<Map> map_obj;
    if (el.GetHeapObjectIfWeak(&map_obj)) {
      Tagged<Map> to = Cast<Map>(map_obj);
      if (to->GetConstructor() == from->GetConstructor() &&
          to->instance_type() == from->instance_type()) {
        return el;
      }
    }
  }
  return Tagged<MaybeObject>();
}

// static
void PrototypeInfo::AddDerivedMap(DirectHandle<PrototypeInfo> info,
                                  DirectHandle<Map> to, Isolate* isolate) {
  if (IsUndefined(info->derived_maps())) {
    // Index 0 is the map for object create
    Tagged<WeakArrayList> derived = *isolate->factory()->NewWeakArrayList(2);
    // GetConstructMap assumes a weak pointer.
    derived->Set(0, kClearedWeakValue);
    derived->Set(1, MakeWeak(*to));
    derived->set_length(2);
    info->set_derived_maps(derived, kReleaseStore);
    return;
  }
  auto derived = handle(Cast<WeakArrayList>(info->derived_maps()), isolate);
  const uint32_t derived_length = derived->length().value();
  // Index 0 is the map for object create
  uint32_t i = 1;
  for (; i < derived_length; ++i) {
    Tagged<MaybeObject> el = derived->Get(i);
    if (el.IsCleared()) {
      derived->Set(i, MakeWeak(*to));
      return;
    }
  }

  auto bigger = WeakArrayList::EnsureSpace(isolate, derived, i + 1);
  bigger->Set(i, MakeWeak(*to));
  bigger->set_length(i + 1);
  if (*bigger != *derived) {
    info->set_derived_maps(*bigger, kReleaseStore);
  }
}

bool PrototypeInfo::IsPrototypeInfoFast(Tagged<Object> object) {
  bool is_proto_info = IsPrototypeInfo(object);
  return is_proto_info;
}

BOOL_ACCESSORS(PrototypeInfo, bit_field, should_be_fast_map,
               ShouldBeFastBit::kShift)

// TODO(375937549): Convert index to uint32_t.
void PrototypeUsers::MarkSlotEmpty(Tagged<WeakArrayList> array, int index) {
  DCHECK_GT(index, 0);
  DCHECK_LT(static_cast<uint32_t>(index), array->length().value());
  // Chain the empty slots into a linked list (each empty slot contains the
  // index of the next empty slot).
  array->Set(index, empty_slot_index(array));
  set_empty_slot_index(array, index);
}

Tagged<Smi> PrototypeUsers::empty_slot_index(Tagged<WeakArrayList> array) {
  return array->Get(kEmptySlotIndex).ToSmi();
}

void PrototypeUsers::set_empty_slot_index(Tagged<WeakArrayList> array,
                                          int index) {
  array->Set(kEmptySlotIndex, Smi::FromInt(index));
}

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_PROTOTYPE_INFO_INL_H_
