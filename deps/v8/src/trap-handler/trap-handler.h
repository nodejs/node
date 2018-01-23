// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_TRAP_HANDLER_H_
#define V8_TRAP_HANDLER_H_

#include <signal.h>
#include <stdint.h>
#include <stdlib.h>

#include "src/base/build_config.h"
#include "src/flags.h"
#include "src/globals.h"

#if V8_OS_LINUX
#include <ucontext.h>
#endif

namespace v8 {
namespace internal {
namespace trap_handler {

// TODO(eholk): Support trap handlers on other platforms.
#if V8_TARGET_ARCH_X64 && V8_OS_LINUX && !V8_OS_ANDROID
#define V8_TRAP_HANDLER_SUPPORTED 1
#else
#define V8_TRAP_HANDLER_SUPPORTED 0
#endif

struct ProtectedInstructionData {
  // The offset of this instruction from the start of its code object.
  // Wasm code never grows larger than 2GB, so uint32_t is sufficient.
  uint32_t instr_offset;

  // The offset of the landing pad from the start of its code object.
  //
  // TODO(eholk): Using a single landing pad and store parameters here.
  uint32_t landing_offset;
};

const int kInvalidIndex = -1;

/// Adjusts the base code pointer.
void UpdateHandlerDataCodePointer(int index, void* base);

/// Adds the handler data to the place where the signal handler will find it.
///
/// This returns a number that can be used to identify the handler data to
/// UpdateHandlerDataCodePointer and ReleaseHandlerData, or -1 on failure.
int RegisterHandlerData(void* base, size_t size,
                        size_t num_protected_instructions,
                        const ProtectedInstructionData* protected_instructions);

/// Removes the data from the master list and frees any memory, if necessary.
/// TODO(mtrofin): once FLAG_wasm_jit_to_native is not needed, we can switch
/// to using size_t for index and not need kInvalidIndex.
void ReleaseHandlerData(int index);

#if V8_OS_WIN
#define THREAD_LOCAL __declspec(thread)
#elif V8_OS_ANDROID
// TODO(eholk): fix this before enabling for trap handlers for Android.
#define THREAD_LOCAL
#else
#define THREAD_LOCAL __thread
#endif

inline bool IsTrapHandlerEnabled() {
  return FLAG_wasm_trap_handler && V8_TRAP_HANDLER_SUPPORTED;
}

extern THREAD_LOCAL int g_thread_in_wasm_code;

inline bool IsThreadInWasm() { return g_thread_in_wasm_code; }

inline void SetThreadInWasm() {
  if (IsTrapHandlerEnabled()) {
    DCHECK(!IsThreadInWasm());
    g_thread_in_wasm_code = true;
  }
}

inline void ClearThreadInWasm() {
  if (IsTrapHandlerEnabled()) {
    DCHECK(IsThreadInWasm());
    g_thread_in_wasm_code = false;
  }
}

bool RegisterDefaultSignalHandler();
V8_EXPORT_PRIVATE void RestoreOriginalSignalHandler();

#if V8_OS_LINUX
bool TryHandleSignal(int signum, siginfo_t* info, ucontext_t* context);
#endif  // V8_OS_LINUX

size_t GetRecoveredTrapCount();

}  // namespace trap_handler
}  // namespace internal
}  // namespace v8

#endif  // V8_TRAP_HANDLER_H_
