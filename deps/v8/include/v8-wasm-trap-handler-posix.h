// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_TRAP_HANDLER_POSIX_H_
#define V8_WASM_TRAP_HANDLER_POSIX_H_

#include <signal.h>

#include "v8config.h"  // NOLINT(build/include_directory)

namespace v8 {
/**
 * This function determines whether a memory access violation has been an
 * out-of-bounds memory access in WebAssembly. If so, it will modify the context
 * parameter and add a return address where the execution can continue after the
 * signal handling, and return true. Otherwise, false will be returned.
 *
 * The parameters to this function correspond to those passed to a Posix signal
 * handler. Use this function only on Linux and Mac.
 *
 * \param sig_code The signal code, e.g. SIGSEGV.
 * \param info A pointer to the siginfo_t struct provided to the signal handler.
 * \param context A pointer to a ucontext_t struct provided to the signal
 * handler.
 */
V8_EXPORT bool TryHandleWebAssemblyTrapPosix(int sig_code, siginfo_t* info,
                                             void* context);

}  // namespace v8
#endif  // V8_WASM_TRAP_HANDLER_POSIX_H_
