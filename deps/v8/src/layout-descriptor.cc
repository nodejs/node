// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sstream>

#include "src/v8.h"

#include "src/base/bits.h"
#include "src/layout-descriptor.h"

using v8::base::bits::CountTrailingZeros32;

namespace v8 {
namespace internal {

Handle<LayoutDescriptor> LayoutDescriptor::New(
    Handle<Map> map, Handle<DescriptorArray> descriptors, int num_descriptors) {
  Isolate* isolate = descriptors->GetIsolate();
  if (!FLAG_unbox_double_fields) return handle(FastPointerLayout(), isolate);

  int inobject_properties = map->inobject_properties();
  if (inobject_properties == 0) return handle(FastPointerLayout(), isolate);

  DCHECK(num_descriptors <= descriptors->number_of_descriptors());

  int layout_descriptor_length;
  const int kMaxWordsPerField = kDoubleSize / kPointerSize;

  if (num_descriptors <= kSmiValueSize / kMaxWordsPerField) {
    // Even in the "worst" case (all fields are doubles) it would fit into
    // a Smi, so no need to calculate length.
    layout_descriptor_length = kSmiValueSize;

  } else {
    layout_descriptor_length = 0;

    for (int i = 0; i < num_descriptors; i++) {
      PropertyDetails details = descriptors->GetDetails(i);
      if (!InobjectUnboxedField(inobject_properties, details)) continue;
      int field_index = details.field_index();
      int field_width_in_words = details.field_width_in_words();
      layout_descriptor_length =
          Max(layout_descriptor_length, field_index + field_width_in_words);
    }

    if (layout_descriptor_length == 0) {
      // No double fields were found, use fast pointer layout.
      return handle(FastPointerLayout(), isolate);
    }
  }
  layout_descriptor_length = Min(layout_descriptor_length, inobject_properties);

  // Initially, layout descriptor corresponds to an object with all fields
  // tagged.
  Handle<LayoutDescriptor> layout_descriptor_handle =
      LayoutDescriptor::New(isolate, layout_descriptor_length);

  DisallowHeapAllocation no_allocation;
  LayoutDescriptor* layout_descriptor = *layout_descriptor_handle;

  for (int i = 0; i < num_descriptors; i++) {
    PropertyDetails details = descriptors->GetDetails(i);
    if (!InobjectUnboxedField(inobject_properties, details)) continue;
    int field_index = details.field_index();
    layout_descriptor = layout_descriptor->SetRawData(field_index);
    if (details.field_width_in_words() > 1) {
      layout_descriptor = layout_descriptor->SetRawData(field_index + 1);
    }
  }
  return handle(layout_descriptor, isolate);
}


Handle<LayoutDescriptor> LayoutDescriptor::Append(Handle<Map> map,
                                                  PropertyDetails details) {
  Isolate* isolate = map->GetIsolate();
  Handle<LayoutDescriptor> layout_descriptor(map->GetLayoutDescriptor(),
                                             isolate);

  if (!InobjectUnboxedField(map->inobject_properties(), details)) {
    return layout_descriptor;
  }
  int field_index = details.field_index();
  layout_descriptor = LayoutDescriptor::EnsureCapacity(
      isolate, layout_descriptor, field_index + details.field_width_in_words());

  DisallowHeapAllocation no_allocation;
  LayoutDescriptor* layout_desc = *layout_descriptor;
  layout_desc = layout_desc->SetRawData(field_index);
  if (details.field_width_in_words() > 1) {
    layout_desc = layout_desc->SetRawData(field_index + 1);
  }
  return handle(layout_desc, isolate);
}


Handle<LayoutDescriptor> LayoutDescriptor::AppendIfFastOrUseFull(
    Handle<Map> map, PropertyDetails details,
    Handle<LayoutDescriptor> full_layout_descriptor) {
  DisallowHeapAllocation no_allocation;
  LayoutDescriptor* layout_descriptor = map->layout_descriptor();
  if (layout_descriptor->IsSlowLayout()) {
    return full_layout_descriptor;
  }
  if (!InobjectUnboxedField(map->inobject_properties(), details)) {
    return handle(layout_descriptor, map->GetIsolate());
  }
  int field_index = details.field_index();
  int new_capacity = field_index + details.field_width_in_words();
  if (new_capacity > layout_descriptor->capacity()) {
    // Current map's layout descriptor runs out of space, so use the full
    // layout descriptor.
    return full_layout_descriptor;
  }

  layout_descriptor = layout_descriptor->SetRawData(field_index);
  if (details.field_width_in_words() > 1) {
    layout_descriptor = layout_descriptor->SetRawData(field_index + 1);
  }
  return handle(layout_descriptor, map->GetIsolate());
}


Handle<LayoutDescriptor> LayoutDescriptor::EnsureCapacity(
    Isolate* isolate, Handle<LayoutDescriptor> layout_descriptor,
    int new_capacity) {
  int old_capacity = layout_descriptor->capacity();
  if (new_capacity <= old_capacity) {
    // Nothing to do with layout in Smi-form.
    return layout_descriptor;
  }
  Handle<LayoutDescriptor> new_layout_descriptor =
      LayoutDescriptor::New(isolate, new_capacity);
  DCHECK(new_layout_descriptor->IsSlowLayout());

  if (layout_descriptor->IsSlowLayout()) {
    memcpy(new_layout_descriptor->DataPtr(), layout_descriptor->DataPtr(),
           layout_descriptor->DataSize());
    return new_layout_descriptor;
  } else {
    // Fast layout.
    uint32_t value =
        static_cast<uint32_t>(Smi::cast(*layout_descriptor)->value());
    new_layout_descriptor->set(0, value);
    return new_layout_descriptor;
  }
}


bool LayoutDescriptor::IsTagged(int field_index, int max_sequence_length,
                                int* out_sequence_length) {
  DCHECK(max_sequence_length > 0);
  if (IsFastPointerLayout()) {
    *out_sequence_length = max_sequence_length;
    return true;
  }

  int layout_word_index;
  int layout_bit_index;

  if (!GetIndexes(field_index, &layout_word_index, &layout_bit_index)) {
    // Out of bounds queries are considered tagged.
    *out_sequence_length = max_sequence_length;
    return true;
  }
  uint32_t layout_mask = static_cast<uint32_t>(1) << layout_bit_index;

  uint32_t value = IsSlowLayout()
                       ? get_scalar(layout_word_index)
                       : static_cast<uint32_t>(Smi::cast(this)->value());

  bool is_tagged = (value & layout_mask) == 0;
  if (!is_tagged) value = ~value;  // Count set bits instead of cleared bits.
  value = value & ~(layout_mask - 1);  // Clear bits we are not interested in.
  int sequence_length = CountTrailingZeros32(value) - layout_bit_index;

  if (layout_bit_index + sequence_length == kNumberOfBits) {
    // This is a contiguous sequence till the end of current word, proceed
    // counting in the subsequent words.
    if (IsSlowLayout()) {
      int len = length();
      ++layout_word_index;
      for (; layout_word_index < len; layout_word_index++) {
        value = get_scalar(layout_word_index);
        bool cur_is_tagged = (value & 1) == 0;
        if (cur_is_tagged != is_tagged) break;
        if (!is_tagged) value = ~value;  // Count set bits instead.
        int cur_sequence_length = CountTrailingZeros32(value);
        sequence_length += cur_sequence_length;
        if (sequence_length >= max_sequence_length) break;
        if (cur_sequence_length != kNumberOfBits) break;
      }
    }
    if (is_tagged && (field_index + sequence_length == capacity())) {
      // The contiguous sequence of tagged fields lasts till the end of the
      // layout descriptor which means that all the fields starting from
      // field_index are tagged.
      sequence_length = std::numeric_limits<int>::max();
    }
  }
  *out_sequence_length = Min(sequence_length, max_sequence_length);
  return is_tagged;
}


Handle<LayoutDescriptor> LayoutDescriptor::NewForTesting(Isolate* isolate,
                                                         int length) {
  return New(isolate, length);
}


LayoutDescriptor* LayoutDescriptor::SetTaggedForTesting(int field_index,
                                                        bool tagged) {
  return SetTagged(field_index, tagged);
}


bool LayoutDescriptorHelper::IsTagged(
    int offset_in_bytes, int end_offset,
    int* out_end_of_contiguous_region_offset) {
  DCHECK(IsAligned(offset_in_bytes, kPointerSize));
  DCHECK(IsAligned(end_offset, kPointerSize));
  DCHECK(offset_in_bytes < end_offset);
  if (all_fields_tagged_) {
    *out_end_of_contiguous_region_offset = end_offset;
    DCHECK(offset_in_bytes < *out_end_of_contiguous_region_offset);
    return true;
  }
  int max_sequence_length = (end_offset - offset_in_bytes) / kPointerSize;
  int field_index = Max(0, (offset_in_bytes - header_size_) / kPointerSize);
  int sequence_length;
  bool tagged = layout_descriptor_->IsTagged(field_index, max_sequence_length,
                                             &sequence_length);
  DCHECK(sequence_length > 0);
  if (offset_in_bytes < header_size_) {
    // Object headers do not contain non-tagged fields. Check if the contiguous
    // region continues after the header.
    if (tagged) {
      // First field is tagged, calculate end offset from there.
      *out_end_of_contiguous_region_offset =
          header_size_ + sequence_length * kPointerSize;

    } else {
      *out_end_of_contiguous_region_offset = header_size_;
    }
    DCHECK(offset_in_bytes < *out_end_of_contiguous_region_offset);
    return true;
  }
  *out_end_of_contiguous_region_offset =
      offset_in_bytes + sequence_length * kPointerSize;
  DCHECK(offset_in_bytes < *out_end_of_contiguous_region_offset);
  return tagged;
}
}
}  // namespace v8::internal
