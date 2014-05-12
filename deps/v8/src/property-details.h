// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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

  STRING            = 8,  // Used to filter symbols and string names
  SYMBOLIC          = 16,
  PRIVATE_SYMBOL    = 32,

  DONT_SHOW         = DONT_ENUM | SYMBOLIC | PRIVATE_SYMBOL,
  ABSENT            = 64  // Used in runtime to indicate a property is absent.
  // ABSENT can never be stored in or returned from a descriptor's attributes
  // bitfield.  It is only used as a return value meaning the attributes of
  // a non-existent property.
};


namespace v8 {
namespace internal {

class Smi;
template<class> class TypeImpl;
struct ZoneTypeConfig;
typedef TypeImpl<ZoneTypeConfig> Type;
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
  CONSTANT                  = 2,
  CALLBACKS                 = 3,
  // Only in lookup results, not in descriptors.
  HANDLER                   = 4,
  INTERCEPTOR               = 5,
  // Only used as a marker in LookupResult.
  NONEXISTENT               = 6
};


class Representation {
 public:
  enum Kind {
    kNone,
    kInteger8,
    kUInteger8,
    kInteger16,
    kUInteger16,
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
  static Representation Integer8() { return Representation(kInteger8); }
  static Representation UInteger8() { return Representation(kUInteger8); }
  static Representation Integer16() { return Representation(kInteger16); }
  static Representation UInteger16() { return Representation(kUInteger16); }
  static Representation Smi() { return Representation(kSmi); }
  static Representation Integer32() { return Representation(kInteger32); }
  static Representation Double() { return Representation(kDouble); }
  static Representation HeapObject() { return Representation(kHeapObject); }
  static Representation External() { return Representation(kExternal); }

  static Representation FromKind(Kind kind) { return Representation(kind); }

  static Representation FromType(Type* type);

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
    if (kind_ == kExternal && other.kind_ == kNone) return true;
    if (kind_ == kExternal && other.kind_ == kExternal) return false;
    if (kind_ == kNone && other.kind_ == kExternal) return false;

    ASSERT(kind_ != kExternal);
    ASSERT(other.kind_ != kExternal);
    if (IsHeapObject()) return other.IsNone();
    if (kind_ == kUInteger8 && other.kind_ == kInteger8) return false;
    if (kind_ == kUInteger16 && other.kind_ == kInteger16) return false;
    return kind_ > other.kind_;
  }

  bool fits_into(const Representation& other) const {
    return other.is_more_general_than(*this) || other.Equals(*this);
  }

  bool CanContainDouble(double value);

  Representation generalize(Representation other) {
    if (other.fits_into(*this)) return *this;
    if (other.is_more_general_than(*this)) return other;
    return Representation::Tagged();
  }

  int size() const {
    ASSERT(!IsNone());
    if (IsInteger8() || IsUInteger8()) {
      return sizeof(uint8_t);
    }
    if (IsInteger16() || IsUInteger16()) {
      return sizeof(uint16_t);
    }
    if (IsInteger32()) {
      return sizeof(uint32_t);
    }
    return kPointerSize;
  }

  Kind kind() const { return static_cast<Kind>(kind_); }
  bool IsNone() const { return kind_ == kNone; }
  bool IsInteger8() const { return kind_ == kInteger8; }
  bool IsUInteger8() const { return kind_ == kUInteger8; }
  bool IsInteger16() const { return kind_ == kInteger16; }
  bool IsUInteger16() const { return kind_ == kUInteger16; }
  bool IsTagged() const { return kind_ == kTagged; }
  bool IsSmi() const { return kind_ == kSmi; }
  bool IsSmiOrTagged() const { return IsSmi() || IsTagged(); }
  bool IsInteger32() const { return kind_ == kInteger32; }
  bool IsSmiOrInteger32() const { return IsSmi() || IsInteger32(); }
  bool IsDouble() const { return kind_ == kDouble; }
  bool IsHeapObject() const { return kind_ == kHeapObject; }
  bool IsExternal() const { return kind_ == kExternal; }
  bool IsSpecialization() const {
    return IsInteger8() || IsUInteger8() ||
      IsInteger16() || IsUInteger16() ||
      IsSmi() || IsInteger32() || IsDouble();
  }
  const char* Mnemonic() const;

 private:
  explicit Representation(Kind k) : kind_(k) { }

  // Make sure kind fits in int8.
  STATIC_ASSERT(kNumRepresentations <= (1 << kBitsPerByte));

  int8_t kind_;
};


static const int kDescriptorIndexBitCount = 10;
// The maximum number of descriptors we want in a descriptor array (should
// fit in a page).
static const int kMaxNumberOfDescriptors =
    (1 << kDescriptorIndexBitCount) - 2;
static const int kInvalidEnumCacheSentinel =
    (1 << kDescriptorIndexBitCount) - 1;


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

  int pointer() const { return DescriptorPointer::decode(value_); }

  PropertyDetails set_pointer(int i) { return PropertyDetails(value_, i); }

  PropertyDetails CopyWithRepresentation(Representation representation) const {
    return PropertyDetails(value_, representation);
  }
  PropertyDetails CopyAddAttributes(PropertyAttributes new_attributes) {
    new_attributes =
        static_cast<PropertyAttributes>(attributes() | new_attributes);
    return PropertyDetails(value_, new_attributes);
  }

  // Conversion for storing details as Object*.
  explicit inline PropertyDetails(Smi* smi);
  inline Smi* AsSmi() const;

  static uint8_t EncodeRepresentation(Representation representation) {
    return representation.kind();
  }

  static Representation DecodeRepresentation(uint32_t bits) {
    return Representation::FromKind(static_cast<Representation::Kind>(bits));
  }

  PropertyType type() const { return TypeField::decode(value_); }

  PropertyAttributes attributes() const {
    return AttributesField::decode(value_);
  }

  int dictionary_index() const {
    return DictionaryStorageField::decode(value_);
  }

  Representation representation() const {
    ASSERT(type() != NORMAL);
    return DecodeRepresentation(RepresentationField::decode(value_));
  }

  int field_index() const {
    return FieldIndexField::decode(value_);
  }

  inline PropertyDetails AsDeleted() const;

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
  class RepresentationField:      public BitField<uint32_t,           6,  4> {};
  class DescriptorPointer:        public BitField<uint32_t, 10,
      kDescriptorIndexBitCount> {};  // NOLINT
  class FieldIndexField:          public BitField<uint32_t,
      10 + kDescriptorIndexBitCount,
      kDescriptorIndexBitCount> {};  // NOLINT
  // All bits for fast objects must fix in a smi.
  STATIC_ASSERT(10 + kDescriptorIndexBitCount + kDescriptorIndexBitCount <= 31);

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
