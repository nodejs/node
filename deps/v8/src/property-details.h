// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_PROPERTY_DETAILS_H_
#define V8_PROPERTY_DETAILS_H_

#include "include/v8.h"
#include "src/allocation.h"
#include "src/utils.h"

namespace v8 {
namespace internal {

// ES6 6.1.7.1
enum PropertyAttributes {
  NONE = ::v8::None,
  READ_ONLY = ::v8::ReadOnly,
  DONT_ENUM = ::v8::DontEnum,
  DONT_DELETE = ::v8::DontDelete,

  ALL_ATTRIBUTES_MASK = READ_ONLY | DONT_ENUM | DONT_DELETE,

  SEALED = DONT_DELETE,
  FROZEN = SEALED | READ_ONLY,

  ABSENT = 64,  // Used in runtime to indicate a property is absent.
  // ABSENT can never be stored in or returned from a descriptor's attributes
  // bitfield.  It is only used as a return value meaning the attributes of
  // a non-existent property.
};


enum PropertyFilter {
  ALL_PROPERTIES = 0,
  ONLY_WRITABLE = 1,
  ONLY_ENUMERABLE = 2,
  ONLY_CONFIGURABLE = 4,
  SKIP_STRINGS = 8,
  SKIP_SYMBOLS = 16,
  ONLY_ALL_CAN_READ = 32,
  ENUMERABLE_STRINGS = ONLY_ENUMERABLE | SKIP_SYMBOLS,
};
// Enable fast comparisons of PropertyAttributes against PropertyFilters.
STATIC_ASSERT(ALL_PROPERTIES == static_cast<PropertyFilter>(NONE));
STATIC_ASSERT(ONLY_WRITABLE == static_cast<PropertyFilter>(READ_ONLY));
STATIC_ASSERT(ONLY_ENUMERABLE == static_cast<PropertyFilter>(DONT_ENUM));
STATIC_ASSERT(ONLY_CONFIGURABLE == static_cast<PropertyFilter>(DONT_DELETE));
STATIC_ASSERT(((SKIP_STRINGS | SKIP_SYMBOLS | ONLY_ALL_CAN_READ) &
               ALL_ATTRIBUTES_MASK) == 0);
STATIC_ASSERT(ALL_PROPERTIES ==
              static_cast<PropertyFilter>(v8::PropertyFilter::ALL_PROPERTIES));
STATIC_ASSERT(ONLY_WRITABLE ==
              static_cast<PropertyFilter>(v8::PropertyFilter::ONLY_WRITABLE));
STATIC_ASSERT(ONLY_ENUMERABLE ==
              static_cast<PropertyFilter>(v8::PropertyFilter::ONLY_ENUMERABLE));
STATIC_ASSERT(ONLY_CONFIGURABLE == static_cast<PropertyFilter>(
                                       v8::PropertyFilter::ONLY_CONFIGURABLE));
STATIC_ASSERT(SKIP_STRINGS ==
              static_cast<PropertyFilter>(v8::PropertyFilter::SKIP_STRINGS));
STATIC_ASSERT(SKIP_SYMBOLS ==
              static_cast<PropertyFilter>(v8::PropertyFilter::SKIP_SYMBOLS));

class Smi;
class TypeInfo;

// Order of kinds is significant.
// Must fit in the BitField PropertyDetails::KindField.
enum PropertyKind { kData = 0, kAccessor = 1 };

// Order of modes is significant.
// Must fit in the BitField PropertyDetails::LocationField.
enum PropertyLocation { kField = 0, kDescriptor = 1 };

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

    DCHECK(kind_ != kExternal);
    DCHECK(other.kind_ != kExternal);
    if (IsHeapObject()) return other.IsNone();
    if (kind_ == kUInteger8 && other.kind_ == kInteger8) return false;
    if (kind_ == kUInteger16 && other.kind_ == kInteger16) return false;
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

