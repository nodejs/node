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
  FieldIndex() : bit_field_(0) {}

  static FieldIndex ForPropertyIndex(Map* map,
                                     int index,
                                     bool is_double = false);
  static FieldIndex ForInObjectOffset(int offset, Map* map = NULL);
  static FieldIndex ForDescriptor(Map* map, int descriptor_index);
  static FieldIndex ForLoadByFieldIndex(Map* map, int index);
  static FieldIndex ForLoadByFieldOffset(Map* map, int index);
  static FieldIndex ForKeyedLookupCacheIndex(Map* map, int index);
  static FieldIndex FromFieldAccessStubKey(int key);

  int GetLoadByFieldIndex() const;
  int GetLoadByFieldOffset() const;

  bool is_inobject() const {
    return IsInObjectBits::decode(bit_field_);
  }

  bool is_hidden_field() const { return IsHiddenField::decode(bit_field_); }

  bool is_double() const {
    return IsDoubleBits::decode(bit_field_);
  }

  int offset() const {
    return index() * kPointerSize;
  }

  // Zero-indexed from beginning of the object.
  int index() const {
    return IndexBits::decode(bit_field_);
  }

  int outobject_array_index() const {
    DCHECK(!is_inobject());
    return index() - first_inobject_property_offset() / kPointerSize;
  }

  // Zero-based from the first inobject property. Overflows to out-of-object
  // properties.
  int property_index() const {
    DCHECK(!is_hidden_field());
    int result = index() - first_inobject_property_offset() / kPointerSize;
    if (!is_inobject()) {
      result += InObjectPropertyBits::decode(bit_field_);
    }
    return result;
  }

  int GetKeyedLookupCacheIndex() const;

  int GetFieldAccessStubKey() const {
    return bit_field_ &
        (IsInObjectBits::kMask | IsDoubleBits::kMask | IndexBits::kMask);
  }

  bool operator==(FieldIndex const& other) const {
    return bit_field_ == other.bit_field_;
  }
  bool operator!=(FieldIndex const& other) const { return !(*this == other); }

 private:
  FieldIndex(bool is_inobject, int local_index, bool is_double,
             int inobject_properties, int first_inobject_property_offset,
             bool is_hidden = false) {
    DCHECK((first_inobject_property_offset & (kPointerSize - 1)) == 0);
    bit_field_ = IsInObjectBits::encode(is_inobject) |
      IsDoubleBits::encode(is_double) |
      FirstInobjectPropertyOffsetBits::encode(first_inobject_property_offset) |
      IsHiddenField::encode(is_hidden) |
      IndexBits::encode(local_index) |
      InObjectPropertyBits::encode(inobject_properties);
  }

  explicit FieldIndex(int bit_field) : bit_field_(bit_field) {}

  int first_inobject_property_offset() const {
    DCHECK(!is_hidden_field());
    return FirstInobjectPropertyOffsetBits::decode(bit_field_);
  }

  static const int kIndexBitsSize = kDescriptorIndexBitCount + 1;

  // Index from beginning of object.
  class IndexBits: public BitField<int, 0, kIndexBitsSize> {};
  class IsInObjectBits: public BitField<bool, IndexBits::kNext, 1> {};
  class IsDoubleBits: public BitField<bool, IsInObjectBits::kNext, 1> {};
  // Number of inobject properties.
  class InObjectPropertyBits
      : public BitField<int, IsDoubleBits::kNext, kDescriptorIndexBitCount> {};
  // Offset of first inobject property from beginning of object.
  class FirstInobjectPropertyOffsetBits
      : public BitField<int, InObjectPropertyBits::kNext, 7> {};
  class IsHiddenField
      : public BitField<bool, FirstInobjectPropertyOffsetBits::kNext, 1> {};
  STATIC_ASSERT(IsHiddenField::kNext <= 32);

  int bit_field_;
};

}  // namespace internal
}  // namespace v8

#endif
