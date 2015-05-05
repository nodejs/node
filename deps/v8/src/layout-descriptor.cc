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

  int layout_descriptor_length =
      CalculateCapacity(*map, *descriptors, num_descriptors);

  if (layout_descriptor_length == 0) {
    // No double fields were found, use fast pointer layout.
    return handle(FastPointerLayout(), isolate);
  }

  // Initially, layout descriptor corresponds to an object with all fields
  // tagged.
  Handle<LayoutDescriptor> layout_descriptor_handle =
      LayoutDescriptor::New(isolate, layout_descriptor_length);

  LayoutDescriptor* layout_descriptor = Initialize(
      *layout_descriptor_handle, *map, *descriptors, num_descriptors);

  return handle(layout_descriptor, isolate);
}


Handle<LayoutDescriptor> LayoutDescriptor::ShareAppend(
    Handle<Map> map, PropertyDetails details) {
  DCHECK(map->owns_descriptors());
  Isolate* isolate = map->GetIsolate();
  Handle<LayoutDescriptor> layout_descriptor(map->GetLayoutDescriptor(),
                                             isolate);

  if (!InobjectUnboxedField(map->inobject_properties(), details)) {
    DCHECK(details.location() != kField ||
           layout_descriptor->IsTagged(details.field_index()));
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
    DCHECK(details.location() != kField ||
           layout_descriptor->IsTagged(details.field_index()));
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


LayoutDescriptor* LayoutDescriptor::Trim(Heap* heap, Map* map,
                                         DescriptorArray* descriptors,
                                         int num_descriptors) {
  DisallowHeapAllocation no_allocation;
  // Fast mode descriptors are never shared and therefore always fully
  // correspond to their map.
  if (!IsSlowLayout()) return this;

  int layout_descriptor_length =
      CalculateCapacity(map, descriptors, num_descriptors);
  // It must not become fast-mode descriptor here, because otherwise it has to
  // be fast pointer layout descriptor already but it's is slow mode now.
  DCHECK_LT(kSmiValueSize, layout_descriptor_length);

  // Trim, clean and reinitialize this slow-mode layout descriptor.
  int array_length = GetSlowModeBackingStoreLength(layout_descriptor_length);
  int current_length = length();
  if (current_length != array_length) {
    DCHECK_LT(array_length, current_length);
    int delta = current_length - array_length;
    heap->RightTrimFixedArray<Heap::FROM_GC>(this, delta);
  }
  memset(DataPtr(), 0, DataSize());
  LayoutDescriptor* layout_descriptor =
      Initialize(this, map, descriptors, num_descriptors);
  DCHECK_EQ(this, layout_descriptor);
  return layout_descriptor;
}


bool LayoutDescriptor::IsConsistentWithMap(Map* map, bool check_tail) {
  if (FLAG_unbox_double_fields) {
    DescriptorArray* descriptors = map->instance_descriptors();
    int nof_descriptors = map->NumberOfOwnDescriptors();
    int last_field_index = 0;
    for (int i = 0; i < nof_descriptors; i++) {
      PropertyDetails details = descriptors->GetDetails(i);
      if (details.location() != kField) continue;
      FieldIndex field_index = FieldIndex::ForDescriptor(map, i);
      bool tagged_expected =
          !field_index.is_inobject() || !details.representation().IsDouble();
      for (int bit = 0; bit < details.field_width_in_words(); bit++) {
        bool tagged_actual = IsTagged(details.field_index() + bit);
        DCHECK_EQ(tagged_expected, tagged_actual);
        if (tagged_actual != tagged_expected) return false;
      }
      last_field_index =
          Max(last_field_index,
              details.field_index() + details.field_width_in_words());
    }
    if (check_tail) {
      int n = capacity();
      for (int i = last_field_index; i < n; i++) {
        DCHECK(IsTagged(i));
      }
    }
  }
  return true;
}
}
}  // namespace v8::internal
