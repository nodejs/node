// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_PROPERTY_H_
#define V8_PROPERTY_H_

#include <iosfwd>

#include "src/factory.h"
#include "src/isolate.h"

namespace v8 {
namespace internal {

// Abstraction for elements in instance-descriptor arrays.
//
// Each descriptor has a key, property attributes, property type,
// property index (in the actual instance-descriptor array) and
// optionally a piece of data.
class Descriptor final BASE_EMBEDDED {
 public:
  Descriptor() : details_(Smi::kZero) {}

  Handle<Name> GetKey() const { return key_; }
  Handle<Object> GetValue() const { return value_; }
  PropertyDetails GetDetails() const { return details_; }

  void SetSortedKeyIndex(int index) { details_ = details_.set_pointer(index); }

  static Descriptor DataField(Handle<Name> key, int field_index,
                              PropertyAttributes attributes,
                              Representation representation);

  static Descriptor DataField(Handle<Name> key, int field_index,
                              PropertyAttributes attributes,
                              PropertyConstness constness,
                              Representation representation,
                              Handle<Object> wrapped_field_type);

  static Descriptor DataConstant(Handle<Name> key, Handle<Object> value,
                                 PropertyAttributes attributes) {
    return Descriptor(key, value, kData, attributes, kDescriptor, kConst,
                      value->OptimalRepresentation(), 0);
  }

  static Descriptor DataConstant(Handle<Name> key, int field_index,
                                 Handle<Object> value,
                                 PropertyAttributes attributes);

  static Descriptor AccessorConstant(Handle<Name> key, Handle<Object> foreign,
                                     PropertyAttributes attributes) {
    return Descriptor(key, foreign, kAccessor, attributes, kDescriptor, kConst,
                      Representation::Tagged(), 0);
  }

 private:
  Handle<Name> key_;
  Handle<Object> value_;
  PropertyDetails details_;

 protected:
  void Init(Handle<Name> key, Handle<Object> value, PropertyDetails details) {
    DCHECK(key->IsUniqueName());
    DCHECK_IMPLIES(key->IsPrivate(), !details.IsEnumerable());
    key_ = key;
    value_ = value;
    details_ = details;
  }

  Descriptor(Handle<Name> key, Handle<Object> value, PropertyDetails details)
      : key_(key), value_(value), details_(details) {
    DCHECK(key->IsUniqueName());
    DCHECK_IMPLIES(key->IsPrivate(), !details_.IsEnumerable());
  }

  Descriptor(Handle<Name> key, Handle<Object> value, PropertyKind kind,
             PropertyAttributes attributes, PropertyLocation location,
             PropertyConstness constness, Representation representation,
             int field_index)
      : key_(key),
        value_(value),
        details_(kind, attributes, location, constness, representation,
                 field_index) {
    DCHECK(key->IsUniqueName());
    DCHECK_IMPLIES(key->IsPrivate(), !details_.IsEnumerable());
  }

  friend class DescriptorArray;
  friend class Map;
  friend class MapUpdater;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_PROPERTY_H_
