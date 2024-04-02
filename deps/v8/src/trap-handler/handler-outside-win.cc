// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// PLEASE READ BEFORE CHANGING THIS FILE!
//
// This file implements the support code for the out of bounds trap handler.
// Nothing in here actually runs in the trap handler, but the code here
// manipulates data structures used by the trap handler so we still need to be
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
// For the code that runs in the trap handler itself, see handler-inside.cc.

#include <windows.h>

#include "src/trap-handler/handler-inside-win.h"
#include "src/trap-handler/trap-handler.h"

namespace v8 {
namespace internal {
namespace trap_handler {

#if V8_TRAP_HANDLER_SUPPORTED

namespace {

// A handle to our registered exception handler, so that we can remove it
// again later.
void* g_registered_handler = nullptr;

}  // namespace

bool RegisterDefaultTrapHandler() {
  constexpr ULONG first = TRUE;
  TH_CHECK(g_registered_handler == nullptr);
  g_registered_handler = AddVectoredExceptionHandler(first, HandleWasmTrap);

  return nullptr != g_registered_handler;
}

void RemoveTrapHandler() {
  if (!g_registered_handler) return;

  RemoveVectoredExceptionHandler(g_registered_handler);
  g_registered_handler = nullptr;
}

#endif  // V8_TRAP_HANDLER_SUPPORTED

}  // namespace trap_handler
}  // namespace internal
}  // namespace v8
