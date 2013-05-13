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
  static int IndexFromValue(Object* value) {
    return Smi::cast(value)->value();
  }

  MUST_USE_RESULT MaybeObject* KeyToUniqueName() {
    if (!key_->IsUniqueName()) {
      MaybeObject* maybe_result = HEAP->InternalizeString(String::cast(key_));
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
             Representation representation)
      : key_(key),
        value_(value),
        details_(attributes, type, representation) { }

  friend class DescriptorArray;
};


class FieldDescriptor: public Descriptor {
 public:
  FieldDescriptor(Name* key,
                  int field_index,
                  PropertyAttributes attributes,
                  Representation representation)
      : Descriptor(key, Smi::FromInt(field_index), attributes,
                   FIELD, representation) {}
};


class ConstantFunctionDescriptor: public Descriptor {
 public:
  ConstantFunctionDescriptor(Name* key,
                             JSFunction* function,
                             PropertyAttributes attributes)
      : Descriptor(key, function, attributes, CONSTANT_FUNCTION,
                   Representation::Tagged()) {}
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
        cacheable_(true),
        details_(NONE, NONEXISTENT, Representation::None()) {
    isolate->SetTopLookupResult(this);
  }

  ~LookupResult() {
    ASSERT(isolate()->top_lookup_result() == this);
    isolate()->SetTopLookupResult(next_);
  }

  Isolate* isolate() const { return isolate_; }

  void DescriptorResult(JSObject* holder, PropertyDetails details, int number) {
    lookup_type_ = DESCRIPTOR_TYPE;
    holder_ = holder;
    details_ = details;
    number_ = number;
  }

  bool CanHoldValue(Handle<Object> value) {
    return value->FitsRepresentation(details_.representation());
  }

  void TransitionResult(JSObject* holder, int number) {
    lookup_type_ = TRANSITION_TYPE;
    details_ = PropertyDetails(NONE, TRANSITION, Representation::None());
    holder_ = holder;
    number_ = number;
  }

  void DictionaryResult(JSObject* holder, int entry) {
    lookup_type_ = DICTIONARY_TYPE;
    holder_ = holder;
    details_ = holder->property_dictionary()->DetailsAt(entry);
    number_ = entry;
  }

  void HandlerResult(JSProxy* proxy) {
    lookup_type_ = HANDLER_TYPE;
    holder_ = proxy;
    details_ = PropertyDetails(NONE, HANDLER, Representation::None());
    cacheable_ = false;
  }

  void InterceptorResult(JSObject* holder) {
    lookup_type_ = INTERCEPTOR_TYPE;
    holder_ = holder;
    details_ = PropertyDetails(NONE, INTERCEPTOR, Representation::None());
  }

  void NotFound() {
    lookup_type_ = NOT_FOUND;
    details_ = PropertyDetails(NONE, NONEXISTENT, Representation::None());
    holder_ = NULL;
  }

  JSObject* holder() {
    ASSERT(IsFound());
    return JSObject::cast(holder_);
  }

  JSProxy* proxy() {
    ASSERT(IsFound());
    return JSProxy::cast(holder_);
  }

  PropertyType type() {
    ASSERT(IsFound());
    return details_.type();
  }

  Representation representation() {
    ASSERT(IsFound());
    ASSERT(!IsTransition());
    ASSERT(details_.type() != NONEXISTENT);
    return details_.representation();
  }

  PropertyAttributes GetAttributes() {
    ASSERT(!IsTransition());
    ASSERT(IsFound());
    ASSERT(details_.type() != NONEXISTENT);
    return details_.attributes();
  }

  PropertyDetails GetPropertyDetails() {
    ASSERT(!IsTransition());
    return details_;
  }

  bool IsFastPropertyType() {
    ASSERT(IsFound());
    return IsTransition() || type() != NORMAL;
  }

  // Property callbacks does not include transitions to callbacks.
  bool IsPropertyCallbacks() {
    ASSERT(!(details_.type() == CALLBACKS && !IsFound()));
    return details_.type() == CALLBACKS;
  }

  bool IsReadOnly() {
    ASSERT(IsFound());
    ASSERT(!IsTransition());
    ASSERT(details_.type() != NONEXISTENT);
    return details_.IsReadOnly();
  }

  bool IsField() {
    ASSERT(!(details_.type() == FIELD && !IsFound()));
    return details_.type() == FIELD;
  }

