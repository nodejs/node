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
  WasmCompilationUnit(WasmEngine*, NativeModule*, FunctionBody, int index,
                      Counters*, ExecutionTier = GetDefaultExecutionTier());

  ~WasmCompilationUnit();

  void ExecuteCompilation(CompilationEnv*, WasmFeatures* detected);

  NativeModule* native_module() const { return native_module_; }
  ExecutionTier mode() const { return mode_; }
  bool failed() const { return result_.failed(); }
  WasmCode* result() const {
    DCHECK(!failed());
    DCHECK_NOT_NULL(result_.value());
    return result_.value();
  }

  void ReportError(ErrorThrower* thrower) const;

  static bool CompileWasmFunction(Isolate* isolate, NativeModule* native_module,
                                  WasmFeatures* detected, ErrorThrower* thrower,
                                  const WasmFunction* function,
                                  ExecutionTier = GetDefaultExecutionTier());

 private:
  friend class LiftoffCompilationUnit;
  friend class compiler::TurbofanWasmCompilationUnit;

  WasmEngine* wasm_engine_;
  FunctionBody func_body_;
  Counters* counters_;
  int func_index_;
  NativeModule* native_module_;
  ExecutionTier mode_;
  wasm::Result<WasmCode*> result_;

  // LiftoffCompilationUnit, set if {mode_ == kLiftoff}.
  std::unique_ptr<LiftoffCompilationUnit> liftoff_unit_;
  // TurbofanWasmCompilationUnit, set if {mode_ == kTurbofan}.
  std::unique_ptr<compiler::TurbofanWasmCompilationUnit> turbofan_unit_;

  void SwitchMode(ExecutionTier new_mode);

  // Called from {ExecuteCompilation} to set the result of compilation.
  void SetResult(WasmCode*);

  DISALLOW_COPY_AND_ASSIGN(WasmCompilationUnit);
};

}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif  // V8_WASM_FUNCTION_COMPILER_H_
