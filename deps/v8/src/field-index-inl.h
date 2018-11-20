// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_FIELD_INDEX_INL_H_
#define V8_FIELD_INDEX_INL_H_

#include "src/field-index.h"
#include "src/objects-inl.h"
#include "src/objects/descriptor-array.h"

namespace v8 {
namespace internal {

inline FieldIndex FieldIndex::ForInObjectOffset(int offset, Encoding encoding,
                                                const Map* map) {
  DCHECK(map == nullptr || offset < map->instance_size());
  DCHECK(encoding == kWord32 ? (offset % kInt32Size) == 0
                             : (offset % kPointerSize) == 0);
  return FieldIndex(true, offset, encoding, 0, 0);
}

inline FieldIndex FieldIndex::ForPropertyIndex(const Map* map,
                                               int property_index,
                                               Representation representation) {
  DCHECK(map->instance_type() >= FIRST_NONSTRING_TYPE);
  int inobject_properties = map->GetInObjectProperties();
  bool is_inobject = property_index < inobject_properties;
  int first_inobject_offset;
  int offset;
  if (is_inobject) {
    first_inobject_offset = map->GetInObjectPropertyOffset(0);
    offset = map->GetInObjectPropertyOffset(property_index);
  } else {
    first_inobject_offset = FixedArray::kHeaderSize;
    property_index -= inobject_properties;
    offset = FixedArray::kHeaderSize + property_index * kPointerSize;
  }
  Encoding encoding = FieldEncoding(representation);
  return FieldIndex(is_inobject, offset, encoding, inobject_properties,
                    first_inobject_offset);
}

// Takes an index as computed by GetLoadByFieldIndex and reconstructs a
// FieldIndex object from it.
inline FieldIndex FieldIndex::ForLoadByFieldIndex(const Map* map,
                                                  int orig_index) {
  int field_index = orig_index;
  bool is_inobject = true;
  int first_inobject_offset = 0;
  Encoding encoding = field_index & 1 ? kDouble : kTagged;
  field_index >>= 1;
  int offset;
  if (field_index < 0) {
    first_inobject_offset = FixedArray::kHeaderSize;
    field_index = -(field_index + 1);
    is_inobject = false;
    offset = FixedArray::kHeaderSize + field_index * kPointerSize;
  } else {
    first_inobject_offset = map->GetInObjectPropertyOffset(0);
    offset = map->GetInObjectPropertyOffset(field_index);
  }
  FieldIndex result(is_inobject, offset, encoding, map->GetInObjectProperties(),
                    first_inobject_offset);
  DCHECK_EQ(result.GetLoadByFieldIndex(), orig_index);
  return result;
}


// Returns the index format accepted by the HLoadFieldByIndex instruction.
// (In-object: zero-based from (object start + JSObject::kHeaderSize),
// out-of-object: zero-based from FixedArray::kHeaderSize.)
inline int FieldIndex::GetLoadByFieldIndex() const {
  // For efficiency, the LoadByFieldIndex instruction takes an index that is
  // optimized for quick access. If the property is inline, the index is
  // positive. If it's out-of-line, the encoded index is -raw_index - 1 to
  // disambiguate the zero out-of-line index from the zero inobject case.
  // The index itself is shifted up by one bit, the lower-most bit
  // signifying if the field is a mutable double box (1) or not (0).
  int result = index();
  if (is_inobject()) {
    result -= JSObject::kHeaderSize / kPointerSize;
  } else {
    result -= FixedArray::kHeaderSize / kPointerSize;
    result = -result - 1;
  }
  result <<= 1;
  return is_double() ? (result | 1) : result;
}

inline FieldIndex FieldIndex::ForDescriptor(const Map* map,
                                            int descriptor_index) {
  PropertyDetails details =
      map->instance_descriptors()->GetDetails(descriptor_index);
  int field_index = details.field_index();
  return ForPropertyIndex(map, field_index, details.representation());
}

}  // namespace internal
}  // namespace v8

#endif  // V8_FIELD_INDEX_INL_H_