  int size() const {
    DCHECK(!IsNone());
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

enum class PropertyCellType {
  // Meaningful when a property cell does not contain the hole.
  kUndefined,     // The PREMONOMORPHIC of property cells.
  kConstant,      // Cell has been assigned only once.
  kConstantType,  // Cell has been assigned only one type.
  kMutable,       // Cell will no longer be tracked as constant.

  // Meaningful when a property cell contains the hole.
  kUninitialized = kUndefined,  // Cell has never been initialized.
  kInvalidated = kConstant,     // Cell has been deleted, invalidated or never
                                // existed.

  // For dictionaries not holding cells.
  kNoCell = kMutable,
};

enum class PropertyCellConstantType {
  kSmi,
  kStableMap,
};


// PropertyDetails captures type and attributes for a property.
// They are used both in property dictionaries and instance descriptors.
class PropertyDetails BASE_EMBEDDED {
 public:
  // Property details for dictionary mode properties/elements.
  PropertyDetails(PropertyKind kind, PropertyAttributes attributes, int index,
                  PropertyCellType cell_type) {
    value_ = KindField::encode(kind) | LocationField::encode(kField) |
             AttributesField::encode(attributes) |
             DictionaryStorageField::encode(index) |
             PropertyCellTypeField::encode(cell_type);
  }

  // Property details for fast mode properties.
  PropertyDetails(PropertyKind kind, PropertyAttributes attributes,
                  PropertyLocation location, Representation representation,
                  int field_index = 0) {
    value_ = KindField::encode(kind) | LocationField::encode(location) |
             AttributesField::encode(attributes) |
             RepresentationField::encode(EncodeRepresentation(representation)) |
             FieldIndexField::encode(field_index);
  }

  static PropertyDetails Empty(
      PropertyCellType cell_type = PropertyCellType::kNoCell) {
    return PropertyDetails(kData, NONE, 0, cell_type);
  }

  int pointer() const { return DescriptorPointer::decode(value_); }

  PropertyDetails set_pointer(int i) const {
    return PropertyDetails(value_, i);
  }

  PropertyDetails set_cell_type(PropertyCellType type) const {
    PropertyDetails details = *this;
    details.value_ = PropertyCellTypeField::update(details.value_, type);
    return details;
  }

  PropertyDetails set_index(int index) const {
    PropertyDetails details = *this;
    details.value_ = DictionaryStorageField::update(details.value_, index);
    return details;
  }

  PropertyDetails CopyWithRepresentation(Representation representation) const {
    return PropertyDetails(value_, representation);
  }
  PropertyDetails CopyAddAttributes(PropertyAttributes new_attributes) const {
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

  PropertyKind kind() const { return KindField::decode(value_); }
  PropertyLocation location() const { return LocationField::decode(value_); }

  PropertyAttributes attributes() const {
    return AttributesField::decode(value_);
  }

  int dictionary_index() const {
    return DictionaryStorageField::decode(value_);
  }

  Representation representation() const {
    return DecodeRepresentation(RepresentationField::decode(value_));
  }

  int field_index() const { return FieldIndexField::decode(value_); }

  inline int field_width_in_words() const;

  static bool IsValidIndex(int index) {
    return DictionaryStorageField::is_valid(index);
  }

  bool IsReadOnly() const { return (attributes() & READ_ONLY) != 0; }
  bool IsConfigurable() const { return (attributes() & DONT_DELETE) == 0; }
  bool IsDontEnum() const { return (attributes() & DONT_ENUM) != 0; }
  bool IsEnumerable() const { return !IsDontEnum(); }
  PropertyCellType cell_type() const {
    return PropertyCellTypeField::decode(value_);
  }

  // Bit fields in value_ (type, shift, size). Must be public so the
  // constants can be embedded in generated code.
  class KindField : public BitField<PropertyKind, 0, 1> {};
  class LocationField : public BitField<PropertyLocation, 1, 1> {};
  class AttributesField : public BitField<PropertyAttributes, 2, 3> {};
  static const int kAttributesReadOnlyMask =
      (READ_ONLY << AttributesField::kShift);

  // Bit fields for normalized objects.
  class PropertyCellTypeField : public BitField<PropertyCellType, 5, 2> {};
  class DictionaryStorageField : public BitField<uint32_t, 7, 24> {};

  // Bit fields for fast objects.
  class RepresentationField : public BitField<uint32_t, 5, 4> {};
  class DescriptorPointer
      : public BitField<uint32_t, 9, kDescriptorIndexBitCount> {};  // NOLINT
  class FieldIndexField
      : public BitField<uint32_t, 9 + kDescriptorIndexBitCount,
                        kDescriptorIndexBitCount> {};  // NOLINT

  // All bits for both fast and slow objects must fit in a smi.
  STATIC_ASSERT(DictionaryStorageField::kNext <= 31);
  STATIC_ASSERT(FieldIndexField::kNext <= 31);

  static const int kInitialIndex = 1;

#ifdef OBJECT_PRINT
  // For our gdb macros, we should perhaps change these in the future.
  void Print(bool dictionary_mode);
#endif

  enum PrintMode {
    kPrintAttributes = 1 << 0,
    kPrintFieldIndex = 1 << 1,
    kPrintRepresentation = 1 << 2,
    kPrintPointer = 1 << 3,

    kForProperties = kPrintFieldIndex,
    kForTransitions = kPrintAttributes,
    kPrintFull = -1,
  };
  void PrintAsSlowTo(std::ostream& out);
  void PrintAsFastTo(std::ostream& out, PrintMode mode = kPrintFull);

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


std::ostream& operator<<(std::ostream& os,
                         const PropertyAttributes& attributes);
}  // namespace internal
}  // namespace v8

#endif  // V8_PROPERTY_DETAILS_H_
