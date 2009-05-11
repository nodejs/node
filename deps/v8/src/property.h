// Copyright 2006-2008 the V8 project authors. All rights reserved.
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

namespace v8 { namespace internal {


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

  Object* KeyToSymbol() {
    if (!StringShape(key_).IsSymbol()) {
      Object* result = Heap::LookupSymbol(key_);
      if (result->IsFailure()) return result;
      key_ = String::cast(result);
    }
    return key_;
  }

  String* GetKey() { return key_; }
  Object* GetValue() { return value_; }
  PropertyDetails GetDetails() { return details_; }

#ifdef DEBUG
  void Print();
#endif

  void SetEnumerationIndex(int index) {
    ASSERT(PropertyDetails::IsValidIndex(index));
    details_ = PropertyDetails(details_.attributes(), details_.type(), index);
  }

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

  friend class DescriptorWriter;
  friend class DescriptorReader;
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
  explicit ConstTransitionDescriptor(String* key)
      : Descriptor(key, Smi::FromInt(0), NONE, CONSTANT_TRANSITION) { }
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
                      Object* proxy,
                      PropertyAttributes attributes,
                      int index = 0)
      : Descriptor(key, proxy, attributes, CALLBACKS, index) {}
};


class LookupResult BASE_EMBEDDED {
 public:
  // Where did we find the result;
  enum {
    NOT_FOUND,
    DESCRIPTOR_TYPE,
    DICTIONARY_TYPE,
    INTERCEPTOR_TYPE,
    CONSTANT_TYPE
  } lookup_type_;

  LookupResult()
      : lookup_type_(NOT_FOUND),
        cacheable_(true),
        details_(NONE, NORMAL) {}

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

  void InterceptorResult(JSObject* holder) {
    lookup_type_ = INTERCEPTOR_TYPE;
    holder_ = holder;
    details_ = PropertyDetails(NONE, INTERCEPTOR);
  }

  void NotFound() {
    lookup_type_ = NOT_FOUND;
  }

  JSObject* holder() {
    ASSERT(IsValid());
    return holder_;
  }

  PropertyType type() {
    ASSERT(IsValid());
    return details_.type();
  }

  bool IsTransitionType() {
    PropertyType t = type();
    if (t == MAP_TRANSITION || t == CONSTANT_TRANSITION) return true;
    return false;
  }

  PropertyAttributes GetAttributes() {
    ASSERT(IsValid());
    return details_.attributes();
  }

  PropertyDetails GetPropertyDetails() {
    return details_;
  }

  bool IsReadOnly() { return details_.IsReadOnly(); }
  bool IsDontDelete() { return details_.IsDontDelete(); }
  bool IsDontEnum() { return details_.IsDontEnum(); }

  bool IsValid() { return  lookup_type_ != NOT_FOUND; }
  bool IsNotFound() { return lookup_type_ == NOT_FOUND; }

  // Tells whether the result is a property.
  // Excluding transitions and the null descriptor.
  bool IsProperty() {
    return IsValid() && type() < FIRST_PHANTOM_PROPERTY_TYPE;
  }

  bool IsCacheable() { return cacheable_; }
  void DisallowCaching() { cacheable_ = false; }

  // Tells whether the value needs to be loaded.
  bool IsLoaded() {
    if (lookup_type_ == DESCRIPTOR_TYPE || lookup_type_ == DICTIONARY_TYPE) {
      Object* target = GetLazyValue();
      return !target->IsJSObject() || JSObject::cast(target)->IsLoaded();
    }
    return true;
  }

  Object* GetLazyValue() {
    switch (type()) {
      case FIELD:
        return holder()->FastPropertyAt(GetFieldIndex());
      case NORMAL:
        return holder()->property_dictionary()->ValueAt(GetDictionaryEntry());
      case CONSTANT_FUNCTION:
        return GetConstantFunction();
      default:
        return Smi::FromInt(0);
    }
  }

  Map* GetTransitionMap() {
    ASSERT(lookup_type_ == DESCRIPTOR_TYPE);
    ASSERT(type() == MAP_TRANSITION);
    return Map::cast(GetValue());
  }

