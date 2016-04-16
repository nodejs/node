// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_FIELD_INDEX_INL_H_
#define V8_FIELD_INDEX_INL_H_

#include "src/field-index.h"

namespace v8 {
namespace internal {


inline FieldIndex FieldIndex::ForInObjectOffset(int offset, Map* map) {
  DCHECK((offset % kPointerSize) == 0);
  int index = offset / kPointerSize;
  DCHECK(map == NULL ||
         index < (map->GetInObjectPropertyOffset(0) / kPointerSize +
                  map->GetInObjectProperties()));
  return FieldIndex(true, index, false, 0, 0, true);
}


inline FieldIndex FieldIndex::ForPropertyIndex(Map* map,
                                               int property_index,
                                               bool is_double) {
  DCHECK(map->instance_type() >= FIRST_NONSTRING_TYPE);
  int inobject_properties = map->GetInObjectProperties();
  bool is_inobject = property_index < inobject_properties;
  int first_inobject_offset;
  if (is_inobject) {
    first_inobject_offset = map->GetInObjectPropertyOffset(0);
  } else {
    first_inobject_offset = FixedArray::kHeaderSize;
    property_index -= inobject_properties;
  }
  return FieldIndex(is_inobject,
                    property_index + first_inobject_offset / kPointerSize,
                    is_double, inobject_properties, first_inobject_offset);
}


// Takes an index as computed by GetLoadFieldByIndex and reconstructs a
// FieldIndex object from it.
inline FieldIndex FieldIndex::ForLoadByFieldIndex(Map* map, int orig_index) {
  int field_index = orig_index;
  int is_inobject = true;
  bool is_double = field_index & 1;
  int first_inobject_offset = 0;
  field_index >>= 1;
  if (field_index < 0) {
    field_index = -(field_index + 1);
    is_inobject = false;
    first_inobject_offset = FixedArray::kHeaderSize;
    field_index += FixedArray::kHeaderSize / kPointerSize;
  } else {
    first_inobject_offset = map->GetInObjectPropertyOffset(0);
    field_index += JSObject::kHeaderSize / kPointerSize;
  }
  FieldIndex result(is_inobject, field_index, is_double,
                    map->GetInObjectProperties(), first_inobject_offset);
  DCHECK(result.GetLoadByFieldIndex() == orig_index);
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


inline FieldIndex FieldIndex::ForDescriptor(Map* map, int descriptor_index) {
  PropertyDetails details =
      map->instance_descriptors()->GetDetails(descriptor_index);
  int field_index = details.field_index();
  return ForPropertyIndex(map, field_index,
                          details.representation().IsDouble());
}


inline FieldIndex FieldIndex::ForKeyedLookupCacheIndex(Map* map, int index) {
  if (FLAG_compiled_keyed_generic_loads) {
    return ForLoadByFieldIndex(map, index);
  } else {
    return ForPropertyIndex(map, index);
  }
}


inline FieldIndex FieldIndex::FromFieldAccessStubKey(int key) {
  return FieldIndex(key);
}


inline int FieldIndex::GetKeyedLookupCacheIndex() const {
  if (FLAG_compiled_keyed_generic_loads) {
    return GetLoadByFieldIndex();
  } else {
    return property_index();
  }
}


}  // namespace internal
}  // namespace v8

#endif
