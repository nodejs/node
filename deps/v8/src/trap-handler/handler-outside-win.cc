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

namespace v8 {
namespace internal {
namespace trap_handler {

#if V8_TRAP_HANDLER_SUPPORTED
bool RegisterDefaultTrapHandler() {
  // Not yet implemented
  return false;
}
#endif

}  // namespace trap_handler
}  // namespace internal
}  // namespace v8
