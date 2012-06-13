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

  MUST_USE_RESULT MaybeObject* KeyToSymbol() {
    if (!StringShape(key_).IsSymbol()) {
      MaybeObject* maybe_result = HEAP->LookupSymbol(key_);
      if (!maybe_result->To(&key_)) return maybe_result;
    }
    return key_;
  }

  String* GetKey() { return key_; }
  Object* GetValue() { return value_; }
  PropertyDetails GetDetails() { return details_; }

#ifdef OBJECT_PRINT
  void Print(FILE* out);
#endif

  void SetEnumerationIndex(int index) {
    ASSERT(PropertyDetails::IsValidIndex(index));
    details_ = PropertyDetails(details_.attributes(), details_.type(), index);
  }

  bool ContainsTransition();

 private:
  String* key_;
  Object* value_;
  PropertyDetails details_;

 protected:
  Descriptor() : details_(Smi::FromInt(0)) {}

  void Init(String* key, Object* value, PropertyDetails details) {
    key_ = key;
    value_ = value;
    details_ = details;
  }

  Descriptor(String* key, Object* value, PropertyDetails details)
      : key_(key),
        value_(value),
        details_(details) { }

  Descriptor(String* key,
             Object* value,
             PropertyAttributes attributes,
             PropertyType type,
             int index = 0)
      : key_(key),
        value_(value),
        details_(attributes, type, index) { }

  friend class DescriptorArray;
};

// A pointer from a map to the new map that is created by adding
// a named property.  These are key to the speed and functioning of V8.
// The two maps should always have the same prototype, since
// MapSpace::CreateBackPointers depends on this.
class MapTransitionDescriptor: public Descriptor {
 public:
  MapTransitionDescriptor(String* key, Map* map, PropertyAttributes attributes)
      : Descriptor(key, map, attributes, MAP_TRANSITION) { }
};

// Marks a field name in a map so that adding the field is guaranteed
// to create a FIELD descriptor in the new map.  Used after adding
// a constant function the first time, creating a CONSTANT_FUNCTION
// descriptor in the new map.  This avoids creating multiple maps with
// the same CONSTANT_FUNCTION field.
class ConstTransitionDescriptor: public Descriptor {
 public:
  explicit ConstTransitionDescriptor(String* key, Map* map)
      : Descriptor(key, map, NONE, CONSTANT_TRANSITION) { }
};


class FieldDescriptor: public Descriptor {
 public:
  FieldDescriptor(String* key,
                  int field_index,
                  PropertyAttributes attributes,
                  int index = 0)
      : Descriptor(key, Smi::FromInt(field_index), attributes, FIELD, index) {}
};


class ConstantFunctionDescriptor: public Descriptor {
 public:
  ConstantFunctionDescriptor(String* key,
                             JSFunction* function,
                             PropertyAttributes attributes,
                             int index = 0)
      : Descriptor(key, function, attributes, CONSTANT_FUNCTION, index) {}
};


class CallbacksDescriptor:  public Descriptor {
 public:
  CallbacksDescriptor(String* key,
                      Object* foreign,
                      PropertyAttributes attributes,
                      int index = 0)
      : Descriptor(key, foreign, attributes, CALLBACKS, index) {}
};


template <class T>
bool IsPropertyDescriptor(T* desc) {
  switch (desc->type()) {
    case NORMAL:
    case FIELD:
    case CONSTANT_FUNCTION:
    case HANDLER:
    case INTERCEPTOR:
      return true;
    case CALLBACKS: {
      Object* callback_object = desc->GetCallbackObject();
      // Non-JavaScript (i.e. native) accessors are always a property, otherwise
      // either the getter or the setter must be an accessor. Put another way:
      // If we only see map transitions and holes in a pair, this is not a
      // property.
      return (!callback_object->IsAccessorPair() ||
              AccessorPair::cast(callback_object)->ContainsAccessor());
    }
    case MAP_TRANSITION:
    case CONSTANT_TRANSITION:
    case NULL_DESCRIPTOR:
      return false;
  }
  UNREACHABLE();  // keep the compiler happy
  return false;
}


class LookupResult BASE_EMBEDDED {
 public:
  explicit LookupResult(Isolate* isolate)
      : isolate_(isolate),
        next_(isolate->top_lookup_result()),
        lookup_type_(NOT_FOUND),
        holder_(NULL),
        cacheable_(true),
        details_(NONE, NORMAL) {
    isolate->SetTopLookupResult(this);
  }

  ~LookupResult() {
    ASSERT(isolate_->top_lookup_result() == this);
    isolate_->SetTopLookupResult(next_);
  }

