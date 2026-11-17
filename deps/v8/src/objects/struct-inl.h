// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_STRUCT_INL_H_
#define V8_OBJECTS_STRUCT_INL_H_

#include "src/objects/struct.h"
// Include the non-inl header before the rest of the headers.

#include "src/heap/heap-write-barrier-inl.h"
#include "src/objects/heap-object-set-map-inl.h"
#include "src/objects/oddball-predicates-inl.h"
#include "src/objects/tagged-field-inl.h"
#include "src/roots/roots-inl.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

Struct::Struct(Tagged<ReadOnly<Map>> map) : HeapObject(map) {}

Tuple2::Tuple2(const AllocationWitness& witness, Tagged<Object> value1,
               Tagged<Object> value2)
    : Struct(witness.roots().tuple2_map()),
      value1_(witness, value1),
      value2_(witness, value2) {}

Tuple2::Tuple2(const AllocationWitness& witness, Tagged<Object> value1,
               Tagged<Object> value2, RelaxedStoreTag tag)
    : Struct(witness.roots().tuple2_map()),
      value1_(witness, value1, tag),
      value2_(witness, value2, tag) {}

Tagged<Object> Tuple2::value1() const { return value1_.load(); }
void Tuple2::set_value1(Tagged<Object> value, WriteBarrierMode mode) {
  value1_.store(this, value, mode);
}
Tagged<Object> Tuple2::value1(RelaxedLoadTag) const {
  return value1_.Relaxed_Load();
}
void Tuple2::set_value1(Tagged<Object> value, RelaxedStoreTag,
                        WriteBarrierMode mode) {
  value1_.Relaxed_Store(this, value, mode);
}
Tagged<Object> Tuple2::value1(AcquireLoadTag) const {
  return value1_.Acquire_Load();
}
void Tuple2::set_value1(Tagged<Object> value, ReleaseStoreTag,
                        WriteBarrierMode mode) {
  value1_.Release_Store(this, value, mode);
}

Tagged<Object> Tuple2::value2() const { return value2_.load(); }
void Tuple2::set_value2(Tagged<Object> value, WriteBarrierMode mode) {
  value2_.store(this, value, mode);
}
Tagged<Object> Tuple2::value2(RelaxedLoadTag) const {
  return value2_.Relaxed_Load();
}
void Tuple2::set_value2(Tagged<Object> value, RelaxedStoreTag,
                        WriteBarrierMode mode) {
  value2_.Relaxed_Store(this, value, mode);
}

AccessorPair::AccessorPair(const AllocationWitness& witness)
    : Struct(witness.roots().accessor_pair_map()),
      getter_(witness.roots().null_value()),
      setter_(witness.roots().null_value()) {}

Tagged<Object> AccessorPair::get(AccessorComponent component) {
  return component == ACCESSOR_GETTER ? getter() : setter();
}

void AccessorPair::set(AccessorComponent component, Tagged<Object> value) {
  if (component == ACCESSOR_GETTER) {
    set_getter(value);
  } else {
    set_setter(value);
  }
}

void AccessorPair::set(AccessorComponent component, Tagged<Object> value,
                       ReleaseStoreTag tag) {
  if (component == ACCESSOR_GETTER) {
    set_getter(value, tag);
  } else {
    set_setter(value, tag);
  }
}

Tagged<Object> AccessorPair::getter() const { return getter_.load(); }
void AccessorPair::set_getter(Tagged<Object> value, WriteBarrierMode mode) {
  getter_.store(this, value, mode);
}

Tagged<Object> AccessorPair::getter(AcquireLoadTag) const {
  return getter_.Acquire_Load();
}
void AccessorPair::set_getter(Tagged<Object> value, ReleaseStoreTag,
                              WriteBarrierMode mode) {
  getter_.Release_Store(this, value, mode);
}

Tagged<Object> AccessorPair::setter() const { return setter_.load(); }
void AccessorPair::set_setter(Tagged<Object> value, WriteBarrierMode mode) {
  setter_.store(this, value, mode);
}

Tagged<Object> AccessorPair::setter(AcquireLoadTag) const {
  return setter_.Acquire_Load();
}
void AccessorPair::set_setter(Tagged<Object> value, ReleaseStoreTag,
                              WriteBarrierMode mode) {
  setter_.Release_Store(this, value, mode);
}

void AccessorPair::SetComponents(Tagged<Object> getter, Tagged<Object> setter) {
  if (!IsNull(getter)) set_getter(getter);
  if (!IsNull(setter)) set_setter(setter);
}

bool AccessorPair::Equals(Tagged<Object> getter_value,
                          Tagged<Object> setter_value) {
  return (getter() == getter_value) && (setter() == setter_value);
}

ClassPositions::ClassPositions(const AllocationWitness& witness, int start,
                               int end)
    : Struct(witness.roots().class_positions_map()),
      start_(Smi::FromInt(start)),
      end_(Smi::FromInt(end)) {}

int ClassPositions::start() const { return start_.load().value(); }

int ClassPositions::end() const { return end_.load().value(); }

ForInEnumeratorHolder::ForInEnumeratorHolder(const AllocationWitness& witness,
                                             Tagged<Map> enum_cache_map,
                                             Tagged<FixedArray> named_keys,
                                             Tagged<Smi> elements_length,
                                             Tagged<Smi> cache_length)
    : Struct(witness.roots().for_in_enumerator_holder_map()),
      enum_cache_map_(witness, enum_cache_map),
      named_keys_(witness, named_keys),
      elements_length_(elements_length),
      cache_length_(cache_length) {}

Tagged<Map> ForInEnumeratorHolder::enum_cache_map() const {
  return enum_cache_map_.load();
}

Tagged<FixedArray> ForInEnumeratorHolder::named_keys() const {
  return named_keys_.load();
}

Tagged<Smi> ForInEnumeratorHolder::elements_length() const {
  return elements_length_.load();
}

Tagged<Smi> ForInEnumeratorHolder::cache_length() const {
  return cache_length_.load();
}

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_STRUCT_INL_H_
