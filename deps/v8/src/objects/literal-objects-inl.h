// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_LITERAL_OBJECTS_INL_H_
#define V8_OBJECTS_LITERAL_OBJECTS_INL_H_

#include "src/objects/literal-objects.h"
// Include the non-inl header before the rest of the headers.

#include <optional>

#include "src/objects/objects-inl.h"
#include "src/objects/trusted-object-inl.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8::internal {

#include "torque-generated/src/objects/literal-objects-tq-inl.inc"

//
// ObjectBoilerplateDescription
//

// static
template <class IsolateT>
Handle<ObjectBoilerplateDescription> ObjectBoilerplateDescription::New(
    IsolateT* isolate, int boilerplate, int all_properties, int index_keys,
    bool has_seen_proto, AllocationType allocation) {
  DCHECK_GE(boilerplate, 0);
  DCHECK_GE(all_properties, index_keys);
  DCHECK_GE(index_keys, 0);

  int capacity = boilerplate * kElementsPerEntry;
  CHECK_LE(static_cast<unsigned>(capacity), kMaxCapacity);

  int backing_store_size =
      all_properties - index_keys - (has_seen_proto ? 1 : 0);
  DCHECK_GE(backing_store_size, 0);

  // Note we explicitly do NOT canonicalize to the
  // empty_object_boilerplate_description here since `flags` may be modified
  // even on empty descriptions.

  std::optional<DisallowGarbageCollection> no_gc;
  auto result = Cast<ObjectBoilerplateDescription>(
      Allocate(isolate, capacity, &no_gc, allocation));
  result->set_flags(0);
  result->set_backing_store_size(backing_store_size);
  MemsetTagged((*result)->RawFieldOfFirstElement(),
               ReadOnlyRoots{isolate}.undefined_value(), capacity);
  return result;
}

int ObjectBoilerplateDescription::backing_store_size() const {
  return backing_store_size_.load().value();
}
void ObjectBoilerplateDescription::set_backing_store_size(int value) {
  backing_store_size_.store(this, Smi::FromInt(value));
}
int ObjectBoilerplateDescription::flags() const {
  return flags_.load().value();
}
void ObjectBoilerplateDescription::set_flags(int value) {
  flags_.store(this, Smi::FromInt(value));
}

Tagged<Object> ObjectBoilerplateDescription::name(int index) const {
  return get(NameIndex(index));
}

Tagged<Object> ObjectBoilerplateDescription::value(int index) const {
  return get(ValueIndex(index));
}

void ObjectBoilerplateDescription::set_key_value(int index, Tagged<Object> key,
                                                 Tagged<Object> value) {
  DCHECK_LT(static_cast<unsigned>(index), boilerplate_properties_count());
  set(NameIndex(index), key);
  set(ValueIndex(index), value);
}

void ObjectBoilerplateDescription::set_value(int index, Tagged<Object> value) {
  DCHECK_LT(static_cast<unsigned>(index), boilerplate_properties_count());
  set(ValueIndex(index), value);
}

int ObjectBoilerplateDescription::boilerplate_properties_count() const {
  DCHECK_EQ(0, capacity() % kElementsPerEntry);
  return capacity() / kElementsPerEntry;
}

//
// ClassBoilerplate
//

int ClassBoilerplate::arguments_count() const {
  return arguments_count_.load().value();
}
void ClassBoilerplate::set_arguments_count(int value) {
  arguments_count_.store(this, Smi::FromInt(value));
}

Tagged<Object> ClassBoilerplate::static_properties_template() const {
  return static_properties_template_.load();
}
void ClassBoilerplate::set_static_properties_template(Tagged<Object> value,
                                                      WriteBarrierMode mode) {
  static_properties_template_.store(this, value, mode);
}

Tagged<Object> ClassBoilerplate::static_elements_template() const {
  return static_elements_template_.load();
}
void ClassBoilerplate::set_static_elements_template(Tagged<Object> value,
                                                    WriteBarrierMode mode) {
  static_elements_template_.store(this, value, mode);
}

Tagged<FixedArray> ClassBoilerplate::static_computed_properties() const {
  return static_computed_properties_.load();
}
void ClassBoilerplate::set_static_computed_properties(Tagged<FixedArray> value,
                                                      WriteBarrierMode mode) {
  static_computed_properties_.store(this, value, mode);
}

Tagged<Object> ClassBoilerplate::instance_properties_template() const {
  return instance_properties_template_.load();
}
void ClassBoilerplate::set_instance_properties_template(Tagged<Object> value,
                                                        WriteBarrierMode mode) {
  instance_properties_template_.store(this, value, mode);
}

Tagged<Object> ClassBoilerplate::instance_elements_template() const {
  return instance_elements_template_.load();
}
void ClassBoilerplate::set_instance_elements_template(Tagged<Object> value,
                                                      WriteBarrierMode mode) {
  instance_elements_template_.store(this, value, mode);
}

Tagged<FixedArray> ClassBoilerplate::instance_computed_properties() const {
  return instance_computed_properties_.load();
}
void ClassBoilerplate::set_instance_computed_properties(
    Tagged<FixedArray> value, WriteBarrierMode mode) {
  instance_computed_properties_.store(this, value, mode);
}

//
// ArrayBoilerplateDescription
//

Tagged<Smi> ArrayBoilerplateDescription::flags() const { return flags_.load(); }
void ArrayBoilerplateDescription::set_flags(Tagged<Smi> value,
                                            WriteBarrierMode mode) {
  flags_.store(this, value, mode);
}

Tagged<FixedArrayBase> ArrayBoilerplateDescription::constant_elements() const {
  return constant_elements_.load();
}
void ArrayBoilerplateDescription::set_constant_elements(
    Tagged<FixedArrayBase> value, WriteBarrierMode mode) {
  constant_elements_.store(this, value, mode);
}

ElementsKind ArrayBoilerplateDescription::elements_kind() const {
  return static_cast<ElementsKind>(flags().value());
}

void ArrayBoilerplateDescription::set_elements_kind(ElementsKind kind) {
  set_flags(Smi::FromInt(kind));
}

bool ArrayBoilerplateDescription::is_empty() const {
  return constant_elements()->length() == 0;
}

//
// RegExpBoilerplateDescription
//

Tagged<RegExpData> RegExpBoilerplateDescription::data(
    IsolateForSandbox isolate) const {
  return data_.load(isolate);
}

void RegExpBoilerplateDescription::set_data(Tagged<RegExpData> value,
                                            WriteBarrierMode mode) {
  data_.store(this, value, mode);
}

Tagged<String> RegExpBoilerplateDescription::source() const {
  return source_.load();
}
void RegExpBoilerplateDescription::set_source(Tagged<String> value,
                                              WriteBarrierMode mode) {
  source_.store(this, value, mode);
}

int RegExpBoilerplateDescription::flags() const {
  return flags_.load().value();
}
void RegExpBoilerplateDescription::set_flags(int value) {
  flags_.store(this, Smi::FromInt(value));
}

}  // namespace v8::internal

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_LITERAL_OBJECTS_INL_H_
