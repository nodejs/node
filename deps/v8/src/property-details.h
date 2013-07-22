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

#ifndef V8_PROPERTY_DETAILS_H_
#define V8_PROPERTY_DETAILS_H_

#include "../include/v8.h"
#include "allocation.h"
#include "utils.h"

// Ecma-262 3rd 8.6.1
enum PropertyAttributes {
  NONE              = v8::None,
  READ_ONLY         = v8::ReadOnly,
  DONT_ENUM         = v8::DontEnum,
  DONT_DELETE       = v8::DontDelete,

  SEALED            = DONT_DELETE,
  FROZEN            = SEALED | READ_ONLY,

  SYMBOLIC          = 8,  // Used to filter symbol names
  DONT_SHOW         = DONT_ENUM | SYMBOLIC,
  ABSENT            = 16  // Used in runtime to indicate a property is absent.
  // ABSENT can never be stored in or returned from a descriptor's attributes
  // bitfield.  It is only used as a return value meaning the attributes of
  // a non-existent property.
};


namespace v8 {
namespace internal {

class Smi;
class Type;
class TypeInfo;

// Type of properties.
// Order of properties is significant.
// Must fit in the BitField PropertyDetails::TypeField.
// A copy of this is in mirror-debugger.js.
enum PropertyType {
  // Only in slow mode.
  NORMAL                    = 0,
  // Only in fast mode.
  FIELD                     = 1,
  CONSTANT_FUNCTION         = 2,
  CALLBACKS                 = 3,
  // Only in lookup results, not in descriptors.
  HANDLER                   = 4,
  INTERCEPTOR               = 5,
  TRANSITION                = 6,
  // Only used as a marker in LookupResult.
  NONEXISTENT               = 7
};


class Representation {
 public:
  enum Kind {
    kNone,
    kSmi,
    kInteger32,
    kDouble,
    kHeapObject,
    kTagged,
    kExternal,
    kNumRepresentations
  };

  Representation() : kind_(kNone) { }

  static Representation None() { return Representation(kNone); }
  static Representation Tagged() { return Representation(kTagged); }
  static Representation Smi() { return Representation(kSmi); }
  static Representation Integer32() { return Representation(kInteger32); }
  static Representation Double() { return Representation(kDouble); }
  static Representation HeapObject() { return Representation(kHeapObject); }
  static Representation External() { return Representation(kExternal); }

  static Representation FromKind(Kind kind) { return Representation(kind); }

  // TODO(rossberg): this should die eventually.
  static Representation FromType(TypeInfo info);
  static Representation FromType(Handle<Type> type);

  bool Equals(const Representation& other) const {
    return kind_ == other.kind_;
  }

  bool IsCompatibleForLoad(const Representation& other) const {
    return (IsDouble() && other.IsDouble()) ||
        (!IsDouble() && !other.IsDouble());
  }

  bool IsCompatibleForStore(const Representation& other) const {
    return Equals(other);
  }

  bool is_more_general_than(const Representation& other) const {
    ASSERT(kind_ != kExternal);
    ASSERT(other.kind_ != kExternal);
    if (IsHeapObject()) return other.IsDouble() || other.IsNone();
    return kind_ > other.kind_;
  }

  bool fits_into(const Representation& other) const {
    return other.is_more_general_than(*this) || other.Equals(*this);
  }

  Representation generalize(Representation other) {
    if (other.fits_into(*this)) return *this;
    if (other.is_more_general_than(*this)) return other;
    return Representation::Tagged();
  }

  Kind kind() const { return static_cast<Kind>(kind_); }
  bool IsNone() const { return kind_ == kNone; }
  bool IsTagged() const { return kind_ == kTagged; }
  bool IsSmi() const { return kind_ == kSmi; }
  bool IsSmiOrTagged() const { return IsSmi() || IsTagged(); }
  bool IsInteger32() const { return kind_ == kInteger32; }
  bool IsSmiOrInteger32() const { return IsSmi() || IsInteger32(); }
  bool IsDouble() const { return kind_ == kDouble; }
  bool IsHeapObject() const { return kind_ == kHeapObject; }
  bool IsExternal() const { return kind_ == kExternal; }
  bool IsSpecialization() const {
    return kind_ == kInteger32 || kind_ == kDouble;
  }
  const char* Mnemonic() const;

