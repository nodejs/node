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
      Tagged<Map> map, int index,
      Representation representation = Representation::Tagged());
  static inline FieldIndex ForInObjectOffset(int offset, Encoding encoding);
  static inline FieldIndex ForSmiLoadHandler(Tagged<Map> map, int32_t handler);
  static inline FieldIndex ForDescriptor(Tagged<Map> map,
                                         InternalIndex descriptor_index);
  static inline FieldIndex ForDescriptor(PtrComprCageBase cage_base,
                                         Tagged<Map> map,
                                         InternalIndex descriptor_index);
  static inline FieldIndex ForDetails(Tagged<Map> map, PropertyDetails details);

  inline int GetLoadByFieldIndex() const;

  bool is_inobject() const { return IsInObjectBits::decode(bit_field_); }

  bool is_double() const { return EncodingBits::decode(bit_field_) == kDouble; }

  // Offset from beginning of the storage object. This is the JSObject for
  // in-object properties (is_inobject == true) and the PropertyArray for
  // out-of-object properties (is_inobject == false).
  int offset() const { return OffsetBits::decode(bit_field_); }

  uint64_t bit_field() const { return bit_field_; }

  // Zero-indexed from beginning of the storage object. Matches the
  // field_offset() in the PropertyDetails.
  int offset_in_words() const {
    DCHECK(IsAligned(offset(), kTaggedSize));
    return offset() / kTaggedSize;
  }

  int outobject_array_index() const {
    DCHECK(!is_inobject());
    DCHECK_EQ(
        first_field_offset_in_storage(),
        FieldStorageLocation::kFirstOutOfObjectOffsetInWords * kTaggedSize);
    return offset() / kTaggedSize -
           FieldStorageLocation::kFirstOutOfObjectOffsetInWords;
  }

  // Zero-based from the first in-object property. Overflows to out-of-object
  // properties.
  int property_index() const {
    int index = (offset() - first_field_offset_in_storage()) / kTaggedSize;
    if (!is_inobject()) {
      index += InObjectPropertyCountBits::decode(bit_field_);
    }
    return index;
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
             int inobject_property_count, int first_field_offset_in_storage) {
    DCHECK(IsAligned(first_field_offset_in_storage, kTaggedSize));
    bit_field_ =
        IsInObjectBits::encode(is_inobject) | EncodingBits::encode(encoding) |
        FirstFieldOffsetInStorageBits::encode(first_field_offset_in_storage) |
        OffsetBits::encode(offset) |
        InObjectPropertyCountBits::encode(inobject_property_count);
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

  int first_field_offset_in_storage() const {
    return FirstFieldOffsetInStorageBits::decode(bit_field_);
  }

  static constexpr int kOffsetBitsSize =
      (kDescriptorIndexBitCount + 1 + kTaggedSizeLog2);

  static constexpr int kPropertyArrayDataStartBitCount = 2 + kTaggedSizeLog2;

  // Index from beginning of object.
  using OffsetBits = base::BitField64<int, 0, kOffsetBitsSize>;
  using IsInObjectBits = OffsetBits::Next<bool, 1>;
  using EncodingBits = IsInObjectBits::Next<Encoding, 2>;
  // Number of inobject properties.
  using InObjectPropertyCountBits =
      EncodingBits::Next<int, kDescriptorIndexBitCount>;
  // Offset of first inobject property from beginning of the storage object.
  using FirstFieldOffsetInStorageBits = InObjectPropertyCountBits::Next<
      int, std::max(kPropertyArrayDataStartBitCount,
                    kFirstInobjectPropertyOffsetBitCount)>;
  static_assert(FirstFieldOffsetInStorageBits::kLastUsedBit < 64);

  uint64_t bit_field_;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_OBJECTS_FIELD_INDEX_H_
