// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_CPPGC_JS_WRAPPABLE_INFO_INL_H_
#define V8_HEAP_CPPGC_JS_WRAPPABLE_INFO_INL_H_

#include "src/base/optional.h"
#include "src/heap/cppgc-js/wrappable-info.h"
#include "src/objects/embedder-data-slot.h"
#include "src/objects/js-objects-inl.h"

namespace v8::internal {

// static
base::Optional<WrappableInfo> WrappableInfo::From(
    Isolate* isolate, JSObject wrapper,
    const WrapperDescriptor& wrapper_descriptor) {
  DCHECK(wrapper.MayHaveEmbedderFields());
  return wrapper.GetEmbedderFieldCount() < 2
             ? base::Optional<WrappableInfo>()
             : From(isolate,
                    EmbedderDataSlot(wrapper,
                                     wrapper_descriptor.wrappable_type_index),
                    EmbedderDataSlot(
                        wrapper, wrapper_descriptor.wrappable_instance_index),
                    wrapper_descriptor);
}

// static
base::Optional<WrappableInfo> WrappableInfo::From(
    Isolate* isolate, const EmbedderDataSlot& type_slot,
    const EmbedderDataSlot& instance_slot,
    const WrapperDescriptor& wrapper_descriptor) {
  void* type;
  void* instance;
  if (type_slot.ToAlignedPointer(isolate, &type) && type &&
      instance_slot.ToAlignedPointer(isolate, &instance) && instance &&
      (wrapper_descriptor.embedder_id_for_garbage_collected ==
           WrapperDescriptor::kUnknownEmbedderId ||
       (*static_cast<uint16_t*>(type) ==
        wrapper_descriptor.embedder_id_for_garbage_collected))) {
    return base::Optional<WrappableInfo>(base::in_place, type, instance);
  }
  return {};
}

}  // namespace v8::internal

#endif  // V8_HEAP_CPPGC_JS_WRAPPABLE_INFO_INL_H_
