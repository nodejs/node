// Copyright 2012 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef V8_PROPERTY_H_
#define V8_PROPERTY_H_

#include "allocation.h"
#include "transitions.h"

namespace v8 {
namespace internal {


// Abstraction for elements in instance-descriptor arrays.
//
// Each descriptor has a key, property attributes, property type,
// property index (in the actual instance-descriptor array) and
// optionally a piece of data.
//

class Descriptor BASE_EMBEDDED {
 public:
  MUST_USE_RESULT MaybeObject* KeyToUniqueName() {
    if (!key_->IsUniqueName()) {
      MaybeObject* maybe_result =
          key_->GetIsolate()->heap()->InternalizeString(String::cast(key_));
      if (!maybe_result->To(&key_)) return maybe_result;
    }
    return key_;
  }

  Name* GetKey() { return key_; }
  Object* GetValue() { return value_; }
  PropertyDetails GetDetails() { return details_; }

#ifdef OBJECT_PRINT
  void Print(FILE* out);
#endif

  void SetSortedKeyIndex(int index) { details_ = details_.set_pointer(index); }

 private:
  Name* key_;
  Object* value_;
  PropertyDetails details_;

 protected:
  Descriptor() : details_(Smi::FromInt(0)) {}

  void Init(Name* key, Object* value, PropertyDetails details) {
    key_ = key;
    value_ = value;
    details_ = details;
  }

  Descriptor(Name* key, Object* value, PropertyDetails details)
      : key_(key),
        value_(value),
        details_(details) { }

  Descriptor(Name* key,
             Object* value,
             PropertyAttributes attributes,
             PropertyType type,
             Representation representation,
             int field_index = 0)
      : key_(key),
        value_(value),
        details_(attributes, type, representation, field_index) { }

  friend class DescriptorArray;
};


class FieldDescriptor: public Descriptor {
 public:
  FieldDescriptor(Name* key,
                  int field_index,
                  PropertyAttributes attributes,
                  Representation representation)
      : Descriptor(key, Smi::FromInt(0), attributes,
                   FIELD, representation, field_index) {}
};


class ConstantDescriptor: public Descriptor {
 public:
  ConstantDescriptor(Name* key,
                     Object* value,
                     PropertyAttributes attributes)
      : Descriptor(key, value, attributes, CONSTANT,
                   value->OptimalRepresentation()) {}
};


class CallbacksDescriptor:  public Descriptor {
 public:
  CallbacksDescriptor(Name* key,
                      Object* foreign,
                      PropertyAttributes attributes)
      : Descriptor(key, foreign, attributes, CALLBACKS,
                   Representation::Tagged()) {}
};


// Holds a property index value distinguishing if it is a field index or an
// index inside the object header.
class PropertyIndex {
 public:
  static PropertyIndex NewFieldIndex(int index) {
    return PropertyIndex(index, false);
  }
  static PropertyIndex NewHeaderIndex(int index) {
    return PropertyIndex(index, true);
  }

  bool is_field_index() { return (index_ & kHeaderIndexBit) == 0; }
  bool is_header_index() { return (index_ & kHeaderIndexBit) != 0; }

  int field_index() {
    ASSERT(is_field_index());
    return value();
  }
  int header_index() {
    ASSERT(is_header_index());
    return value();
  }

  bool is_inobject(Handle<JSObject> holder) {
    if (is_header_index()) return true;
    return field_index() < holder->map()->inobject_properties();
  }

  int translate(Handle<JSObject> holder) {
    if (is_header_index()) return header_index();
    int index = field_index() - holder->map()->inobject_properties();
    if (index >= 0) return index;
    return index + holder->map()->instance_size() / kPointerSize;
  }

 private:
  static const int kHeaderIndexBit = 1 << 31;
  static const int kIndexMask = ~kHeaderIndexBit;

  int value() { return index_ & kIndexMask; }

  PropertyIndex(int index, bool is_header_based)
      : index_(index | (is_header_based ? kHeaderIndexBit : 0)) {
    ASSERT(index <= kIndexMask);
  }

  int index_;
};


class LookupResult BASE_EMBEDDED {
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
    ASSERT(isolate()->top_lookup_result() == this);
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

  bool CanHoldValue(Handle<Object> value) {
    if (IsNormal()) return true;
    ASSERT(!IsTransition());
    return value->FitsRepresentation(details_.representation());
  }

  void TransitionResult(JSObject* holder, Map* target) {
    lookup_type_ = TRANSITION_TYPE;
    details_ = PropertyDetails(NONE, TRANSITION, Representation::None());
    holder_ = holder;
    transition_ = target;
    number_ = 0xAAAA;
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
    ASSERT(IsFound());
    return JSObject::cast(holder_);
  }

  JSProxy* proxy() const {
    ASSERT(IsHandler());
    return JSProxy::cast(holder_);
  }

  PropertyType type() const {
    ASSERT(IsFound());
    return details_.type();
  }

  Representation representation() const {
    ASSERT(IsFound());
    ASSERT(!IsTransition());
    ASSERT(details_.type() != NONEXISTENT);
    return details_.representation();
  }

  PropertyAttributes GetAttributes() const {
    ASSERT(!IsTransition());
    ASSERT(IsFound());
    ASSERT(details_.type() != NONEXISTENT);
    return details_.attributes();
  }

  PropertyDetails GetPropertyDetails() const {
    ASSERT(!IsTransition());
    return details_;
  }

  bool IsFastPropertyType() const {
    ASSERT(IsFound());
    return IsTransition() || type() != NORMAL;
  }