  int GetFieldIndex() {
    ASSERT(lookup_type_ == DESCRIPTOR_TYPE);
    ASSERT(type() == FIELD);
    return Descriptor::IndexFromValue(GetValue());
  }

  int GetDictionaryEntry() {
    ASSERT(lookup_type_ == DICTIONARY_TYPE);
    return number_;
  }

  JSFunction* GetConstantFunction() {
    ASSERT(type() == CONSTANT_FUNCTION);
    return JSFunction::cast(GetValue());
  }

  Object* GetCallbackObject() {
    if (lookup_type_ == CONSTANT_TYPE) {
      // For now we only have the __proto__ as constant type.
      return Heap::prototype_accessors();
    }
    return GetValue();
  }

#ifdef DEBUG
  void Print();
#endif

  Object* GetValue() {
    if (lookup_type_ == DESCRIPTOR_TYPE) {
      DescriptorArray* descriptors = holder()->map()->instance_descriptors();
      return descriptors->GetValue(number_);
    }
    // In the dictionary case, the data is held in the value field.
    ASSERT(lookup_type_ == DICTIONARY_TYPE);
    return holder()->property_dictionary()->ValueAt(GetDictionaryEntry());
  }

 private:
  JSObject* holder_;
  int number_;
  bool cacheable_;
  PropertyDetails details_;
};


// The DescriptorStream is an abstraction for iterating over a map's
// instance descriptors.
class DescriptorStream BASE_EMBEDDED {
 public:
  explicit DescriptorStream(DescriptorArray* descriptors, int pos) {
    descriptors_ = descriptors;
    pos_ = pos;
    limit_ = descriptors_->number_of_descriptors();
  }

  // Tells whether we have reached the end of the steam.
  bool eos() { return pos_ >= limit_; }

  int next_position() { return pos_ + 1; }
  void advance() { pos_ = next_position(); }

 protected:
  DescriptorArray* descriptors_;
  int pos_;   // Current position.
  int limit_;  // Limit for position.
};


class DescriptorReader: public DescriptorStream {
 public:
  explicit DescriptorReader(DescriptorArray* descriptors, int pos = 0)
      : DescriptorStream(descriptors, pos) {}

  String* GetKey() { return descriptors_->GetKey(pos_); }
  Object* GetValue() { return descriptors_->GetValue(pos_); }
  PropertyDetails GetDetails() {
    return PropertyDetails(descriptors_->GetDetails(pos_));
  }

  int GetFieldIndex() { return Descriptor::IndexFromValue(GetValue()); }

  bool IsDontEnum() { return GetDetails().IsDontEnum(); }

  PropertyType type() { return GetDetails().type(); }

  // Tells whether the type is a transition.
  bool IsTransition() {
    PropertyType t = type();
    ASSERT(t != INTERCEPTOR);
    return t == MAP_TRANSITION || t == CONSTANT_TRANSITION;
  }

  bool IsNullDescriptor() {
    return type() == NULL_DESCRIPTOR;
  }

  bool IsProperty() {
    return type() < FIRST_PHANTOM_PROPERTY_TYPE;
  }

  JSFunction* GetConstantFunction() { return JSFunction::cast(GetValue()); }

  AccessorDescriptor* GetCallbacks() {
    ASSERT(type() == CALLBACKS);
    Proxy* p = Proxy::cast(GetCallbacksObject());
    return reinterpret_cast<AccessorDescriptor*>(p->proxy());
  }

  Object* GetCallbacksObject() {
    ASSERT(type() == CALLBACKS);
    return GetValue();
  }

  bool Equals(String* name) { return name->Equals(GetKey()); }

  void Get(Descriptor* desc) {
    descriptors_->Get(pos_, desc);
  }
};

class DescriptorWriter: public DescriptorStream {
 public:
  explicit DescriptorWriter(DescriptorArray* descriptors)
      : DescriptorStream(descriptors, 0) {}

  // Append a descriptor to this stream.
  void Write(Descriptor* desc);
  // Read a descriptor from the reader and append it to this stream.
  void WriteFrom(DescriptorReader* reader);
};

} }  // namespace v8::internal

#endif  // V8_PROPERTY_H_
