// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_FUNCTION_COMPILER_H_
#define V8_WASM_FUNCTION_COMPILER_H_

#include "src/wasm/compilation-environment.h"
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
class NativeModule;
class WasmCode;
class WasmEngine;
struct WasmFunction;

class WasmCompilationUnit final {
 public:
  static ExecutionTier GetDefaultExecutionTier();

  // If constructing from a background thread, pass in a Counters*, and ensure
  // that the Counters live at least as long as this compilation unit (which
  // typically means to hold a std::shared_ptr<Counters>).
  // If used exclusively from a foreground thread, Isolate::counters() may be
  // used by callers to pass Counters.
  WasmCompilationUnit(WasmEngine*, NativeModule*, int index,
                      ExecutionTier = GetDefaultExecutionTier());

  ~WasmCompilationUnit();

  void ExecuteCompilation(CompilationEnv*, std::shared_ptr<WireBytesStorage>,
                          Counters*, WasmFeatures* detected);

  NativeModule* native_module() const { return native_module_; }
  ExecutionTier tier() const { return tier_; }
  WasmCode* result() const { return result_; }

  static void CompileWasmFunction(Isolate* isolate, NativeModule* native_module,
                                  WasmFeatures* detected,
                                  const WasmFunction* function,
                                  ExecutionTier = GetDefaultExecutionTier());

 private:
  friend class LiftoffCompilationUnit;
  friend class compiler::TurbofanWasmCompilationUnit;

  WasmEngine* const wasm_engine_;
  const int func_index_;
  NativeModule* const native_module_;
  ExecutionTier tier_;
  WasmCode* result_ = nullptr;

  // LiftoffCompilationUnit, set if {tier_ == kLiftoff}.
  std::unique_ptr<LiftoffCompilationUnit> liftoff_unit_;
  // TurbofanWasmCompilationUnit, set if {tier_ == kTurbofan}.
  std::unique_ptr<compiler::TurbofanWasmCompilationUnit> turbofan_unit_;

  void SwitchTier(ExecutionTier new_tier);

  // Called from {ExecuteCompilation} to set the result of compilation.
  void SetResult(WasmCode*, Counters*);

  DISALLOW_COPY_AND_ASSIGN(WasmCompilationUnit);
};

}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif  // V8_WASM_FUNCTION_COMPILER_H_
