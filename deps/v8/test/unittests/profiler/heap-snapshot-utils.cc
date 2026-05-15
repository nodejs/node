// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "test/unittests/profiler/heap-snapshot-utils.h"

#include <cstring>
#include <string>

namespace v8::internal {

const HeapGraphEdge* GetNamedEdge(const HeapEntry& entry, const char* name) {
  for (int i = 0; i < entry.children_count(); ++i) {
    const HeapGraphEdge* edge = entry.child(i);
    switch (edge->type()) {
      case HeapGraphEdge::kContextVariable:
      case HeapGraphEdge::kProperty:
      case HeapGraphEdge::kInternal:
      case HeapGraphEdge::kShortcut:
      case HeapGraphEdge::kWeak:
        if (edge->name() != nullptr && strcmp(edge->name(), name) == 0) {
          return edge;
        }
        break;
      case HeapGraphEdge::kElement:
      case HeapGraphEdge::kHidden:
        break;
    }
  }
  return nullptr;
}

bool HasNamedEdge(const HeapEntry& entry, const char* name) {
  return GetNamedEdge(entry, name) != nullptr;
}

std::optional<int> GetIntEdge(const HeapEntry* node, const char* name) {
  const HeapGraphEdge* edge = GetNamedEdge(*node, name);
  if (!edge || edge->to()->type() != HeapEntry::kHeapNumber ||
      strcmp("int", edge->to()->name()) != 0) {
    return std::nullopt;
  }
  const HeapGraphEdge* value_edge = GetNamedEdge(*edge->to(), "value");
  if (!value_edge || value_edge->to()->type() != HeapEntry::kString) {
    return std::nullopt;
  }
  return std::stoi(value_edge->to()->name());
}

}  // namespace v8::internal
