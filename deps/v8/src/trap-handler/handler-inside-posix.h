// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_TRAP_HANDLER_HANDLER_INSIDE_POSIX_H_
#define V8_TRAP_HANDLER_HANDLER_INSIDE_POSIX_H_

#include <signal.h>

namespace v8 {
namespace internal {
namespace trap_handler {

void HandleSignal(int signum, siginfo_t* info, void* context);

bool TryHandleSignal(int signum, siginfo_t* info, void* context);

}  // namespace trap_handler
}  // namespace internal
}  // namespace v8

#endif  // V8_TRAP_HANDLER_HANDLER_INSIDE_POSIX_H_
