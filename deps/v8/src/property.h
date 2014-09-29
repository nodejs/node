// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_PROPERTY_H_
#define V8_PROPERTY_H_

#include "src/factory.h"
#include "src/field-index.h"
#include "src/field-index-inl.h"
#include "src/isolate.h"
#include "src/types.h"

namespace v8 {
namespace internal {

class OStream;

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


OStream& operator<<(OStream& os, const Descriptor& d);


class FieldDescriptor V8_FINAL : public Descriptor {
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


class ConstantDescriptor V8_FINAL : public Descriptor {
 public:
  ConstantDescriptor(Handle<Name> key,
                     Handle<Object> value,
                     PropertyAttributes attributes)
      : Descriptor(key, value, attributes, CONSTANT,
                   value->OptimalRepresentation()) {}
};


class CallbacksDescriptor V8_FINAL : public Descriptor {
 public:
  CallbacksDescriptor(Handle<Name> key,
                      Handle<Object> foreign,
                      PropertyAttributes attributes)
      : Descriptor(key, foreign, attributes, CALLBACKS,
                   Representation::Tagged()) {}
};


class LookupResult V8_FINAL BASE_EMBEDDED {
 public:
  explicit LookupResult(Isolate* isolate)
      : isolate_(isolate),
        next_(isolate->top_lookup_result()),
        lookup_type_(NOT_FOUND),
        holder_(NULL),
        transition_(NULL),
        cacheable_(true),
        details_(NONE, NONEXISTENT, Representation::None()) {
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

  bool CanHoldValue(Handle<Object> value) const {
    switch (type()) {
      case NORMAL:
        return true;
      case FIELD:
        return value->FitsRepresentation(representation()) &&
            GetFieldType()->NowContains(value);
      case CONSTANT:
        DCHECK(GetConstant() != *value ||
               value->FitsRepresentation(representation()));
        return GetConstant() == *value;
      case CALLBACKS:
      case HANDLER:
      case INTERCEPTOR:
        return true;
      case NONEXISTENT:
        UNREACHABLE();
    }
    UNREACHABLE();
    return true;
  }

  void TransitionResult(JSObject* holder, Map* target) {
    lookup_type_ = TRANSITION_TYPE;
    number_ = target->LastAdded();
    details_ = target->instance_descriptors()->GetDetails(number_);
    holder_ = holder;
    transition_ = target;
  }

  void DictionaryResult(JSObject* holder, int entry) {
    lookup_type_ = DICTIONARY_TYPE;
    holder_ = holder;
    transition_ = NULL;
    details_ = holder->property_dictionary()->DetailsAt(entry);
    number_ = entry;
  }

  void HandlerResult(JSProxy* proxy) {
    lookup_type_ = HANDLER_TYPE;
    holder_ = proxy;
    transition_ = NULL;
    details_ = PropertyDetails(NONE, HANDLER, Representation::Tagged());
    cacheable_ = false;
  }

  void InterceptorResult(JSObject* holder) {
    lookup_type_ = INTERCEPTOR_TYPE;
    holder_ = holder;
    transition_ = NULL;
    details_ = PropertyDetails(NONE, INTERCEPTOR, Representation::Tagged());
  }

  void NotFound() {
    lookup_type_ = NOT_FOUND;
    details_ = PropertyDetails(NONE, NONEXISTENT, Representation::None());
    holder_ = NULL;
    transition_ = NULL;
  }

  JSObject* holder() const {
    DCHECK(IsFound());
    return JSObject::cast(holder_);
  }

  JSProxy* proxy() const {
    DCHECK(IsHandler());
    return JSProxy::cast(holder_);
  }

  PropertyType type() const {
    DCHECK(IsFound());
    return details_.type();
  }

  Representation representation() const {
    DCHECK(IsFound());
    DCHECK(details_.type() != NONEXISTENT);
    return details_.representation();
  }

  PropertyAttributes GetAttributes() const {
    DCHECK(IsFound());
    DCHECK(details_.type() != NONEXISTENT);
    return details_.attributes();
  }

  PropertyDetails GetPropertyDetails() const {
    return details_;
  }

  bool IsFastPropertyType() const {
    DCHECK(IsFound());
    return IsTransition() || type() != NORMAL;
  }

  // Property callbacks does not include transitions to callbacks.
  bool IsPropertyCallbacks() const {
    DCHECK(!(details_.type() == CALLBACKS && !IsFound()));
    return !IsTransition() && details_.type() == CALLBACKS;
  }

  bool IsReadOnly() const {
    DCHECK(IsFound());
    DCHECK(details_.type() != NONEXISTENT);
    return details_.IsReadOnly();
  }

  bool IsField() const {
    DCHECK(!(details_.type() == FIELD && !IsFound()));
    return IsDescriptorOrDictionary() && type() == FIELD;
  }

  bool IsNormal() const {
    DCHECK(!(details_.type() == NORMAL && !IsFound()));
    return IsDescriptorOrDictionary() && type() == NORMAL;
  }

  bool IsConstant() const {
    DCHECK(!(details_.type() == CONSTANT && !IsFound()));
    return IsDescriptorOrDictionary() && type() == CONSTANT;
  }

  bool IsConstantFunction() const {
    return IsConstant() && GetConstant()->IsJSFunction();
  }

  bool IsDontDelete() const { return details_.IsDontDelete(); }
  bool IsDontEnum() const { return details_.IsDontEnum(); }
  bool IsFound() const { return lookup_type_ != NOT_FOUND; }
  bool IsDescriptorOrDictionary() const {
    return lookup_type_ == DESCRIPTOR_TYPE || lookup_type_ == DICTIONARY_TYPE;
  }
  bool IsTransition() const { return lookup_type_ == TRANSITION_TYPE; }
  bool IsHandler() const { return lookup_type_ == HANDLER_TYPE; }
  bool IsInterceptor() const { return lookup_type_ == INTERCEPTOR_TYPE; }

  // Is the result is a property excluding transitions and the null descriptor?
  bool IsProperty() const {
    return IsFound() && !IsTransition();
  }

  bool IsDataProperty() const {
    switch (lookup_type_) {
      case NOT_FOUND:
      case TRANSITION_TYPE:
      case HANDLER_TYPE:
      case INTERCEPTOR_TYPE:
        return false;

      case DESCRIPTOR_TYPE:
      case DICTIONARY_TYPE:
        switch (type()) {
          case FIELD:
          case NORMAL:
          case CONSTANT:
            return true;
          case CALLBACKS: {
            Object* callback = GetCallbackObject();
            DCHECK(!callback->IsForeign());
            return callback->IsAccessorInfo();
          }
          case HANDLER:
          case INTERCEPTOR:
          case NONEXISTENT:
            UNREACHABLE();
            return false;
        }
    }
    UNREACHABLE();
    return false;
  }

  bool IsCacheable() const { return cacheable_; }
  void DisallowCaching() { cacheable_ = false; }

  Object* GetLazyValue() const {
    switch (lookup_type_) {
      case NOT_FOUND:
      case TRANSITION_TYPE:
      case HANDLER_TYPE:
      case INTERCEPTOR_TYPE:
        return isolate()->heap()->the_hole_value();

      case DESCRIPTOR_TYPE:
      case DICTIONARY_TYPE:
        switch (type()) {
          case FIELD:
            return holder()->RawFastPropertyAt(GetFieldIndex());
          case NORMAL: {
            Object* value = holder()->property_dictionary()->ValueAt(
                GetDictionaryEntry());
            if (holder()->IsGlobalObject()) {
              value = PropertyCell::cast(value)->value();
            }
            return value;
          }
          case CONSTANT:
            return GetConstant();
          case CALLBACKS:
            return isolate()->heap()->the_hole_value();
          case HANDLER:
          case INTERCEPTOR:
          case NONEXISTENT:
            UNREACHABLE();
            return NULL;
        }
    }
    UNREACHABLE();
    return NULL;
  }

  Map* GetTransitionTarget() const {
    DCHECK(IsTransition());
    return transition_;
  }

  bool IsTransitionToField() const {
    return IsTransition() && details_.type() == FIELD;
  }

  bool IsTransitionToConstant() const {
    return IsTransition() && details_.type() == CONSTANT;
  }

  int GetDescriptorIndex() const {
    DCHECK(lookup_type_ == DESCRIPTOR_TYPE);
    return number_;
  }

  FieldIndex GetFieldIndex() const {
    DCHECK(lookup_type_ == DESCRIPTOR_TYPE ||
           lookup_type_ == TRANSITION_TYPE);
    return FieldIndex::ForLookupResult(this);
  }

  int GetLocalFieldIndexFromMap(Map* map) const {
    return GetFieldIndexFromMap(map) - map->inobject_properties();
  }

  int GetDictionaryEntry() const {
    DCHECK(lookup_type_ == DICTIONARY_TYPE);
    return number_;
  }

  JSFunction* GetConstantFunction() const {
    DCHECK(type() == CONSTANT);
    return JSFunction::cast(GetValue());
  }

  Object* GetConstantFromMap(Map* map) const {
    DCHECK(type() == CONSTANT);
    return GetValueFromMap(map);
  }

  JSFunction* GetConstantFunctionFromMap(Map* map) const {
    return JSFunction::cast(GetConstantFromMap(map));
  }

  Object* GetConstant() const {
    DCHECK(type() == CONSTANT);
    return GetValue();
  }

  Object* GetCallbackObject() const {
    DCHECK(!IsTransition());
    DCHECK(type() == CALLBACKS);
    return GetValue();
  }

  Object* GetValue() const {
    if (lookup_type_ == DESCRIPTOR_TYPE) {
      return GetValueFromMap(holder()->map());
    } else if (lookup_type_ == TRANSITION_TYPE) {
      return GetValueFromMap(transition_);
    }
    // In the dictionary case, the data is held in the value field.
    DCHECK(lookup_type_ == DICTIONARY_TYPE);
    return holder()->GetNormalizedProperty(this);
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

  HeapType* GetFieldType() const {
    DCHECK(type() == FIELD);
    if (lookup_type_ == DESCRIPTOR_TYPE) {
      return GetFieldTypeFromMap(holder()->map());
    }
    DCHECK(lookup_type_ == TRANSITION_TYPE);
    return GetFieldTypeFromMap(transition_);
  }

  HeapType* GetFieldTypeFromMap(Map* map) const {
    DCHECK(lookup_type_ == DESCRIPTOR_TYPE ||
           lookup_type_ == TRANSITION_TYPE);
    DCHECK(number_ < map->NumberOfOwnDescriptors());
    return map->instance_descriptors()->GetFieldType(number_);
  }

  Map* GetFieldOwner() const {
    return GetFieldOwnerFromMap(holder()->map());
  }

  Map* GetFieldOwnerFromMap(Map* map) const {
    DCHECK(lookup_type_ == DESCRIPTOR_TYPE ||
           lookup_type_ == TRANSITION_TYPE);
    DCHECK(number_ < map->NumberOfOwnDescriptors());
    return map->FindFieldOwner(number_);
  }

  bool ReceiverIsHolder(Handle<Object> receiver) {
    if (*receiver == holder()) return true;
    if (lookup_type_ == TRANSITION_TYPE) return true;
    return false;
  }

  void Iterate(ObjectVisitor* visitor);

 private:
  Isolate* isolate_;
  LookupResult* next_;

  // Where did we find the result;
  enum {
    NOT_FOUND,
    DESCRIPTOR_TYPE,
    TRANSITION_TYPE,
    DICTIONARY_TYPE,
    HANDLER_TYPE,
    INTERCEPTOR_TYPE
  } lookup_type_;

  JSReceiver* holder_;
  Map* transition_;
  int number_;
  bool cacheable_;
  PropertyDetails details_;
};


OStream& operator<<(OStream& os, const LookupResult& r);
} }  // namespace v8::internal

#endif  // V8_PROPERTY_H_
