// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/base/platform/platform.h"

namespace v8 {
namespace base {

namespace {

// A pointer to current thread's stack beginning.
thread_local void* thread_stack_start = nullptr;

}  // namespace

// static
Stack::StackSlot Stack::GetStackStartUnchecked() {
  if (!thread_stack_start) {
    thread_stack_start = ObtainCurrentThreadStackStart();
  }
  return thread_stack_start;
}

// static
Stack::StackSlot Stack::GetStackStart() { return GetStackStartUnchecked(); }

// static
int OS::GetCurrentThreadId() {
  static thread_local int id = GetCurrentThreadIdInternal();
  return id;
}

}  // namespace base
}  // namespace v8
