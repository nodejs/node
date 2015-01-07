// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_LAYOUT_DESCRIPTOR_INL_H_
#define V8_LAYOUT_DESCRIPTOR_INL_H_

#include "src/layout-descriptor.h"

namespace v8 {
namespace internal {

LayoutDescriptor* LayoutDescriptor::FromSmi(Smi* smi) {
  return LayoutDescriptor::cast(smi);
}


Handle<LayoutDescriptor> LayoutDescriptor::New(Isolate* isolate, int length) {
  if (length <= kSmiValueSize) {
    // The whole bit vector fits into a smi.
    return handle(LayoutDescriptor::FromSmi(Smi::FromInt(0)), isolate);
  }

  length = (length + kNumberOfBits - 1) / kNumberOfBits;
  DCHECK(length > 0);

  if (SmiValuesAre32Bits() && (length & 1)) {
    // On 64-bit systems if the length is odd then the half-word space would be
    // lost anyway (due to alignment and the fact that we are allocating
    // uint32-typed array), so we increase the length of allocated array
    // to utilize that "lost" space which could also help to avoid layout
    // descriptor reallocations.
    ++length;
  }
  return Handle<LayoutDescriptor>::cast(
      isolate->factory()->NewFixedTypedArray(length, kExternalUint32Array));
}


bool LayoutDescriptor::InobjectUnboxedField(int inobject_properties,
                                            PropertyDetails details) {
  if (details.type() != FIELD || !details.representation().IsDouble()) {
    return false;
  }
  // We care only about in-object properties.
  return details.field_index() < inobject_properties;
}


LayoutDescriptor* LayoutDescriptor::FastPointerLayout() {
  return LayoutDescriptor::FromSmi(Smi::FromInt(0));
}


bool LayoutDescriptor::GetIndexes(int field_index, int* layout_word_index,
                                  int* layout_bit_index) {
  if (static_cast<unsigned>(field_index) >= static_cast<unsigned>(capacity())) {
    return false;
  }

  *layout_word_index = field_index / kNumberOfBits;
  CHECK((!IsSmi() && (*layout_word_index < length())) ||
        (IsSmi() && (*layout_word_index < 1)));

  *layout_bit_index = field_index % kNumberOfBits;
  return true;
}


LayoutDescriptor* LayoutDescriptor::SetTagged(int field_index, bool tagged) {
  int layout_word_index;
  int layout_bit_index;

  if (!GetIndexes(field_index, &layout_word_index, &layout_bit_index)) {
    CHECK(false);
    return this;
  }
  uint32_t layout_mask = static_cast<uint32_t>(1) << layout_bit_index;

  if (IsSlowLayout()) {
    uint32_t value = get_scalar(layout_word_index);
    if (tagged) {
      value &= ~layout_mask;
    } else {
      value |= layout_mask;
    }
    set(layout_word_index, value);
    return this;
  } else {
    uint32_t value = static_cast<uint32_t>(Smi::cast(this)->value());
    if (tagged) {
      value &= ~layout_mask;
    } else {
      value |= layout_mask;
    }
    return LayoutDescriptor::FromSmi(Smi::FromInt(static_cast<int>(value)));
  }
}


bool LayoutDescriptor::IsTagged(int field_index) {
  if (IsFastPointerLayout()) return true;

  int layout_word_index;
  int layout_bit_index;

  if (!GetIndexes(field_index, &layout_word_index, &layout_bit_index)) {
    // All bits after Out of bounds queries
    return true;
  }
  uint32_t layout_mask = static_cast<uint32_t>(1) << layout_bit_index;

  if (IsSlowLayout()) {
    uint32_t value = get_scalar(layout_word_index);
    return (value & layout_mask) == 0;
  } else {
    uint32_t value = static_cast<uint32_t>(Smi::cast(this)->value());
    return (value & layout_mask) == 0;
  }
}


bool LayoutDescriptor::IsFastPointerLayout() {
  return this == FastPointerLayout();
}


bool LayoutDescriptor::IsFastPointerLayout(Object* layout_descriptor) {
  return layout_descriptor == FastPointerLayout();
}


bool LayoutDescriptor::IsSlowLayout() { return !IsSmi(); }


int LayoutDescriptor::capacity() {
  return IsSlowLayout() ? (length() * kNumberOfBits) : kSmiValueSize;
}


LayoutDescriptor* LayoutDescriptor::cast_gc_safe(Object* object) {
  if (object->IsSmi()) {
    // Fast mode layout descriptor.
    return reinterpret_cast<LayoutDescriptor*>(object);
  }

  // This is a mixed descriptor which is a fixed typed array.
  MapWord map_word = reinterpret_cast<HeapObject*>(object)->map_word();
  if (map_word.IsForwardingAddress()) {
    // Mark-compact has already moved layout descriptor.
    object = map_word.ToForwardingAddress();
  }
  return LayoutDescriptor::cast(object);
}


// InobjectPropertiesHelper is a helper class for querying whether inobject
// property at offset is Double or not.
LayoutDescriptorHelper::LayoutDescriptorHelper(Map* map)
    : all_fields_tagged_(true),
      header_size_(0),
      layout_descriptor_(LayoutDescriptor::FastPointerLayout()) {
  if (!FLAG_unbox_double_fields) return;

  layout_descriptor_ = map->layout_descriptor_gc_safe();
  if (layout_descriptor_->IsFastPointerLayout()) {
    return;
  }

  int inobject_properties = map->inobject_properties();
  DCHECK(inobject_properties > 0);
  header_size_ = map->instance_size() - (inobject_properties * kPointerSize);
  DCHECK(header_size_ >= 0);

  all_fields_tagged_ = false;
}


bool LayoutDescriptorHelper::IsTagged(int offset_in_bytes) {
  DCHECK(IsAligned(offset_in_bytes, kPointerSize));
  if (all_fields_tagged_) return true;
  // Object headers do not contain non-tagged fields.
  if (offset_in_bytes < header_size_) return true;
  int field_index = (offset_in_bytes - header_size_) / kPointerSize;

  return layout_descriptor_->IsTagged(field_index);
}
}
}  // namespace v8::internal

#endif  // V8_LAYOUT_DESCRIPTOR_INL_H_
