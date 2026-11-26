// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/base/platform/platform.h"

namespace v8 {
namespace base {

namespace {

struct ThreadStackData {
  void* os_stack_start = nullptr;
  void* reserved_stack_limit = nullptr;
  bool initialized = false;
};

// Current thread's stack bounds, as reported by the OS.
thread_local ThreadStackData thread_stack_data;

// A pointer to current thread's stack start, as overridden by V8 or by the
// embedder.
thread_local void* custom_thread_stack_start = nullptr;

}  // namespace

// static
void Stack::EnsureThreadStackDataInitialized() {
  if (!thread_stack_data.initialized) [[unlikely]] {
    StackBounds bounds = ObtainCurrentThreadStackBounds();
    thread_stack_data.os_stack_start = bounds.start;
    thread_stack_data.reserved_stack_limit = bounds.reserved_limit;
    thread_stack_data.initialized = true;
  }
}

// static
Stack::StackSlot Stack::GetReservedStackLimit() {
  EnsureThreadStackDataInitialized();
  return thread_stack_data.reserved_stack_limit;
}

// static
Stack::StackSlot Stack::GetStackStart() {
  EnsureThreadStackDataInitialized();
  return thread_stack_data.os_stack_start;
}

// static
Stack::StackBounds Stack::GetStackBounds() {
  EnsureThreadStackDataInitialized();
  return {thread_stack_data.os_stack_start,
          thread_stack_data.reserved_stack_limit};
}

// static
Stack::StackSlot Stack::GetEffectiveStackStart() {
  if (custom_thread_stack_start) return custom_thread_stack_start;
  return GetStackStart();
}

// static
Stack::StackSlot Stack::SetCurrentThreadStackStart(StackSlot stack_start) {
  if (stack_start != nullptr) {
    DCHECK_LE(GetCurrentStackPosition(), stack_start);
    DCHECK_LE(stack_start, GetStackStart());
  }
  StackSlot previous = custom_thread_stack_start;
  custom_thread_stack_start = stack_start;
  return previous;
}

// static
bool Stack::HasCustomCurrentThreadStackStart() {
  return custom_thread_stack_start != nullptr;
}

// static
void Stack::ValidateStackSegment(const void* start, const void* top,
                                 bool check_for_tls) {
  CHECK_NOT_NULL(start);
  CHECK_NOT_NULL(top);
  CHECK_LE(top, start);
  const void* current_stack_start = GetStackStart();
  const void* current_stack_top = GetCurrentStackPosition();
  CHECK_LE(current_stack_top, top);
  CHECK_LE(start, current_stack_start);
  // TODO(540324230): Remove check_for_tls entirely and check this
  // unconditionally, once embedders correctly set the stack start for all
  // threads.
  if (check_for_tls) {
    // Check that the stack segment does not contain TLS data. This may be
    // violated on Linux with glibc, because pthread_create allocates a
    // single memory region for TLS data and for the stack and all threads
    // except the main thread of the process report the wrong stack start.
    //
    // If this check fails, it probably means that an embedder using V8
    // should have used v8::StackStartScope to appropriately set the stack
    // start for the current thread.
    const void* tls = &thread_stack_data;
    CHECK(!(top <= tls && tls < start));
  }
}

// static
int OS::GetCurrentThreadId() {
  static thread_local int id = GetCurrentThreadIdInternal();
  return id;
}

}  // namespace base
}  // namespace v8
