// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_TRAP_HANDLER_WIN_H_
#define V8_WASM_TRAP_HANDLER_WIN_H_

#include <windows.h>

#include "v8config.h"  // NOLINT(build/include)

namespace v8 {
/**
 * This function determines whether a memory access violation has been an
 * out-of-bounds memory access in WebAssembly. If so, it will modify the
 * exception parameter and add a return address where the execution can continue
 * after the exception handling, and return true. Otherwise the return value
 * will be false.
 *
 * The parameter to this function corresponds to the one passed to a Windows
 * vectored exception handler. Use this function only on Windows.
 *
 * \param exception An EXCEPTION_POINTERS* as provided to the exception handler.
 */
V8_EXPORT bool TryHandleWebAssemblyTrapWindows(EXCEPTION_POINTERS* exception);

}  // namespace v8
#endif  // V8_WASM_TRAP_HANDLER_WIN_H_