 private:
  explicit Representation(Kind k) : kind_(k) { }

  // Make sure kind fits in int8.
  STATIC_ASSERT(kNumRepresentations <= (1 << kBitsPerByte));

  int8_t kind_;
};


// PropertyDetails captures type and attributes for a property.
// They are used both in property dictionaries and instance descriptors.
class PropertyDetails BASE_EMBEDDED {
 public:
  PropertyDetails(PropertyAttributes attributes,
                  PropertyType type,
                  int index) {
    value_ = TypeField::encode(type)
        | AttributesField::encode(attributes)
        | DictionaryStorageField::encode(index);

    ASSERT(type == this->type());
    ASSERT(attributes == this->attributes());
  }

  PropertyDetails(PropertyAttributes attributes,
                  PropertyType type,
                  Representation representation,
                  int field_index = 0) {
    value_ = TypeField::encode(type)
        | AttributesField::encode(attributes)
        | RepresentationField::encode(EncodeRepresentation(representation))
        | FieldIndexField::encode(field_index);
  }

  int pointer() { return DescriptorPointer::decode(value_); }

  PropertyDetails set_pointer(int i) { return PropertyDetails(value_, i); }

  PropertyDetails CopyWithRepresentation(Representation representation) {
    return PropertyDetails(value_, representation);
  }
  PropertyDetails CopyAddAttributes(PropertyAttributes new_attributes) {
    new_attributes =
        static_cast<PropertyAttributes>(attributes() | new_attributes);
    return PropertyDetails(value_, new_attributes);
  }

  // Conversion for storing details as Object*.
  explicit inline PropertyDetails(Smi* smi);
  inline Smi* AsSmi();

  static uint8_t EncodeRepresentation(Representation representation) {
    return representation.kind();
  }

  static Representation DecodeRepresentation(uint32_t bits) {
    return Representation::FromKind(static_cast<Representation::Kind>(bits));
  }

  PropertyType type() { return TypeField::decode(value_); }

  PropertyAttributes attributes() const {
    return AttributesField::decode(value_);
  }

  int dictionary_index() {
    return DictionaryStorageField::decode(value_);
  }

  Representation representation() {
    ASSERT(type() != NORMAL);
    return DecodeRepresentation(RepresentationField::decode(value_));
  }

  int  field_index() {
    return FieldIndexField::decode(value_);
  }

  inline PropertyDetails AsDeleted();

  static bool IsValidIndex(int index) {
    return DictionaryStorageField::is_valid(index);
  }

  bool IsReadOnly() const { return (attributes() & READ_ONLY) != 0; }
  bool IsDontDelete() const { return (attributes() & DONT_DELETE) != 0; }
  bool IsDontEnum() const { return (attributes() & DONT_ENUM) != 0; }
  bool IsDeleted() const { return DeletedField::decode(value_) != 0;}

  // Bit fields in value_ (type, shift, size). Must be public so the
  // constants can be embedded in generated code.
  class TypeField:                public BitField<PropertyType,       0,  3> {};
  class AttributesField:          public BitField<PropertyAttributes, 3,  3> {};

  // Bit fields for normalized objects.
  class DeletedField:             public BitField<uint32_t,           6,  1> {};
  class DictionaryStorageField:   public BitField<uint32_t,           7, 24> {};

  // Bit fields for fast objects.
  class DescriptorPointer:        public BitField<uint32_t,           6, 11> {};
  class RepresentationField:      public BitField<uint32_t,          17,  3> {};
  class FieldIndexField:          public BitField<uint32_t,          20, 11> {};

  static const int kInitialIndex = 1;

 private:
  PropertyDetails(int value, int pointer) {
    value_ = DescriptorPointer::update(value, pointer);
  }
  PropertyDetails(int value, Representation representation) {
    value_ = RepresentationField::update(
        value, EncodeRepresentation(representation));
  }
  PropertyDetails(int value, PropertyAttributes attributes) {
    value_ = AttributesField::update(value, attributes);
  }

  uint32_t value_;
};

} }  // namespace v8::internal

#endif  // V8_PROPERTY_DETAILS_H_
