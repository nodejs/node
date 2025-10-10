// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/objects/number-string-cache.h"

#include "src/objects/number-string-cache-inl.h"
#include "src/utils/ostreams.h"

namespace v8::internal {

uint32_t SmiStringCache::GetUsedEntriesCount() {
  uint32_t count = 0;
  for (auto entry : InternalIndex::Range(capacity())) {
    uint32_t entry_index = entry.as_uint32() * kEntrySize;
    Tagged<Object> key = get(entry_index + kEntryKeyIndex);
    if (key != kEmptySentinel) {
      ++count;
    }
  }
  return count;
}

void SmiStringCache::Print(const char* comment) {
#ifdef OBJECT_PRINT
  StdoutStream os;
  os << comment;
  for (auto entry : InternalIndex::Range(capacity())) {
    uint32_t entry_index = entry.as_uint32() * kEntrySize;
    Tagged<Object> key = get(entry_index + kEntryKeyIndex);
    if (key != kEmptySentinel) {
      os << "\n  - " << Brief(key);
    }
  }
  os << "\n";
#endif
}

uint32_t DoubleStringCache::GetUsedEntriesCount() {
  uint32_t count = 0;
  for (auto& entry : *this) {
    if (entry.value_.load() != kEmptySentinel) {
      ++count;
    }
  }
  return count;
}

void DoubleStringCache::Print(const char* comment) {
#ifdef OBJECT_PRINT
  StdoutStream os;
  os << comment << "\n";
  DoubleStringCachePrint(os);
#endif
}

}  // namespace v8::internal
