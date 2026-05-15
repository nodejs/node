// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_FIELD_INDEX_INL_H_
#define V8_OBJECTS_FIELD_INDEX_INL_H_

#include "src/objects/field-index.h"
// Include the non-inl header before the rest of the headers.

#include "src/ic/handler-configuration.h"
#include "src/objects/descriptor-array-inl.h"
#include "src/objects/map-inl.h"
#include "src/objects/objects-inl.h"
#include "src/objects/tagged-field.h"

namespace v8 {
namespace internal {

FieldIndex FieldIndex::ForInObjectOffset(int offset, Encoding encoding) {
  DCHECK_IMPLIES(encoding == kWord32, IsAligned(offset, kInt32Size));
  DCHECK_IMPLIES(encoding == kTagged, IsAligned(offset, kTaggedSize));
  DCHECK_IMPLIES(encoding == kDouble, IsAligned(offset, kDoubleSize));
  return FieldIndex(true, offset, encoding, 0, 0);
}

FieldIndex FieldIndex::ForSmiLoadHandler(Tagged<Map> map, int32_t handler) {
  DCHECK_EQ(LoadHandler::KindBits::decode(handler), LoadHandler::Kind::kField);

  bool is_inobject = LoadHandler::IsInobjectBits::decode(handler);
  int inobject_property_count = map->GetInObjectProperties();
  int first_field_offset_in_storage;
  if (is_inobject) {
    first_field_offset_in_storage = map->GetInObjectPropertyOffset(0);
  } else {
    first_field_offset_in_storage = PropertyArray::kHeaderSize;
  }
  return FieldIndex(
      is_inobject,
      LoadHandler::StorageOffsetInWordsBits::decode(handler) * kTaggedSize,
      LoadHandler::IsDoubleBits::decode(handler) ? kDouble : kTagged,
      inobject_property_count, first_field_offset_in_storage);
}

FieldIndex FieldIndex::ForPropertyIndex(Tagged<Map> map, int property_index,
                                        Representation representation) {
  DCHECK(map->instance_type() >= FIRST_NONSTRING_TYPE);
  int inobject_property_count = map->GetInObjectProperties();
  bool is_inobject = property_index < inobject_property_count;
  int first_field_offset_in_storage;
  int offset;
  if (is_inobject) {
    first_field_offset_in_storage = map->GetInObjectPropertyOffset(0);
    offset = map->GetInObjectPropertyOffset(property_index);
  } else {
    // Have to have these static asserts somewhere, why not here.
    static_assert(PropertyArray::kHeaderSize <
                  (1 << FieldIndex::kPropertyArrayDataStartBitCount));
    static_assert(PropertyArray::kHeaderSize >=
                  (1 << (FieldIndex::kPropertyArrayDataStartBitCount - 1)));

    first_field_offset_in_storage = PropertyArray::kHeaderSize;
    property_index -= inobject_property_count;
    offset = PropertyArray::OffsetOfElementAt(property_index);
  }
  Encoding encoding = FieldEncoding(representation);
  return FieldIndex(is_inobject, offset, encoding, inobject_property_count,
                    first_field_offset_in_storage);
}

// Returns the index format accepted by the LoadFieldByIndex instruction.
// (In-object: zero-based from (object start + JSObject::kHeaderSize),
// out-of-object: zero-based from PropertyArray::kHeaderSize.)
int FieldIndex::GetLoadByFieldIndex() const {
  // For efficiency, the LoadByFieldIndex instruction takes an index that is
  // optimized for quick access. If the property is inline, the index is
  // positive. If it's out-of-line, the encoded index is -raw_index - 1 to
  // disambiguate the zero out-of-line index from the zero inobject case.
  // The index itself is shifted up by one bit, the lower-most bit
  // signifying if the field is a mutable double box (1) or not (0).
  int result = offset_in_words();
  if (is_inobject()) {
    result -= JSObject::kHeaderSize / kTaggedSize;
  } else {
    result -= PropertyArray::kHeaderSize / kTaggedSize;
    result = -result - 1;
  }
  result = static_cast<uint32_t>(result) << 1;
  return is_double() ? (result | 1) : result;
}

FieldIndex FieldIndex::ForDescriptor(Tagged<Map> map,
                                     InternalIndex descriptor_index) {
  PtrComprCageBase cage_base = GetPtrComprCageBase(map);
  return ForDescriptor(cage_base, map, descriptor_index);
}

FieldIndex FieldIndex::ForDescriptor(PtrComprCageBase cage_base,
                                     Tagged<Map> map,
                                     InternalIndex descriptor_index) {
  PropertyDetails details = map->instance_descriptors(cage_base, kAcquireLoad)
                                ->GetDetails(descriptor_index);
  return ForDetails(map, details);
}

FieldIndex FieldIndex::ForDetails(Tagged<Map> map, PropertyDetails details) {
  bool is_inobject = details.is_in_object();
  int offset = details.field_offset() * kTaggedSize;
  Encoding encoding = FieldEncoding(details.representation());
  int inobject_property_count = map->GetInObjectProperties();
  int first_field_offset_in_storage;
  if (is_inobject) {
    first_field_offset_in_storage = map->GetInObjectPropertyOffset(0);
  } else {
    first_field_offset_in_storage = PropertyArray::kHeaderSize;
  }
  return FieldIndex(is_inobject, offset, encoding, inobject_property_count,
                    first_field_offset_in_storage);
}

}  // namespace internal
}  // namespace v8

#endif  // V8_OBJECTS_FIELD_INDEX_INL_H_
