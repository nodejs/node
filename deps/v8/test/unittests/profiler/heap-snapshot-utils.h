// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_UNITTESTS_PROFILER_HEAP_SNAPSHOT_UTILS_H_
#define V8_UNITTESTS_PROFILER_HEAP_SNAPSHOT_UTILS_H_

#include <optional>
#include <type_traits>

#include "include/cppgc/type-traits.h"
#include "src/execution/isolate.h"
#include "src/heap/cppgc/heap-object-header.h"
#include "src/objects/objects.h"
#include "src/profiler/heap-profiler.h"
#include "src/profiler/heap-snapshot-generator.h"

namespace v8::internal {

class Isolate;

const HeapGraphEdge* GetNamedEdge(const HeapEntry& entry, const char* name);
const HeapGraphEdge* FindFirstEdgeTo(const HeapEntry& from,
                                     const HeapEntry& to);
const HeapEntry* GetEntryByName(HeapSnapshot* snapshot, const char* name);
bool HasNamedEdge(const HeapEntry& entry, const char* name);
std::optional<int> GetIntEdge(const HeapEntry* node, const char* name);
std::optional<bool> GetBoolEdge(const HeapEntry* node, const char* name);

const HeapEntry* GetEntryFor(Isolate* isolate, HeapSnapshot* snapshot,
                             Tagged<HeapObject> object);
const HeapEntry* GetEntryFor(Isolate* isolate, HeapSnapshot* snapshot,
                             Address addr);
const HeapEntry* GetEntryFor(Isolate* isolate, HeapSnapshot* snapshot,
                             const cppgc::internal::HeapObjectHeader& header);

template <typename T>
  requires cppgc::IsGarbageCollectedTypeV<std::remove_const_t<T>>
const HeapEntry* GetEntryFor(Isolate* isolate, HeapSnapshot* snapshot,
                             const T* object) {
  return GetEntryFor(isolate, snapshot,
                     cppgc::internal::HeapObjectHeader::FromObject(object));
}

}  // namespace v8::internal

#endif  // V8_UNITTESTS_PROFILER_HEAP_SNAPSHOT_UTILS_H_
