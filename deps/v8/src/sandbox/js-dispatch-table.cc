// Copyright 2024 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/sandbox/js-dispatch-table.h"

#include "src/common/code-memory-access-inl.h"
#include "src/execution/isolate.h"
#include "src/logging/counters.h"
#include "src/objects/code-inl.h"
#include "src/sandbox/js-dispatch-table-inl.h"

#ifdef V8_ENABLE_SANDBOX

namespace v8 {
namespace internal {

void JSDispatchEntry::CheckFieldOffsets() {
  static_assert(JSDispatchEntry::kEntrypointOffset ==
                offsetof(JSDispatchEntry, entrypoint_));
  static_assert(JSDispatchEntry::kCodeObjectOffset ==
                offsetof(JSDispatchEntry, encoded_word_));
}

JSDispatchHandle JSDispatchTable::PreAllocateEntries(
    Space* space, int count, bool ensure_static_handles) {
  DCHECK(space->BelongsTo(this));
  DCHECK_IMPLIES(ensure_static_handles, space->is_internal_read_only_space());
  JSDispatchHandle first;
  for (int i = 0; i < count; ++i) {
    uint32_t idx = AllocateEntry(space);
    if (i == 0) {
      first = IndexToHandle(idx);
    } else {
      // Pre-allocated entries should be consecutive.
      DCHECK_EQ(IndexToHandle(idx), IndexToHandle(HandleToIndex(first) + i));
    }
    if (ensure_static_handles) {
      CHECK_EQ(IndexToHandle(idx), GetStaticHandleForReadOnlySegmentEntry(i));
    }
  }
  return first;
}

bool JSDispatchTable::PreAllocatedEntryNeedsInitialization(
    Space* space, JSDispatchHandle handle) {
  DCHECK(space->BelongsTo(this));
  uint32_t index = HandleToIndex(handle);
  return at(index).IsFreelistEntry();
}

void JSDispatchTable::InitializePreAllocatedEntry(Space* space,
                                                  JSDispatchHandle handle,
                                                  Tagged<Code> code,
                                                  uint16_t parameter_count) {
  DCHECK(space->BelongsTo(this));
  uint32_t index = HandleToIndex(handle);
  DCHECK(space->Contains(index));
  DCHECK(at(index).IsFreelistEntry());
  CFIMetadataWriteScope write_scope(
      "JSDispatchTable initialize pre-allocated entry");
  at(index).MakeJSDispatchEntry(code.address(), code->instruction_start(),
                                parameter_count, space->allocate_black());
}

#ifdef DEBUG
bool JSDispatchTable::IsMarked(JSDispatchHandle handle) {
  return at(HandleToIndex(handle)).IsMarked();
}

// Static
std::atomic<bool> JSDispatchTable::initialized_ = false;
#endif  // DEBUG

void JSDispatchTable::PrintEntry(JSDispatchHandle handle) {
  uint32_t index = HandleToIndex(handle);
  i::PrintF("JSDispatchEntry @ %p\n", &at(index));
  i::PrintF("* code 0x%lx\n", GetCode(handle).address());
  i::PrintF("* params %d\n", at(HandleToIndex(handle)).GetParameterCount());
  i::PrintF("* entrypoint 0x%lx\n", GetEntrypoint(handle));
}

// Static
base::LeakyObject<JSDispatchTable> JSDispatchTable::instance_;

}  // namespace internal
}  // namespace v8

#endif  // V8_ENABLE_SANDBOX
