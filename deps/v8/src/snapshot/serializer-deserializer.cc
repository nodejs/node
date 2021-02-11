// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/snapshot/serializer-deserializer.h"

#include "src/objects/foreign-inl.h"
#include "src/objects/objects-inl.h"

namespace v8 {
namespace internal {

// The startup object cache is terminated by undefined. We visit the context
// snapshot...
//  - during deserialization to populate it.
//  - during normal GC to keep its content alive.
//  - not during serialization. The context serializer adds to it explicitly.
DISABLE_CFI_PERF
void SerializerDeserializer::Iterate(Isolate* isolate, RootVisitor* visitor) {
  std::vector<Object>* cache = isolate->startup_object_cache();
  for (size_t i = 0;; ++i) {
    // Extend the array ready to get a value when deserializing.
    if (cache->size() <= i) cache->push_back(Smi::zero());
    // During deserialization, the visitor populates the startup object cache
    // and eventually terminates the cache with undefined.
    visitor->VisitRootPointer(Root::kStartupObjectCache, nullptr,
                              FullObjectSlot(&cache->at(i)));
    if (cache->at(i).IsUndefined(isolate)) break;
  }
}

bool SerializerDeserializer::CanBeDeferred(HeapObject o) {
  // Maps cannot be deferred as objects are expected to have a valid map
  // immediately. Internalized strings cannot be deferred as they might be
  // converted to thin strings during post processing, at which point forward
  // references to the now-thin string will already have been written.
  // TODO(leszeks): Could we defer string serialization if forward references
  // were resolved after object post processing?
  return !o.IsMap() && !o.IsInternalizedString();
}

void SerializerDeserializer::RestoreExternalReferenceRedirector(
    Isolate* isolate, Handle<AccessorInfo> accessor_info) {
  // Restore wiped accessor infos.
  Foreign::cast(accessor_info->js_getter())
      .set_foreign_address(isolate, accessor_info->redirected_getter());
}

void SerializerDeserializer::RestoreExternalReferenceRedirector(
    Isolate* isolate, Handle<CallHandlerInfo> call_handler_info) {
  Foreign::cast(call_handler_info->js_callback())
      .set_foreign_address(isolate, call_handler_info->redirected_callback());
}

}  // namespace internal
}  // namespace v8
