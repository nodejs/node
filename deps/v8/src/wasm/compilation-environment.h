// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_COMPILATION_ENVIRONMENT_H_
#define V8_WASM_COMPILATION_ENVIRONMENT_H_

#include "src/wasm/function-body-decoder.h"
#include "src/wasm/wasm-limits.h"
#include "src/wasm/wasm-module.h"
#include "src/wasm/wasm-tier.h"

namespace v8 {
namespace internal {
namespace wasm {

class CompilationState;
class LiftoffCompilationUnit;
struct ModuleWireBytes;
class NativeModule;
class WasmCode;
class WasmEngine;
struct WasmFunction;

enum RuntimeExceptionSupport : bool {
  kRuntimeExceptionSupport = true,
  kNoRuntimeExceptionSupport = false
};

enum UseTrapHandler : bool { kUseTrapHandler = true, kNoTrapHandler = false };

enum LowerSimd : bool { kLowerSimd = true, kNoLowerSimd = false };

// The {CompilationEnv} encapsulates the module data that is used during
// compilation. CompilationEnvs are shareable across multiple compilations.
struct CompilationEnv {
  // A pointer to the decoded module's static representation.
  const WasmModule* const module;

  // True if trap handling should be used in compiled code, rather than
  // compiling in bounds checks for each memory access.
  const UseTrapHandler use_trap_handler;

  // If the runtime doesn't support exception propagation,
  // we won't generate stack checks, and trap handling will also
  // be generated differently.
  const RuntimeExceptionSupport runtime_exception_support;

  // The smallest size of any memory that could be used with this module, in
  // bytes.
  const uint64_t min_memory_size;

  // The largest size of any memory that could be used with this module, in
  // bytes.
  const uint64_t max_memory_size;

  const LowerSimd lower_simd;

  constexpr CompilationEnv(const WasmModule* module,
                           UseTrapHandler use_trap_handler,
                           RuntimeExceptionSupport runtime_exception_support,
                           LowerSimd lower_simd = kNoLowerSimd)
      : module(module),
        use_trap_handler(use_trap_handler),
        runtime_exception_support(runtime_exception_support),
        min_memory_size(module ? module->initial_pages * uint64_t{kWasmPageSize}
                               : 0),
        max_memory_size(module && module->has_maximum_pages
                            ? (module->maximum_pages * uint64_t{kWasmPageSize})
                            : kSpecMaxWasmMemoryBytes),
        lower_simd(lower_simd) {}
};

// The implementation of {CompilationState}, {CompilationStateDeleter} and
// {CancelAndWaitCompilationState} lives in module-compiler.cc.
struct CompilationStateDeleter {
  void operator()(CompilationState* compilation_state) const;
};

void CancelAndWaitCompilationState(CompilationState* state);

// Wrapper to create a CompilationState exists in order to avoid having
// the CompilationState in the header file.
std::unique_ptr<CompilationState, CompilationStateDeleter> NewCompilationState(
    Isolate*, NativeModule*);

}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif  // V8_WASM_COMPILATION_ENVIRONMENT_H_
