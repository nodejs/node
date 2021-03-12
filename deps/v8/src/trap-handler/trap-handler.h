// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_TRAP_HANDLER_TRAP_HANDLER_H_
#define V8_TRAP_HANDLER_TRAP_HANDLER_H_

#include <stdint.h>
#include <stdlib.h>

#include <atomic>

#include "src/base/build_config.h"
#include "src/common/globals.h"
#include "src/flags/flags.h"

namespace v8 {
namespace internal {
namespace trap_handler {

// TODO(eholk): Support trap handlers on other platforms.
#if V8_TARGET_ARCH_X64 && V8_OS_LINUX && !V8_OS_ANDROID
#define V8_TRAP_HANDLER_SUPPORTED true
#elif V8_TARGET_ARCH_X64 && V8_OS_WIN
#define V8_TRAP_HANDLER_SUPPORTED true
#elif V8_TARGET_ARCH_X64 && V8_OS_MACOSX
#define V8_TRAP_HANDLER_SUPPORTED true
#elif V8_TARGET_ARCH_X64 && V8_OS_FREEBSD
#define V8_TRAP_HANDLER_SUPPORTED true
#elif V8_HOST_ARCH_ARM64 && V8_TARGET_ARCH_ARM64 && V8_OS_MACOSX
#define V8_TRAP_HANDLER_SUPPORTED true
#else
#define V8_TRAP_HANDLER_SUPPORTED false
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

/// Adds the handler data to the place where the trap handler will find it.
///
/// This returns a number that can be used to identify the handler data to
/// ReleaseHandlerData, or -1 on failure.
int V8_EXPORT_PRIVATE RegisterHandlerData(
    Address base, size_t size, size_t num_protected_instructions,
    const ProtectedInstructionData* protected_instructions);

/// Removes the data from the master list and frees any memory, if necessary.
/// TODO(mtrofin): We can switch to using size_t for index and not need
/// kInvalidIndex.
void V8_EXPORT_PRIVATE ReleaseHandlerData(int index);

#if V8_OS_WIN
#define THREAD_LOCAL __declspec(thread)
#elif V8_OS_ANDROID
// TODO(eholk): fix this before enabling for trap handlers for Android.
#define THREAD_LOCAL
#else
#define THREAD_LOCAL __thread
#endif

// Initially false, set to true if when trap handlers are enabled. Never goes
// back to false then.
extern bool g_is_trap_handler_enabled;

// Initially true, set to false when either {IsTrapHandlerEnabled} or
// {EnableTrapHandler} is called to prevent calling {EnableTrapHandler}
// repeatedly, or after {IsTrapHandlerEnabled}. Needs to be atomic because
// {IsTrapHandlerEnabled} can be called from any thread. Updated using relaxed
// semantics, since it's not used for synchronization.
extern std::atomic<bool> g_can_enable_trap_handler;

// Enables trap handling for WebAssembly bounds checks.
//
// use_v8_handler indicates that V8 should install its own handler
// rather than relying on the embedder to do it.
V8_EXPORT_PRIVATE bool EnableTrapHandler(bool use_v8_handler);

inline bool IsTrapHandlerEnabled() {
  DCHECK_IMPLIES(g_is_trap_handler_enabled, V8_TRAP_HANDLER_SUPPORTED);
  // Disallow enabling the trap handler after retrieving the current value.
  // Re-enabling them late can produce issues because code or objects might have
  // been generated under the assumption that trap handlers are disabled.
  // Note: We test before setting to avoid contention by an unconditional write.
  if (g_can_enable_trap_handler.load(std::memory_order_relaxed)) {
    g_can_enable_trap_handler.store(false, std::memory_order_relaxed);
  }
  return g_is_trap_handler_enabled;
}

extern THREAD_LOCAL int g_thread_in_wasm_code;

// Return the address of the thread-local {g_thread_in_wasm_code} variable. This
// pointer can be accessed and modified as long as the thread calling this
// function exists. Only use if from the same thread do avoid race conditions.
V8_NOINLINE V8_EXPORT_PRIVATE int* GetThreadInWasmThreadLocalAddress();

// On Windows, asan installs its own exception handler which maps shadow
// memory. Since our exception handler may be executed before the asan exception
// handler, we have to make sure that asan shadow memory is not accessed here.
DISABLE_ASAN inline bool IsThreadInWasm() { return g_thread_in_wasm_code; }

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

bool RegisterDefaultTrapHandler();
V8_EXPORT_PRIVATE void RemoveTrapHandler();

size_t GetRecoveredTrapCount();

}  // namespace trap_handler
}  // namespace internal
}  // namespace v8

#endif  // V8_TRAP_HANDLER_TRAP_HANDLER_H_
