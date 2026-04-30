// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_UNITTESTS_PROFILER_HEAP_SNAPSHOT_UTILS_H_
#define V8_UNITTESTS_PROFILER_HEAP_SNAPSHOT_UTILS_H_

#include <optional>

#include "src/objects/objects.h"
#include "src/profiler/heap-snapshot-generator.h"

namespace v8::internal {

class Isolate;

const HeapGraphEdge* GetNamedEdge(const HeapEntry& entry, const char* name);
bool HasNamedEdge(const HeapEntry& entry, const char* name);
std::optional<int> GetIntEdge(const HeapEntry* node, const char* name);
std::optional<bool> GetBoolEdge(const HeapEntry* node, const char* name);

const HeapEntry* GetEntryFor(Isolate* isolate, HeapSnapshot* snapshot,
                             Tagged<HeapObject> object);
const HeapEntry* GetEntryFor(Isolate* isolate, HeapSnapshot* snapshot,
                             Address addr);

}  // namespace v8::internal

#endif  // V8_UNITTESTS_PROFILER_HEAP_SNAPSHOT_UTILS_H_
