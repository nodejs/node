// Copyright 2017 The Abseil Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// Portable implementation using <execinfo.h> backtrace()
//
// Note:  The glibc implementation may cause a call to malloc.
// This can cause a deadlock in HeapProfiler.

#ifndef ABSL_DEBUGGING_INTERNAL_STACKTRACE_GENERIC_INL_H_
#define ABSL_DEBUGGING_INTERNAL_STACKTRACE_GENERIC_INL_H_

#include <execinfo.h>
#include <atomic>
#include <cstring>

#include "absl/debugging/stacktrace.h"
#include "absl/base/attributes.h"

// Sometimes, we can try to get a stack trace from within a stack
// trace, because we don't block signals inside this code (which would be too
// expensive: the two extra system calls per stack trace do matter here).
// That can cause a self-deadlock.
// Protect against such reentrant call by failing to get a stack trace.
//
// We use __thread here because the code here is extremely low level -- it is
// called while collecting stack traces from within malloc and mmap, and thus
// can not call anything which might call malloc or mmap itself.
static __thread int recursive = 0;

#if defined(__GLIBC__)
// glibc's backtrace() lazily dlopen()s libgcc_s on first use, which
// allocates. If that first call happens very early (e.g. from the first
// malloc when using tcmalloc), it can recurse into an uninitialized
// allocator. Suppress stack traces until static initialization has run.
// Other libcs that use this file (bionic, Apple) don't lazy-load.
static std::atomic<bool> disable_stacktraces(true);  // Disabled until healthy.
// This file is included into stacktrace.cc so this will only run once.
[[maybe_unused]] static int stacktraces_enabler = []() {
  void* unused_stack[1];
  // Force the first backtrace to happen early to get the one-time shared lib
  // loading (allocation) out of the way. After the first call it is much safer
  // to use backtrace from a signal handler if we crash somewhere later.
  backtrace(unused_stack, 1);
  disable_stacktraces.store(false, std::memory_order_relaxed);
  return 0;
}();
#endif  // defined(__GLIBC__)

template <bool IS_STACK_FRAMES, bool IS_WITH_CONTEXT>
static int UnwindImpl(void** result, uintptr_t* frames, int* sizes,
                      int max_depth, int skip_count, const void* ucp,
                      int* min_dropped_frames) {
  if (recursive
#if defined(__GLIBC__)
      || disable_stacktraces.load(std::memory_order_relaxed)
#endif
  ) {
    return 0;
  }
  ++recursive;

  static_cast<void>(ucp);  // Unused.
  static const int kStackLength = 64;
  void * stack[kStackLength];
  int size;

  size = backtrace(stack, kStackLength);
  skip_count++;  // we want to skip the current frame as well

  int num_frames = size - skip_count;
  if (num_frames < 0) num_frames = 0;
  if (num_frames > max_depth) num_frames = max_depth;

  int result_count = 0;
  for (int i = 0; i < num_frames; i++) {
    int stack_index = i + skip_count;
    // Follow x86 and stop if the return address is null (end of stack).
    if (stack[stack_index] == nullptr) {
      break;
    }
    result[result_count++] = stack[stack_index];
  }

  if (IS_STACK_FRAMES) {
    // No implementation for finding out the stack frames yet.
    if (frames != nullptr) {
      memset(frames, 0, sizeof(*frames) * static_cast<size_t>(result_count));
    }
    if (sizes != nullptr) {
      memset(sizes, 0, sizeof(*sizes) * static_cast<size_t>(result_count));
    }
  }
  if (min_dropped_frames != nullptr) {
    if (size - skip_count - max_depth > 0) {
      *min_dropped_frames = size - skip_count - max_depth;
    } else {
      *min_dropped_frames = 0;
    }
  }

  --recursive;

  return result_count;
}

namespace absl {
ABSL_NAMESPACE_BEGIN
namespace debugging_internal {
bool StackTraceWorksForTest() {
  return true;
}
}  // namespace debugging_internal
ABSL_NAMESPACE_END
}  // namespace absl

#endif  // ABSL_DEBUGGING_INTERNAL_STACKTRACE_GENERIC_INL_H_
