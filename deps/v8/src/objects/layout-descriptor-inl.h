// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_LAYOUT_DESCRIPTOR_INL_H_
#define V8_OBJECTS_LAYOUT_DESCRIPTOR_INL_H_

#include "src/objects/layout-descriptor.h"

#include "src/handles/handles-inl.h"
#include "src/objects/descriptor-array-inl.h"
#include "src/objects/objects-inl.h"
#include "src/objects/smi.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

LayoutDescriptor::LayoutDescriptor(Address ptr)
    : ByteArray(ptr, AllowInlineSmiStorage::kAllowBeingASmi) {
  SLOW_DCHECK(IsLayoutDescriptor());
}
CAST_ACCESSOR(LayoutDescriptor)

LayoutDescriptor LayoutDescriptor::FromSmi(Smi smi) {
  return LayoutDescriptor::cast(smi);
}

Handle<LayoutDescriptor> LayoutDescriptor::New(Isolate* isolate, int length) {
  if (length <= kBitsInSmiLayout) {
    // The whole bit vector fits into a smi.
    return handle(LayoutDescriptor::FromSmi(Smi::zero()), isolate);
  }
  int backing_store_length = GetSlowModeBackingStoreLength(length);
  Handle<LayoutDescriptor> result =
      Handle<LayoutDescriptor>::cast(isolate->factory()->NewByteArray(
          backing_store_length, AllocationType::kOld));
  memset(reinterpret_cast<void*>(result->GetDataStartAddress()), 0,
         result->DataSize());
  return result;
}

bool LayoutDescriptor::InobjectUnboxedField(int inobject_properties,
                                            PropertyDetails details) {
  if (details.location() != kField || !details.representation().IsDouble()) {
    return false;
  }
  // We care only about in-object properties.
  return details.field_index() < inobject_properties;
}

LayoutDescriptor LayoutDescriptor::FastPointerLayout() {
  return LayoutDescriptor::FromSmi(Smi::zero());
}

bool LayoutDescriptor::GetIndexes(int field_index, int* layout_word_index,
                                  int* layout_bit_index) {
  if (static_cast<unsigned>(field_index) >= static_cast<unsigned>(capacity())) {
    return false;
  }

  *layout_word_index = field_index / kBitsPerLayoutWord;
  CHECK((!IsSmi() && (*layout_word_index < length())) ||
        (IsSmi() && (*layout_word_index < 1)));

  *layout_bit_index = field_index % kBitsPerLayoutWord;
  return true;
}

LayoutDescriptor LayoutDescriptor::SetRawData(int field_index) {
  return SetTagged(field_index, false);
}

LayoutDescriptor LayoutDescriptor::SetTagged(int field_index, bool tagged) {
  int layout_word_index = 0;
  int layout_bit_index = 0;

  CHECK(GetIndexes(field_index, &layout_word_index, &layout_bit_index));
  uint32_t layout_mask = static_cast<uint32_t>(1) << layout_bit_index;

  if (IsSlowLayout()) {
    uint32_t value = get_layout_word(layout_word_index);
    if (tagged) {
      value &= ~layout_mask;
    } else {
      value |= layout_mask;
    }
    set_layout_word(layout_word_index, value);
    return *this;
  } else {
    uint32_t value = static_cast<uint32_t>(Smi::ToInt(*this));
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
    uint32_t value = get_layout_word(layout_word_index);
    return (value & layout_mask) == 0;
  } else {
    uint32_t value = static_cast<uint32_t>(Smi::ToInt(*this));
    return (value & layout_mask) == 0;
  }
}

bool LayoutDescriptor::IsFastPointerLayout() {
  return *this == FastPointerLayout();
}

bool LayoutDescriptor::IsFastPointerLayout(Object layout_descriptor) {
  return layout_descriptor == FastPointerLayout();
}

bool LayoutDescriptor::IsSlowLayout() { return !IsSmi(); }

int LayoutDescriptor::capacity() {
  return IsSlowLayout() ? (length() * kBitsPerByte) : kBitsInSmiLayout;
}

