// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_FIELD_INDEX_INL_H_
#define V8_FIELD_INDEX_INL_H_

#include "src/field-index.h"
#include "src/objects-inl.h"
#include "src/objects/descriptor-array-inl.h"

namespace v8 {
namespace internal {

FieldIndex FieldIndex::ForInObjectOffset(int offset, Encoding encoding) {
  DCHECK_IMPLIES(encoding == kWord32, IsAligned(offset, kInt32Size));
  DCHECK_IMPLIES(encoding == kTagged, IsAligned(offset, kTaggedSize));
  DCHECK_IMPLIES(encoding == kDouble, IsAligned(offset, kDoubleSize));
  return FieldIndex(true, offset, encoding, 0, 0);
}

FieldIndex FieldIndex::ForPropertyIndex(const Map map, int property_index,
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
    offset = PropertyArray::OffsetOfElementAt(property_index);
  }
  Encoding encoding = FieldEncoding(representation);
  return FieldIndex(is_inobject, offset, encoding, inobject_properties,
                    first_inobject_offset);
}

// Returns the index format accepted by the HLoadFieldByIndex instruction.
// (In-object: zero-based from (object start + JSObject::kHeaderSize),
// out-of-object: zero-based from FixedArray::kHeaderSize.)
int FieldIndex::GetLoadByFieldIndex() const {
  // For efficiency, the LoadByFieldIndex instruction takes an index that is
  // optimized for quick access. If the property is inline, the index is
  // positive. If it's out-of-line, the encoded index is -raw_index - 1 to
  // disambiguate the zero out-of-line index from the zero inobject case.
  // The index itself is shifted up by one bit, the lower-most bit
  // signifying if the field is a mutable double box (1) or not (0).
  int result = index();
  if (is_inobject()) {
    result -= JSObject::kHeaderSize / kTaggedSize;
  } else {
    result -= FixedArray::kHeaderSize / kTaggedSize;
    result = -result - 1;
  }
  result = static_cast<uint32_t>(result) << 1;
  return is_double() ? (result | 1) : result;
}

FieldIndex FieldIndex::ForDescriptor(const Map map, int descriptor_index) {
  PropertyDetails details =
      map->instance_descriptors()->GetDetails(descriptor_index);
  int field_index = details.field_index();
  return ForPropertyIndex(map, field_index, details.representation());
}

}  // namespace internal
}  // namespace v8

#endif  // V8_FIELD_INDEX_INL_H_
