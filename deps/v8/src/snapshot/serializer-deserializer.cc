// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/snapshot/serializer-deserializer.h"

#include "src/objects/foreign-inl.h"
#include "src/objects/objects-inl.h"

namespace v8 {
namespace internal {

namespace {
DISABLE_CFI_PERF
void IterateObjectCache(Isolate* isolate, std::vector<Object>* cache,
                        Root root_id, RootVisitor* visitor) {
  for (size_t i = 0;; ++i) {
    // Extend the array ready to get a value when deserializing.
    if (cache->size() <= i) cache->push_back(Smi::zero());
    // During deserialization, the visitor populates the object cache and
    // eventually terminates the cache with undefined.
    visitor->VisitRootPointer(root_id, nullptr, FullObjectSlot(&cache->at(i)));
    if (cache->at(i).IsUndefined(isolate)) break;
  }
}
}  // namespace

// The startup and shared heap object caches are terminated by undefined. We
// visit these caches...
//  - during deserialization to populate it.
//  - during normal GC to keep its content alive.
//  - not during serialization. The context serializer adds to it explicitly.
void SerializerDeserializer::IterateStartupObjectCache(Isolate* isolate,
                                                       RootVisitor* visitor) {
  IterateObjectCache(isolate, isolate->startup_object_cache(),
                     Root::kStartupObjectCache, visitor);
}

void SerializerDeserializer::IterateSharedHeapObjectCache(
    Isolate* isolate, RootVisitor* visitor) {
  IterateObjectCache(isolate, isolate->shared_heap_object_cache(),
                     Root::kSharedHeapObjectCache, visitor);
}

bool SerializerDeserializer::CanBeDeferred(HeapObject o) {
  // 1. Maps cannot be deferred as objects are expected to have a valid map
  // immediately.
  // 2. Internalized strings cannot be deferred as they might be
  // converted to thin strings during post processing, at which point forward
  // references to the now-thin string will already have been written.
  // 3. JS objects with embedder fields cannot be deferred because the
  // serialize/deserialize callbacks need the back reference immediately to
  // identify the object.
  // TODO(leszeks): Could we defer string serialization if forward references
  // were resolved after object post processing?
  return !o.IsMap() && !o.IsInternalizedString() &&
         !(o.IsJSObject() && JSObject::cast(o).GetEmbedderFieldCount() > 0);
}

void SerializerDeserializer::RestoreExternalReferenceRedirector(
    Isolate* isolate, AccessorInfo accessor_info) {
  DisallowGarbageCollection no_gc;
  // Restore wiped accessor infos.
  Foreign::cast(accessor_info.js_getter())
      .set_foreign_address(isolate, accessor_info.redirected_getter());
}

void SerializerDeserializer::RestoreExternalReferenceRedirector(
    Isolate* isolate, CallHandlerInfo call_handler_info) {
  DisallowGarbageCollection no_gc;
  Foreign::cast(call_handler_info.js_callback())
      .set_foreign_address(isolate, call_handler_info.redirected_callback());
}

}  // namespace internal
}  // namespace v8