LayoutDescriptor LayoutDescriptor::cast_gc_safe(Object object) {
  // The map word of the object can be a forwarding pointer during
  // object evacuation phase of GC. Since the layout descriptor methods
  // for checking whether a field is tagged or not do not depend on the
  // object map, it should be safe.
  return LayoutDescriptor::unchecked_cast(object);
}

int LayoutDescriptor::GetSlowModeBackingStoreLength(int length) {
  DCHECK_LT(0, length);
  // We allocate kTaggedSize rounded blocks of memory anyway so we increase
  // the length  of allocated array to utilize that "lost" space which could
  // also help to avoid layout descriptor reallocations.
  return RoundUp(length, kBitsPerByte * kTaggedSize) / kBitsPerByte;
}

int LayoutDescriptor::CalculateCapacity(Map map, DescriptorArray descriptors,
                                        int num_descriptors) {
  int inobject_properties = map.GetInObjectProperties();
  if (inobject_properties == 0) return 0;

  DCHECK_LE(num_descriptors, descriptors.number_of_descriptors());

  int layout_descriptor_length;
  const int kMaxWordsPerField = kDoubleSize / kTaggedSize;

  if (num_descriptors <= kBitsInSmiLayout / kMaxWordsPerField) {
    // Even in the "worst" case (all fields are doubles) it would fit into
    // a Smi, so no need to calculate length.
    layout_descriptor_length = kBitsInSmiLayout;

  } else {
    layout_descriptor_length = 0;

    for (int i = 0; i < num_descriptors; i++) {
      PropertyDetails details = descriptors.GetDetails(i);
      if (!InobjectUnboxedField(inobject_properties, details)) continue;
      int field_index = details.field_index();
      int field_width_in_words = details.field_width_in_words();
      layout_descriptor_length =
          Max(layout_descriptor_length, field_index + field_width_in_words);
    }
  }
  layout_descriptor_length = Min(layout_descriptor_length, inobject_properties);
  return layout_descriptor_length;
}

LayoutDescriptor LayoutDescriptor::Initialize(
    LayoutDescriptor layout_descriptor, Map map, DescriptorArray descriptors,
    int num_descriptors) {
  DisallowHeapAllocation no_allocation;
  int inobject_properties = map.GetInObjectProperties();

  for (int i = 0; i < num_descriptors; i++) {
    PropertyDetails details = descriptors.GetDetails(i);
    if (!InobjectUnboxedField(inobject_properties, details)) {
      DCHECK(details.location() != kField ||
             layout_descriptor.IsTagged(details.field_index()));
      continue;
    }
    int field_index = details.field_index();
    layout_descriptor = layout_descriptor.SetRawData(field_index);
    if (details.field_width_in_words() > 1) {
      layout_descriptor = layout_descriptor.SetRawData(field_index + 1);
    }
  }
  return layout_descriptor;
}

int LayoutDescriptor::number_of_layout_words() {
  return length() / kUInt32Size;
}

uint32_t LayoutDescriptor::get_layout_word(int index) const {
  return get_uint32(index);
}

void LayoutDescriptor::set_layout_word(int index, uint32_t value) {
  set_uint32(index, value);
}

// LayoutDescriptorHelper is a helper class for querying whether inobject
// property at offset is Double or not.
LayoutDescriptorHelper::LayoutDescriptorHelper(Map map)
    : all_fields_tagged_(true),
      header_size_(0),
      layout_descriptor_(LayoutDescriptor::FastPointerLayout()) {
  if (!FLAG_unbox_double_fields) return;

  layout_descriptor_ = map.layout_descriptor_gc_safe();
  if (layout_descriptor_.IsFastPointerLayout()) {
    return;
  }

  header_size_ = map.GetInObjectPropertiesStartInWords() * kTaggedSize;
  DCHECK_GE(header_size_, 0);

  all_fields_tagged_ = false;
}

bool LayoutDescriptorHelper::IsTagged(int offset_in_bytes) {
  DCHECK(IsAligned(offset_in_bytes, kTaggedSize));
  if (all_fields_tagged_) return true;
  // Object headers do not contain non-tagged fields.
  if (offset_in_bytes < header_size_) return true;
  int field_index = (offset_in_bytes - header_size_) / kTaggedSize;

  return layout_descriptor_.IsTagged(field_index);
}

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_LAYOUT_DESCRIPTOR_INL_H_
