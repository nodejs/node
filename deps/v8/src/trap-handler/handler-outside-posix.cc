// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// PLEASE READ BEFORE CHANGING THIS FILE!
//
// This file implements the support code for the out of bounds signal handler.
// Nothing in here actually runs in the signal handler, but the code here
// manipulates data structures used by the signal handler so we still need to be
// careful. In order to minimize this risk, here are some rules to follow.
//
// 1. Avoid introducing new external dependencies. The files in src/trap-handler
//    should be as self-contained as possible to make it easy to audit the code.
//
// 2. Any changes must be reviewed by someone from the crash reporting
//    or security team. Se OWNERS for suggested reviewers.
//
// For more information, see https://goo.gl/yMeyUY.
//
// For the code that runs in the signal handler itself, see handler-inside.cc.

#include <signal.h>

#include <cstdio>

#include "src/trap-handler/handler-inside-posix.h"
#include "src/trap-handler/trap-handler-internal.h"

namespace v8 {
namespace internal {
namespace trap_handler {

#if V8_TRAP_HANDLER_SUPPORTED
namespace {
struct sigaction g_old_handler;

// When using the default signal handler, we save the old one to restore in case
// V8 chooses not to handle the signal.
bool g_is_default_signal_handler_registered;

}  // namespace

bool RegisterDefaultTrapHandler() {
  TH_CHECK(!g_is_default_signal_handler_registered);

  struct sigaction action;
  action.sa_sigaction = HandleSignal;
  action.sa_flags = SA_SIGINFO;
  sigemptyset(&action.sa_mask);
  // {sigaction} installs a new custom segfault handler. On success, it returns
  // 0. If we get a nonzero value, we report an error to the caller by returning
  // false.
  if (sigaction(kOobSignal, &action, &g_old_handler) != 0) {
    return false;
  }

// Sanitizers often prevent us from installing our own signal handler. Attempt
// to detect this and if so, refuse to enable trap handling.
//
// TODO(chromium:830894): Remove this once all bots support custom signal
// handlers.
#if defined(ADDRESS_SANITIZER) || defined(MEMORY_SANITIZER) || \
    defined(THREAD_SANITIZER) || defined(LEAK_SANITIZER) ||    \
    defined(UNDEFINED_SANITIZER)
  struct sigaction installed_handler;
  TH_CHECK(sigaction(kOobSignal, NULL, &installed_handler) == 0);
  // If the installed handler does not point to HandleSignal, then
  // allow_user_segv_handler is 0.
  if (installed_handler.sa_sigaction != HandleSignal) {
    printf(
        "WARNING: sanitizers are preventing signal handler installation. "
        "Trap handlers are disabled.\n");
    return false;
  }
#endif

  g_is_default_signal_handler_registered = true;
  return true;
}

void RemoveTrapHandler() {
  if (g_is_default_signal_handler_registered) {
    if (sigaction(kOobSignal, &g_old_handler, nullptr) == 0) {
      g_is_default_signal_handler_registered = false;
    }
  }
}
#endif  // V8_TRAP_HANDLER_SUPPORTED

}  // namespace trap_handler
}  // namespace internal
}  // namespace v8
