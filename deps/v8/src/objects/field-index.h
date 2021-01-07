// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_FIELD_INDEX_H_
#define V8_OBJECTS_FIELD_INDEX_H_

// TODO(jkummerow): Consider forward-declaring instead.
#include "src/objects/internal-index.h"
#include "src/objects/property-details.h"
#include "src/utils/utils.h"

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

  static inline FieldIndex ForPropertyIndex(
      Map map, int index,
      Representation representation = Representation::Tagged());
  static inline FieldIndex ForInObjectOffset(int offset, Encoding encoding);
  static inline FieldIndex ForDescriptor(Map map,
                                         InternalIndex descriptor_index);
  static inline FieldIndex ForDescriptor(IsolateRoot isolate, Map map,
                                         InternalIndex descriptor_index);

  inline int GetLoadByFieldIndex() const;

  bool is_inobject() const { return IsInObjectBits::decode(bit_field_); }

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
             int inobject_properties, int first_inobject_property_offset) {
    DCHECK(IsAligned(first_inobject_property_offset, kTaggedSize));
    bit_field_ = IsInObjectBits::encode(is_inobject) |
                 EncodingBits::encode(encoding) |
                 FirstInobjectPropertyOffsetBits::encode(
                     first_inobject_property_offset) |
                 OffsetBits::encode(offset) |
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
    return FirstInobjectPropertyOffsetBits::decode(bit_field_);
  }

  static const int kOffsetBitsSize =
      (kDescriptorIndexBitCount + 1 + kTaggedSizeLog2);

  // Index from beginning of object.
  using OffsetBits = base::BitField64<int, 0, kOffsetBitsSize>;
  using IsInObjectBits = OffsetBits::Next<bool, 1>;
  using EncodingBits = IsInObjectBits::Next<Encoding, 2>;
  // Number of inobject properties.
  using InObjectPropertyBits =
      EncodingBits::Next<int, kDescriptorIndexBitCount>;
  // Offset of first inobject property from beginning of object.
  using FirstInobjectPropertyOffsetBits =
      InObjectPropertyBits::Next<int, kFirstInobjectPropertyOffsetBitCount>;
  STATIC_ASSERT(FirstInobjectPropertyOffsetBits::kLastUsedBit < 64);

  uint64_t bit_field_;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_OBJECTS_FIELD_INDEX_H_