  // Property callbacks does not include transitions to callbacks.
  bool IsPropertyCallbacks() const {
    ASSERT(!(details_.type() == CALLBACKS && !IsFound()));
    return details_.type() == CALLBACKS;
  }

  bool IsReadOnly() const {
    ASSERT(IsFound());
    ASSERT(!IsTransition());
    ASSERT(details_.type() != NONEXISTENT);
    return details_.IsReadOnly();
  }

  bool IsField() const {
    ASSERT(!(details_.type() == FIELD && !IsFound()));
    return details_.type() == FIELD;
  }

  bool IsNormal() const {
    ASSERT(!(details_.type() == NORMAL && !IsFound()));
    return details_.type() == NORMAL;
  }

  bool IsConstant() const {
    ASSERT(!(details_.type() == CONSTANT && !IsFound()));
    return details_.type() == CONSTANT;
  }

  bool IsConstantFunction() const {
    return IsConstant() && GetValue()->IsJSFunction();
  }

  bool IsDontDelete() const { return details_.IsDontDelete(); }
  bool IsDontEnum() const { return details_.IsDontEnum(); }
  bool IsFound() const { return lookup_type_ != NOT_FOUND; }
  bool IsTransition() const { return lookup_type_ == TRANSITION_TYPE; }
  bool IsHandler() const { return lookup_type_ == HANDLER_TYPE; }
  bool IsInterceptor() const { return lookup_type_ == INTERCEPTOR_TYPE; }

  // Is the result is a property excluding transitions and the null descriptor?
  bool IsProperty() const {
    return IsFound() && !IsTransition();
  }

  bool IsDataProperty() const {
    switch (type()) {
      case FIELD:
      case NORMAL:
      case CONSTANT:
        return true;
      case CALLBACKS: {
        Object* callback = GetCallbackObject();
        return callback->IsAccessorInfo() || callback->IsForeign();
      }
      case HANDLER:
      case INTERCEPTOR:
      case TRANSITION:
      case NONEXISTENT:
        return false;
    }
    UNREACHABLE();
    return false;
  }

  bool IsCacheable() const { return cacheable_; }
  void DisallowCaching() { cacheable_ = false; }

  Object* GetLazyValue() const {
    switch (type()) {
      case FIELD:
        return holder()->RawFastPropertyAt(GetFieldIndex().field_index());
      case NORMAL: {
        Object* value;
        value = holder()->property_dictionary()->ValueAt(GetDictionaryEntry());
        if (holder()->IsGlobalObject()) {
          value = PropertyCell::cast(value)->value();
        }
        return value;
      }
      case CONSTANT:
        return GetConstant();
      case CALLBACKS:
      case HANDLER:
      case INTERCEPTOR:
      case TRANSITION:
      case NONEXISTENT:
        return isolate()->heap()->the_hole_value();
    }
    UNREACHABLE();
    return NULL;
  }

  Map* GetTransitionTarget() const {
    return transition_;
  }

  PropertyDetails GetTransitionDetails() const {
    ASSERT(IsTransition());
    return transition_->GetLastDescriptorDetails();
  }

  bool IsTransitionToField() const {
    return IsTransition() && GetTransitionDetails().type() == FIELD;
  }

  bool IsTransitionToConstant() const {
    return IsTransition() && GetTransitionDetails().type() == CONSTANT;
  }

  int GetDescriptorIndex() const {
    ASSERT(lookup_type_ == DESCRIPTOR_TYPE);
    return number_;
  }

  PropertyIndex GetFieldIndex() const {
    ASSERT(lookup_type_ == DESCRIPTOR_TYPE);
    return PropertyIndex::NewFieldIndex(GetFieldIndexFromMap(holder()->map()));
  }

  int GetLocalFieldIndexFromMap(Map* map) const {
    return GetFieldIndexFromMap(map) - map->inobject_properties();
  }

  int GetDictionaryEntry() const {
    ASSERT(lookup_type_ == DICTIONARY_TYPE);
    return number_;
  }

  JSFunction* GetConstantFunction() const {
    ASSERT(type() == CONSTANT);
    return JSFunction::cast(GetValue());
  }

  Object* GetConstantFromMap(Map* map) const {
    ASSERT(type() == CONSTANT);
    return GetValueFromMap(map);
  }

  JSFunction* GetConstantFunctionFromMap(Map* map) const {
    return JSFunction::cast(GetConstantFromMap(map));
  }

  Object* GetConstant() const {
    ASSERT(type() == CONSTANT);
    return GetValue();
  }

  Object* GetCallbackObject() const {
    ASSERT(type() == CALLBACKS && !IsTransition());
    return GetValue();
  }

#ifdef OBJECT_PRINT
  void Print(FILE* out);
#endif

  Object* GetValue() const {
    if (lookup_type_ == DESCRIPTOR_TYPE) {
      return GetValueFromMap(holder()->map());
    }
    // In the dictionary case, the data is held in the value field.
    ASSERT(lookup_type_ == DICTIONARY_TYPE);
    return holder()->GetNormalizedProperty(this);
  }

  Object* GetValueFromMap(Map* map) const {
    ASSERT(lookup_type_ == DESCRIPTOR_TYPE);
    ASSERT(number_ < map->NumberOfOwnDescriptors());
    return map->instance_descriptors()->GetValue(number_);
  }

  int GetFieldIndexFromMap(Map* map) const {
    ASSERT(lookup_type_ == DESCRIPTOR_TYPE);
    ASSERT(number_ < map->NumberOfOwnDescriptors());
    return map->instance_descriptors()->GetFieldIndex(number_);
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


} }  // namespace v8::internal

#endif  // V8_PROPERTY_H_
