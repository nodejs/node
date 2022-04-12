// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef V8_HEAP_EMBEDDER_TRACING_INL_H_
#define V8_HEAP_EMBEDDER_TRACING_INL_H_

#include "src/heap/embedder-tracing.h"
#include "src/objects/embedder-data-slot.h"
#include "src/objects/js-objects-inl.h"

namespace v8 {
namespace internal {

// static
bool LocalEmbedderHeapTracer::ExtractWrappableInfo(
    Isolate* isolate, JSObject js_object,
    const WrapperDescriptor& wrapper_descriptor, WrapperInfo* info) {
  DCHECK(js_object.MayHaveEmbedderFields());
  if (js_object.GetEmbedderFieldCount() < 2) return false;

  return ExtractWrappableInfo(
      isolate, wrapper_descriptor,
      EmbedderDataSlot(js_object, wrapper_descriptor.wrappable_type_index),
      EmbedderDataSlot(js_object, wrapper_descriptor.wrappable_instance_index),
      info);
}

// static
bool LocalEmbedderHeapTracer::ExtractWrappableInfo(
    Isolate* isolate, const WrapperDescriptor& wrapper_descriptor,
    const EmbedderDataSlot& type_slot, const EmbedderDataSlot& instance_slot,
    WrapperInfo* info) {
  if (type_slot.ToAlignedPointer(isolate, &info->first) && info->first &&
      instance_slot.ToAlignedPointer(isolate, &info->second) && info->second) {
    return (wrapper_descriptor.embedder_id_for_garbage_collected ==
            WrapperDescriptor::kUnknownEmbedderId) ||
           (*static_cast<uint16_t*>(info->first) ==
            wrapper_descriptor.embedder_id_for_garbage_collected);
  }
  return false;
}

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_EMBEDDER_TRACING_INL_H_