  void DescriptorResult(JSObject* holder, PropertyDetails details, int number) {
    lookup_type_ = DESCRIPTOR_TYPE;
    holder_ = holder;
    details_ = details;
    number_ = number;
  }

  void ConstantResult(JSObject* holder) {
    lookup_type_ = CONSTANT_TYPE;
    holder_ = holder;
    details_ =
        PropertyDetails(static_cast<PropertyAttributes>(DONT_ENUM |
                                                        DONT_DELETE),
                        CALLBACKS);
    number_ = -1;
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
    details_ = PropertyDetails(NONE, HANDLER);
    cacheable_ = false;
  }

  void InterceptorResult(JSObject* holder) {
    lookup_type_ = INTERCEPTOR_TYPE;
    holder_ = holder;
    details_ = PropertyDetails(NONE, INTERCEPTOR);
  }

  void NotFound() {
    lookup_type_ = NOT_FOUND;
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

  PropertyAttributes GetAttributes() {
    ASSERT(IsFound());
    return details_.attributes();
  }

  PropertyDetails GetPropertyDetails() {
    return details_;
  }

  bool IsReadOnly() { return details_.IsReadOnly(); }
  bool IsDontDelete() { return details_.IsDontDelete(); }
  bool IsDontEnum() { return details_.IsDontEnum(); }
  bool IsDeleted() { return details_.IsDeleted(); }
  bool IsFound() { return lookup_type_ != NOT_FOUND; }
  bool IsHandler() { return lookup_type_ == HANDLER_TYPE; }

  // Is the result is a property excluding transitions and the null descriptor?
  bool IsProperty() {
    return IsFound() && IsPropertyDescriptor(this);
  }

  bool IsCacheable() { return cacheable_; }
  void DisallowCaching() { cacheable_ = false; }

  Object* GetLazyValue() {
    switch (type()) {
      case FIELD:
        return holder()->FastPropertyAt(GetFieldIndex());
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
      default:
        return Smi::FromInt(0);
    }
  }


  Map* GetTransitionMap() {
    ASSERT(lookup_type_ == DESCRIPTOR_TYPE);
    ASSERT(type() == MAP_TRANSITION ||
           type() == CONSTANT_TRANSITION);
    return Map::cast(GetValue());
  }

  Map* GetTransitionMapFromMap(Map* map) {
    ASSERT(lookup_type_ == DESCRIPTOR_TYPE);
    ASSERT(type() == MAP_TRANSITION);
    return Map::cast(map->instance_descriptors()->GetValue(number_));
  }

  int GetFieldIndex() {
    ASSERT(lookup_type_ == DESCRIPTOR_TYPE);
    ASSERT(type() == FIELD);
    return Descriptor::IndexFromValue(GetValue());
  }

  int GetLocalFieldIndexFromMap(Map* map) {
    ASSERT(lookup_type_ == DESCRIPTOR_TYPE);
    ASSERT(type() == FIELD);
    return Descriptor::IndexFromValue(
        map->instance_descriptors()->GetValue(number_)) -
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
    ASSERT(lookup_type_ == DESCRIPTOR_TYPE);
    ASSERT(type() == CONSTANT_FUNCTION);
    return JSFunction::cast(map->instance_descriptors()->GetValue(number_));
  }

  Object* GetCallbackObject() {
    if (lookup_type_ == CONSTANT_TYPE) {
      // For now we only have the __proto__ as constant type.
      return HEAP->prototype_accessors();
    }
    return GetValue();
  }

#ifdef OBJECT_PRINT
  void Print(FILE* out);
#endif

  Object* GetValue() {
    if (lookup_type_ == DESCRIPTOR_TYPE) {
      DescriptorArray* descriptors = holder()->map()->instance_descriptors();
      return descriptors->GetValue(number_);
    }
    // In the dictionary case, the data is held in the value field.
    ASSERT(lookup_type_ == DICTIONARY_TYPE);
    return holder()->GetNormalizedProperty(this);
  }

  void Iterate(ObjectVisitor* visitor);

 private:
  Isolate* isolate_;
  LookupResult* next_;

  // Where did we find the result;
  enum {
    NOT_FOUND,
    DESCRIPTOR_TYPE,
    DICTIONARY_TYPE,
    HANDLER_TYPE,
    INTERCEPTOR_TYPE,
    CONSTANT_TYPE
  } lookup_type_;

  JSReceiver* holder_;
  int number_;
  bool cacheable_;
  PropertyDetails details_;
};


} }  // namespace v8::internal

#endif  // V8_PROPERTY_H_
