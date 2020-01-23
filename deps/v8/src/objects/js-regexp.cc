// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/objects/js-regexp.h"

#include "src/objects/js-array-inl.h"
#include "src/objects/js-regexp-inl.h"

namespace v8 {
namespace internal {
Handle<JSArray> JSRegExpResult::GetAndCacheIndices(
    Isolate* isolate, Handle<JSRegExpResult> regexp_result) {
  // Check for cached indices.
  Handle<Object> indices_or_match_info(
      regexp_result->cached_indices_or_match_info(), isolate);
  if (indices_or_match_info->IsRegExpMatchInfo()) {
    // Build and cache indices for next lookup.
    // TODO(joshualitt): Instead of caching the indices, we could call
    // ReconfigureToDataProperty on 'indices' setting its value to this
    // newly created array. However, care would have to be taken to ensure
    // a new map is not created each time.
    Handle<RegExpMatchInfo> match_info(
        RegExpMatchInfo::cast(regexp_result->cached_indices_or_match_info()),
        isolate);
    Handle<Object> maybe_names(regexp_result->names(), isolate);
    indices_or_match_info =
        JSRegExpResultIndices::BuildIndices(isolate, match_info, maybe_names);

    // Cache the result and clear the names array.
    regexp_result->set_cached_indices_or_match_info(*indices_or_match_info);
    regexp_result->set_names(ReadOnlyRoots(isolate).undefined_value());
  }
  return Handle<JSArray>::cast(indices_or_match_info);
}

Handle<JSRegExpResultIndices> JSRegExpResultIndices::BuildIndices(
    Isolate* isolate, Handle<RegExpMatchInfo> match_info,
    Handle<Object> maybe_names) {
  Handle<JSRegExpResultIndices> indices(Handle<JSRegExpResultIndices>::cast(
      isolate->factory()->NewJSObjectFromMap(
          isolate->regexp_result_indices_map())));

  // Initialize indices length to avoid having a partially initialized object
  // should GC be triggered by creating a NewFixedArray.
  indices->set_length(Smi::kZero);

  // Build indices array from RegExpMatchInfo.
  int num_indices = match_info->NumberOfCaptureRegisters();
  int num_results = num_indices >> 1;
  Handle<FixedArray> indices_array =
      isolate->factory()->NewFixedArray(num_results);
  JSArray::SetContent(indices, indices_array);

  for (int i = 0; i < num_results; i++) {
    int base_offset = i * 2;
    int start_offset = match_info->Capture(base_offset);
    int end_offset = match_info->Capture(base_offset + 1);

    // Any unmatched captures are set to undefined, otherwise we set them to a
    // subarray of the indices.
    if (start_offset == -1) {
      indices_array->set(i, ReadOnlyRoots(isolate).undefined_value());
    } else {
      Handle<FixedArray> indices_sub_array(
          isolate->factory()->NewFixedArray(2));
      indices_sub_array->set(0, Smi::FromInt(start_offset));
      indices_sub_array->set(1, Smi::FromInt(end_offset));
      Handle<JSArray> indices_sub_jsarray =
          isolate->factory()->NewJSArrayWithElements(indices_sub_array,
                                                     PACKED_SMI_ELEMENTS, 2);
      indices_array->set(i, *indices_sub_jsarray);
    }
  }

  // If there are no capture groups, set the groups property to undefined.
  FieldIndex groups_index = FieldIndex::ForDescriptor(
      indices->map(), InternalIndex(kGroupsDescriptorIndex));
  if (maybe_names->IsUndefined(isolate)) {
    indices->RawFastPropertyAtPut(groups_index,
                                  ReadOnlyRoots(isolate).undefined_value());
    return indices;
  }

  // Create a groups property which returns a dictionary of named captures to
  // their corresponding capture indices.
  Handle<FixedArray> names(Handle<FixedArray>::cast(maybe_names));
  int num_names = names->length() >> 1;
  Handle<NameDictionary> group_names = NameDictionary::New(isolate, num_names);
  for (int i = 0; i < num_names; i++) {
    int base_offset = i * 2;
    int name_offset = base_offset;
    int index_offset = base_offset + 1;
    Handle<String> name(String::cast(names->get(name_offset)), isolate);
    Handle<Smi> smi_index(Smi::cast(names->get(index_offset)), isolate);
    Handle<Object> capture_indices(indices_array->get(smi_index->value()),
                                   isolate);
    if (!capture_indices->IsUndefined(isolate)) {
      capture_indices = Handle<JSArray>::cast(capture_indices);
    }
    group_names = NameDictionary::Add(
        isolate, group_names, name, capture_indices, PropertyDetails::Empty());
  }

  // Convert group_names to a JSObject and store at the groups property of the
  // result indices.
  Handle<FixedArrayBase> elements = isolate->factory()->empty_fixed_array();
  Handle<HeapObject> null =
      Handle<HeapObject>::cast(isolate->factory()->null_value());
  Handle<JSObject> js_group_names =
      isolate->factory()->NewSlowJSObjectWithPropertiesAndElements(
          null, group_names, elements);
  indices->RawFastPropertyAtPut(groups_index, *js_group_names);
  return indices;
}

}  // namespace internal
}  // namespace v8
