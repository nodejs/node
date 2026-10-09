// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_LITERAL_OBJECTS_INL_H_
#define V8_OBJECTS_LITERAL_OBJECTS_INL_H_

#include "src/objects/literal-objects.h"
// Include the non-inl header before the rest of the headers.

#include <optional>

#include "src/objects/heap-object-set-map-inl.h"
#include "src/objects/objects-inl.h"
#include "src/objects/trusted-object-inl.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8::internal {

//
// ObjectBoilerplateDescription
//

// static
template <class IsolateT>
Handle<ObjectBoilerplateDescription> ObjectBoilerplateDescription::New(
    IsolateT* isolate, uint32_t boilerplate, uint32_t backing_store_size,
    AllocationType allocation) {
  const uint32_t capacity = boilerplate * kElementsPerEntry;
  CHECK_LE(capacity, kMaxCapacity);

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

Tagged<ObjectBoilerplateDescription::KeyT> ObjectBoilerplateDescription::name(
    int index) const {
  return Cast<ObjectBoilerplateDescription::KeyT>(get(NameIndex(index)));
}

Tagged<Object> ObjectBoilerplateDescription::value(int index) const {
  return get(ValueIndex(index));
}

void ObjectBoilerplateDescription::set_key_value(
    int index, Tagged<ObjectBoilerplateDescription::KeyT> key,
    Tagged<Object> value) {
  DCHECK_LT(static_cast<unsigned>(index), boilerplate_properties_count());
  set(NameIndex(index), key);
  set(ValueIndex(index), value);
}

void ObjectBoilerplateDescription::set_value(int index, Tagged<Object> value) {
  DCHECK_LT(static_cast<unsigned>(index), boilerplate_properties_count());
  set(ValueIndex(index), value);
}

// TODO(375937549): Convert to uint32_t.
int ObjectBoilerplateDescription::boilerplate_properties_count() const {
  const uint32_t cap = capacity().value();
  DCHECK_EQ(0, cap % kElementsPerEntry);
  return static_cast<int>(cap / kElementsPerEntry);
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

ArrayBoilerplateDescription::ArrayBoilerplateDescription(
    const AllocationWitness& witness, ReadOnlyRoots roots,
    ElementsKind elements_kind, Tagged<FixedArrayBase> constant_values)
    : Struct(roots.array_boilerplate_description_map()),
      flags_(Smi::FromInt(elements_kind)),
      constant_elements_(witness, constant_values) {}

Tagged<Smi> ArrayBoilerplateDescription::flags() const { return flags_.load(); }

Tagged<FixedArrayBase> ArrayBoilerplateDescription::constant_elements() const {
  return constant_elements_.load();
}

ElementsKind ArrayBoilerplateDescription::elements_kind() const {
  return static_cast<ElementsKind>(flags().value());
}

bool ArrayBoilerplateDescription::is_empty() const {
  return constant_elements()->ulength().value() == 0;
}

//
// RegExpBoilerplateDescription
//

RegExpBoilerplateDescription::RegExpBoilerplateDescription(
    const AllocationWitness& witness, ReadOnlyRoots roots,
    Tagged<RegExpData> data, Tagged<Smi> flags)
    : Struct(roots.regexp_boilerplate_description_map()),
      data_(witness, data),
      flags_(flags) {}

Tagged<RegExpData> RegExpBoilerplateDescription::data(
    IsolateForSandbox isolate) const {
  return data_.load(isolate);
}

int RegExpBoilerplateDescription::flags() const {
  return flags_.load().value();
}

PrototypeSharedClosureInfo::PrototypeSharedClosureInfo(
    const AllocationWitness& witness, ReadOnlyRoots roots,
    Tagged<ObjectBoilerplateDescription> boilerplate_description,
    Tagged<ClosureFeedbackCellArray> closure_feedback_cell_array,
    Tagged<Context> context)
    : Struct(roots.prototype_shared_closure_info_map()),
      boilerplate_description_(witness, boilerplate_description),
      closure_feedback_cell_array_(witness, closure_feedback_cell_array),
      context_(witness, context) {}

Tagged<ObjectBoilerplateDescription>
PrototypeSharedClosureInfo::boilerplate_description() const {
  return boilerplate_description_.load();
}

Tagged<ClosureFeedbackCellArray>
PrototypeSharedClosureInfo::closure_feedback_cell_array() const {
  return closure_feedback_cell_array_.load();
}

Tagged<Context> PrototypeSharedClosureInfo::context() const {
  return context_.load();
}

}  // namespace v8::internal

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_LITERAL_OBJECTS_INL_H_
