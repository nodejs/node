// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_TRAP_HANDLER_HANDLER_INSIDE_WIN_H_
#define V8_TRAP_HANDLER_HANDLER_INSIDE_WIN_H_

#include <windows.h>

#include "src/base/macros.h"

namespace v8 {
namespace internal {
namespace trap_handler {

LONG WINAPI HandleWasmTrap(EXCEPTION_POINTERS* exception);

// On Windows, asan installs its own exception handler which maps shadow
// memory. Since our exception handler may be executed before the asan exception
// handler, we have to make sure that asan shadow memory is not accessed here.
DISABLE_ASAN bool TryHandleWasmTrap(EXCEPTION_POINTERS* exception);

}  // namespace trap_handler
}  // namespace internal
}  // namespace v8

#endif  // V8_TRAP_HANDLER_HANDLER_INSIDE_WIN_H_
