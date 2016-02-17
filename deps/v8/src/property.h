// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_PROPERTY_H_
#define V8_PROPERTY_H_

#include <iosfwd>

#include "src/factory.h"
#include "src/field-index.h"
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


class DataDescriptor final : public Descriptor {
 public:
  DataDescriptor(Handle<Name> key, int field_index,
                 PropertyAttributes attributes, Representation representation)
      : Descriptor(key, HeapType::Any(key->GetIsolate()), attributes, DATA,
                   representation, field_index) {}
  // The field type is either a simple type or a map wrapped in a weak cell.
  DataDescriptor(Handle<Name> key, int field_index,
                 Handle<Object> wrapped_field_type,
                 PropertyAttributes attributes, Representation representation)
      : Descriptor(key, wrapped_field_type, attributes, DATA, representation,
                   field_index) {
    DCHECK(wrapped_field_type->IsSmi() || wrapped_field_type->IsWeakCell());
  }
};


class DataConstantDescriptor final : public Descriptor {
 public:
  DataConstantDescriptor(Handle<Name> key, Handle<Object> value,
                         PropertyAttributes attributes)
      : Descriptor(key, value, attributes, DATA_CONSTANT,
                   value->OptimalRepresentation()) {}
};


class AccessorConstantDescriptor final : public Descriptor {
 public:
  AccessorConstantDescriptor(Handle<Name> key, Handle<Object> foreign,
                             PropertyAttributes attributes)
      : Descriptor(key, foreign, attributes, ACCESSOR_CONSTANT,
                   Representation::Tagged()) {}
};


}  // namespace internal
}  // namespace v8

#endif  // V8_PROPERTY_H_
