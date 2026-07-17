// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/sandbox/sandboxable-thread.h"

#ifdef V8_ENABLE_SANDBOX_HARDWARE_SUPPORT
#include <alloca.h>
#endif

#include "src/base/logging.h"
#include "src/base/platform/platform.h"
#include "src/sandbox/hardware-support.h"
#include "src/utils/allocation.h"

namespace v8 {
namespace internal {

void SandboxableThread::Dispatch() {
#ifdef V8_ENABLE_SANDBOX_HARDWARE_SUPPORT
  // Set up this thread for sandboxing.
  //
  // This involves setting the "sandbox extension" pkey for the thread's stack.
  // However, since we're using pthreads, the top of the stack segment will
  // contain the TLS block which we should not set a PKEY on (both because we
  // don't want to allow writes to TLS from sandboxed code, but also because a
  // PKEY-tagged TLS will quickly cause weird crashes in e.g. signal handlers).
  // To work around that, we reserve one page of stack memory here, then only
  // set the pkey for all pages below the current one.
  size_t page_size = AllocatePageSize();
  char* stack_buffer = static_cast<char*>(alloca(page_size));

  // Use the buffer to prevent it from being optimized away.
  *reinterpret_cast<volatile char*>(stack_buffer) = 42;
  *reinterpret_cast<volatile char*>(&stack_buffer[page_size - 1]) = 42;

  void* top_of_sandboxed_stack = stack_buffer;
  SandboxHardwareSupport::EnableForCurrentThread(top_of_sandboxed_stack);
#endif
  Run();
}

}  // namespace internal
}  // namespace v8
