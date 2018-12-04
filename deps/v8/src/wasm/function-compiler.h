// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_FUNCTION_COMPILER_H_
#define V8_WASM_FUNCTION_COMPILER_H_

#include "src/wasm/function-body-decoder.h"
#include "src/wasm/wasm-limits.h"
#include "src/wasm/wasm-module.h"
#include "src/wasm/wasm-tier.h"

namespace v8 {
namespace internal {

class Counters;

namespace compiler {
class TurbofanWasmCompilationUnit;
}  // namespace compiler

namespace wasm {

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

// The {ModuleEnv} encapsulates the module data that is used during compilation.
// ModuleEnvs are shareable across multiple compilations.
struct ModuleEnv {
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

  constexpr ModuleEnv(const WasmModule* module, UseTrapHandler use_trap_handler,
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

class WasmCompilationUnit final {
 public:
  static ExecutionTier GetDefaultExecutionTier();

  // If constructing from a background thread, pass in a Counters*, and ensure
  // that the Counters live at least as long as this compilation unit (which
  // typically means to hold a std::shared_ptr<Counters>).
  // If used exclusively from a foreground thread, Isolate::counters() may be
  // used by callers to pass Counters.
  WasmCompilationUnit(WasmEngine* wasm_engine, ModuleEnv*, NativeModule*,
                      FunctionBody, int index, Counters*,
                      ExecutionTier = GetDefaultExecutionTier());

  ~WasmCompilationUnit();

  void ExecuteCompilation(WasmFeatures* detected);
  WasmCode* FinishCompilation(ErrorThrower* thrower);

  static WasmCode* CompileWasmFunction(
      Isolate* isolate, NativeModule* native_module, WasmFeatures* detected,
      ErrorThrower* thrower, ModuleEnv* env, const WasmFunction* function,
      ExecutionTier = GetDefaultExecutionTier());

  NativeModule* native_module() const { return native_module_; }
  ExecutionTier mode() const { return mode_; }

 private:
  friend class LiftoffCompilationUnit;
  friend class compiler::TurbofanWasmCompilationUnit;

  ModuleEnv* env_;
  WasmEngine* wasm_engine_;
  FunctionBody func_body_;
  Counters* counters_;
  int func_index_;
  NativeModule* native_module_;
  ExecutionTier mode_;
  // LiftoffCompilationUnit, set if {mode_ == kLiftoff}.
  std::unique_ptr<LiftoffCompilationUnit> liftoff_unit_;
  // TurbofanWasmCompilationUnit, set if {mode_ == kTurbofan}.
  std::unique_ptr<compiler::TurbofanWasmCompilationUnit> turbofan_unit_;

  void SwitchMode(ExecutionTier new_mode);

  DISALLOW_COPY_AND_ASSIGN(WasmCompilationUnit);
};

}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif  // V8_WASM_FUNCTION_COMPILER_H_