  bool IsNormal() {
    ASSERT(!(details_.type() == NORMAL && !IsFound()));
    return details_.type() == NORMAL;
  }

  bool IsConstantFunction() {
    ASSERT(!(details_.type() == CONSTANT_FUNCTION && !IsFound()));
    return details_.type() == CONSTANT_FUNCTION;
  }

  bool IsDontDelete() { return details_.IsDontDelete(); }
  bool IsDontEnum() { return details_.IsDontEnum(); }
  bool IsDeleted() { return details_.IsDeleted(); }
  bool IsFound() { return lookup_type_ != NOT_FOUND; }
  bool IsTransition() { return lookup_type_ == TRANSITION_TYPE; }
  bool IsHandler() { return lookup_type_ == HANDLER_TYPE; }
  bool IsInterceptor() { return lookup_type_ == INTERCEPTOR_TYPE; }

  // Is the result is a property excluding transitions and the null descriptor?
  bool IsProperty() {
    return IsFound() && !IsTransition();
  }

  bool IsDataProperty() {
    switch (type()) {
      case FIELD:
      case NORMAL:
      case CONSTANT_FUNCTION:
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

  bool IsCacheable() { return cacheable_; }
  void DisallowCaching() { cacheable_ = false; }

  Object* GetLazyValue() {
    switch (type()) {
      case FIELD:
        return holder()->RawFastPropertyAt(GetFieldIndex().field_index());
      case NORMAL: {
        Object* value;
        value = holder()->property_dictionary()->ValueAt(GetDictionaryEntry());
        if (holder()->IsGlobalObject()) {
          value = JSGlobalPropertyCell::cast(value)->value();
        }
        return value;
      }
      case CONSTANT_FUNCTION:
        return GetConstantFunction();
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

  Map* GetTransitionTarget(Map* map) {
    ASSERT(IsTransition());
    TransitionArray* transitions = map->transitions();
    return transitions->GetTarget(number_);
  }

  Map* GetTransitionTarget() {
    return GetTransitionTarget(holder()->map());
  }

  PropertyDetails GetTransitionDetails(Map* map) {
    ASSERT(IsTransition());
    TransitionArray* transitions = map->transitions();
    return transitions->GetTargetDetails(number_);
  }

  PropertyDetails GetTransitionDetails() {
    return GetTransitionDetails(holder()->map());
  }

  bool IsTransitionToField(Map* map) {
    return IsTransition() && GetTransitionDetails(map).type() == FIELD;
  }

  Map* GetTransitionMap() {
    ASSERT(IsTransition());
    return Map::cast(GetValue());
  }

  Map* GetTransitionMapFromMap(Map* map) {
    ASSERT(IsTransition());
    return map->transitions()->GetTarget(number_);
  }

  int GetTransitionIndex() {
    ASSERT(IsTransition());
    return number_;
  }

  int GetDescriptorIndex() {
    ASSERT(lookup_type_ == DESCRIPTOR_TYPE);
    return number_;
  }

  PropertyIndex GetFieldIndex() {
    ASSERT(lookup_type_ == DESCRIPTOR_TYPE);
    ASSERT(IsField());
    return PropertyIndex::NewFieldIndex(
        Descriptor::IndexFromValue(GetValue()));
  }

  int GetLocalFieldIndexFromMap(Map* map) {
    ASSERT(IsField());
    return Descriptor::IndexFromValue(GetValueFromMap(map)) -
        map->inobject_properties();
  }

  int GetDictionaryEntry() {
    ASSERT(lookup_type_ == DICTIONARY_TYPE);
    return number_;
  }

  JSFunction* GetConstantFunction() {
    ASSERT(type() == CONSTANT_FUNCTION);
    return JSFunction::cast(GetValue());
  }

  JSFunction* GetConstantFunctionFromMap(Map* map) {
    ASSERT(type() == CONSTANT_FUNCTION);
    return JSFunction::cast(GetValueFromMap(map));
  }

  Object* GetCallbackObject() {
    ASSERT(type() == CALLBACKS && !IsTransition());
    return GetValue();
  }

#ifdef OBJECT_PRINT
  void Print(FILE* out);
#endif

  Object* GetValue() {
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
  int number_;
  bool cacheable_;
  PropertyDetails details_;
};


} }  // namespace v8::internal

#endif  // V8_PROPERTY_H_
