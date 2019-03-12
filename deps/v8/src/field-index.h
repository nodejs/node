// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_FIELD_INDEX_H_
#define V8_FIELD_INDEX_H_

#include "src/property-details.h"
#include "src/utils.h"

namespace v8 {
namespace internal {

class Map;

// Wrapper class to hold a field index, usually but not necessarily generated
// from a property index. When available, the wrapper class captures additional
// information to allow the field index to be translated back into the property
// index it was originally generated from.
class FieldIndex final {
 public:
  enum Encoding { kTagged, kDouble, kWord32 };

  FieldIndex() : bit_field_(0) {}

  static FieldIndex ForPropertyIndex(
      const Map map, int index,
      Representation representation = Representation::Tagged());
  static FieldIndex ForInObjectOffset(int offset, Encoding encoding);
  static FieldIndex ForDescriptor(const Map map, int descriptor_index);

  int GetLoadByFieldIndex() const;

  bool is_inobject() const {
    return IsInObjectBits::decode(bit_field_);
  }

  bool is_hidden_field() const { return IsHiddenField::decode(bit_field_); }

  bool is_double() const { return EncodingBits::decode(bit_field_) == kDouble; }

  int offset() const { return OffsetBits::decode(bit_field_); }

  // Zero-indexed from beginning of the object.
  int index() const {
    DCHECK(IsAligned(offset(), kTaggedSize));
    return offset() / kTaggedSize;
  }

  int outobject_array_index() const {
    DCHECK(!is_inobject());
    return index() - first_inobject_property_offset() / kTaggedSize;
  }

  // Zero-based from the first inobject property. Overflows to out-of-object
  // properties.
  int property_index() const {
    DCHECK(!is_hidden_field());
    int result = index() - first_inobject_property_offset() / kTaggedSize;
    if (!is_inobject()) {
      result += InObjectPropertyBits::decode(bit_field_);
    }
    return result;
  }

  int GetFieldAccessStubKey() const {
    return bit_field_ &
           (IsInObjectBits::kMask | EncodingBits::kMask | OffsetBits::kMask);
  }

  bool operator==(FieldIndex const& other) const {
    return bit_field_ == other.bit_field_;
  }
  bool operator!=(FieldIndex const& other) const { return !(*this == other); }

 private:
  FieldIndex(bool is_inobject, int offset, Encoding encoding,
             int inobject_properties, int first_inobject_property_offset,
             bool is_hidden = false) {
    DCHECK(IsAligned(first_inobject_property_offset, kTaggedSize));
    bit_field_ = IsInObjectBits::encode(is_inobject) |
                 EncodingBits::encode(encoding) |
                 FirstInobjectPropertyOffsetBits::encode(
                     first_inobject_property_offset) |
                 IsHiddenField::encode(is_hidden) | OffsetBits::encode(offset) |
                 InObjectPropertyBits::encode(inobject_properties);
  }

  static Encoding FieldEncoding(Representation representation) {
    switch (representation.kind()) {
      case Representation::kNone:
      case Representation::kSmi:
      case Representation::kHeapObject:
      case Representation::kTagged:
        return kTagged;
      case Representation::kDouble:
        return kDouble;
      default:
        break;
    }
    PrintF("%s\n", representation.Mnemonic());
    UNREACHABLE();
    return kTagged;
  }

  int first_inobject_property_offset() const {
    DCHECK(!is_hidden_field());
    return FirstInobjectPropertyOffsetBits::decode(bit_field_);
  }

  static const int kOffsetBitsSize =
      (kDescriptorIndexBitCount + 1 + kTaggedSizeLog2);

  // Index from beginning of object.
  class OffsetBits : public BitField64<int, 0, kOffsetBitsSize> {};
  class IsInObjectBits : public BitField64<bool, OffsetBits::kNext, 1> {};
  class EncodingBits : public BitField64<Encoding, IsInObjectBits::kNext, 2> {};
  // Number of inobject properties.
  class InObjectPropertyBits
      : public BitField64<int, EncodingBits::kNext, kDescriptorIndexBitCount> {
  };
  // Offset of first inobject property from beginning of object.
  class FirstInobjectPropertyOffsetBits
      : public BitField64<int, InObjectPropertyBits::kNext,
                          kFirstInobjectPropertyOffsetBitCount> {};
  class IsHiddenField
      : public BitField64<bool, FirstInobjectPropertyOffsetBits::kNext, 1> {};
  STATIC_ASSERT(IsHiddenField::kNext <= 64);

  uint64_t bit_field_;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_FIELD_INDEX_H_
