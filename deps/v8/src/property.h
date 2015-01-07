// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_PROPERTY_H_
#define V8_PROPERTY_H_

#include <iosfwd>

#include "src/factory.h"
#include "src/field-index.h"
#include "src/field-index-inl.h"
#include "src/isolate.h"
#include "src/types.h"

namespace v8 {
namespace internal {

// Abstraction for elements in instance-descriptor arrays.
//
// Each descriptor has a key, property attributes, property type,
// property index (in the actual instance-descriptor array) and
// optionally a piece of data.
class Descriptor BASE_EMBEDDED {
 public:
  void KeyToUniqueName() {
    if (!key_->IsUniqueName()) {
      key_ = key_->GetIsolate()->factory()->InternalizeString(
          Handle<String>::cast(key_));
    }
  }

  Handle<Name> GetKey() const { return key_; }
  Handle<Object> GetValue() const { return value_; }
  PropertyDetails GetDetails() const { return details_; }

  void SetSortedKeyIndex(int index) { details_ = details_.set_pointer(index); }

 private:
  Handle<Name> key_;
  Handle<Object> value_;
  PropertyDetails details_;

 protected:
  Descriptor() : details_(Smi::FromInt(0)) {}

  void Init(Handle<Name> key, Handle<Object> value, PropertyDetails details) {
    key_ = key;
    value_ = value;
    details_ = details;
  }

  Descriptor(Handle<Name> key, Handle<Object> value, PropertyDetails details)
      : key_(key),
        value_(value),
        details_(details) { }

  Descriptor(Handle<Name> key,
             Handle<Object> value,
             PropertyAttributes attributes,
             PropertyType type,
             Representation representation,
             int field_index = 0)
      : key_(key),
        value_(value),
        details_(attributes, type, representation, field_index) { }

  friend class DescriptorArray;
  friend class Map;
};


std::ostream& operator<<(std::ostream& os, const Descriptor& d);


class FieldDescriptor FINAL : public Descriptor {
 public:
  FieldDescriptor(Handle<Name> key,
                  int field_index,
                  PropertyAttributes attributes,
                  Representation representation)
      : Descriptor(key, HeapType::Any(key->GetIsolate()), attributes,
                   FIELD, representation, field_index) {}
  FieldDescriptor(Handle<Name> key,
                  int field_index,
                  Handle<HeapType> field_type,
                  PropertyAttributes attributes,
                  Representation representation)
      : Descriptor(key, field_type, attributes, FIELD,
                   representation, field_index) { }
};


class ConstantDescriptor FINAL : public Descriptor {
 public:
  ConstantDescriptor(Handle<Name> key,
                     Handle<Object> value,
                     PropertyAttributes attributes)
      : Descriptor(key, value, attributes, CONSTANT,
                   value->OptimalRepresentation()) {}
};


class CallbacksDescriptor FINAL : public Descriptor {
 public:
  CallbacksDescriptor(Handle<Name> key,
                      Handle<Object> foreign,
                      PropertyAttributes attributes)
      : Descriptor(key, foreign, attributes, CALLBACKS,
                   Representation::Tagged()) {}
};


class LookupResult FINAL BASE_EMBEDDED {
 public:
  explicit LookupResult(Isolate* isolate)
      : isolate_(isolate),
        next_(isolate->top_lookup_result()),
        lookup_type_(NOT_FOUND),
        holder_(NULL),
        transition_(NULL),
        details_(NONE, FIELD, Representation::None()) {
    isolate->set_top_lookup_result(this);
  }

  ~LookupResult() {
    DCHECK(isolate()->top_lookup_result() == this);
    isolate()->set_top_lookup_result(next_);
  }

  Isolate* isolate() const { return isolate_; }

  void DescriptorResult(JSObject* holder, PropertyDetails details, int number) {
    lookup_type_ = DESCRIPTOR_TYPE;
    holder_ = holder;
    transition_ = NULL;
    details_ = details;
    number_ = number;
  }

  void TransitionResult(JSObject* holder, Map* target) {
    lookup_type_ = TRANSITION_TYPE;
    number_ = target->LastAdded();
    details_ = target->instance_descriptors()->GetDetails(number_);
    holder_ = holder;
    transition_ = target;
  }

  void NotFound() {
    lookup_type_ = NOT_FOUND;
    details_ = PropertyDetails(NONE, FIELD, 0);
    holder_ = NULL;
    transition_ = NULL;
  }

  Representation representation() const {
    DCHECK(IsFound());
    return details_.representation();
  }

  // Property callbacks does not include transitions to callbacks.
  bool IsPropertyCallbacks() const {
    return !IsTransition() && details_.type() == CALLBACKS;
  }

  bool IsReadOnly() const {
    DCHECK(IsFound());
    return details_.IsReadOnly();
  }

  bool IsField() const {
    return lookup_type_ == DESCRIPTOR_TYPE && details_.type() == FIELD;
  }

  bool IsConstant() const {
    return lookup_type_ == DESCRIPTOR_TYPE && details_.type() == CONSTANT;
  }

  bool IsConfigurable() const { return details_.IsConfigurable(); }
  bool IsFound() const { return lookup_type_ != NOT_FOUND; }
  bool IsTransition() const { return lookup_type_ == TRANSITION_TYPE; }

  // Is the result is a property excluding transitions and the null descriptor?
  bool IsProperty() const {
    return IsFound() && !IsTransition();
  }

  Map* GetTransitionTarget() const {
    DCHECK(IsTransition());
    return transition_;
  }

  bool IsTransitionToField() const {
    return IsTransition() && details_.type() == FIELD;
  }

  int GetLocalFieldIndexFromMap(Map* map) const {
    return GetFieldIndexFromMap(map) - map->inobject_properties();
  }

  Object* GetConstantFromMap(Map* map) const {
    DCHECK(details_.type() == CONSTANT);
    return GetValueFromMap(map);
  }

  Object* GetValueFromMap(Map* map) const {
    DCHECK(lookup_type_ == DESCRIPTOR_TYPE ||
           lookup_type_ == TRANSITION_TYPE);
    DCHECK(number_ < map->NumberOfOwnDescriptors());
    return map->instance_descriptors()->GetValue(number_);
  }

  int GetFieldIndexFromMap(Map* map) const {
    DCHECK(lookup_type_ == DESCRIPTOR_TYPE ||
           lookup_type_ == TRANSITION_TYPE);
    DCHECK(number_ < map->NumberOfOwnDescriptors());
    return map->instance_descriptors()->GetFieldIndex(number_);
  }

  HeapType* GetFieldTypeFromMap(Map* map) const {
    DCHECK_NE(NOT_FOUND, lookup_type_);
    DCHECK(number_ < map->NumberOfOwnDescriptors());
    return map->instance_descriptors()->GetFieldType(number_);
  }

  Map* GetFieldOwnerFromMap(Map* map) const {
    DCHECK(lookup_type_ == DESCRIPTOR_TYPE ||
           lookup_type_ == TRANSITION_TYPE);
    DCHECK(number_ < map->NumberOfOwnDescriptors());
    return map->FindFieldOwner(number_);
  }

  void Iterate(ObjectVisitor* visitor);

 private:
  Isolate* isolate_;
  LookupResult* next_;

  // Where did we find the result;
  enum { NOT_FOUND, DESCRIPTOR_TYPE, TRANSITION_TYPE } lookup_type_;

  JSReceiver* holder_;
  Map* transition_;
  int number_;
  PropertyDetails details_;
};


std::ostream& operator<<(std::ostream& os, const LookupResult& r);
} }  // namespace v8::internal

#endif  // V8_PROPERTY_H_
