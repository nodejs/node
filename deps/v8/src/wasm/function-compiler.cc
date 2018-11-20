// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/wasm/function-compiler.h"

#include "src/compiler/wasm-compiler.h"
#include "src/counters.h"
#include "src/macro-assembler-inl.h"
#include "src/wasm/baseline/liftoff-compiler.h"
#include "src/wasm/wasm-code-manager.h"

namespace v8 {
namespace internal {
namespace wasm {

namespace {
const char* GetCompilationModeAsString(
    WasmCompilationUnit::CompilationMode mode) {
  switch (mode) {
    case WasmCompilationUnit::CompilationMode::kLiftoff:
      return "liftoff";
    case WasmCompilationUnit::CompilationMode::kTurbofan:
      return "turbofan";
  }
  UNREACHABLE();
}
}  // namespace

// static
WasmCompilationUnit::CompilationMode
WasmCompilationUnit::GetDefaultCompilationMode() {
  return FLAG_liftoff ? CompilationMode::kLiftoff : CompilationMode::kTurbofan;
}

WasmCompilationUnit::WasmCompilationUnit(Isolate* isolate, ModuleEnv* env,
                                         wasm::NativeModule* native_module,
                                         wasm::FunctionBody body,
                                         wasm::WasmName name, int index,
                                         CompilationMode mode,
                                         Counters* counters, bool lower_simd)
    : env_(env),
      wasm_engine_(isolate->wasm_engine()),
      func_body_(body),
      func_name_(name),
      counters_(counters ? counters : isolate->counters()),
      func_index_(index),
      native_module_(native_module),
      lower_simd_(lower_simd),
      mode_(mode) {
  DCHECK_GE(index, env->module->num_imported_functions);
  DCHECK_LT(index, env->module->functions.size());
  // Always disable Liftoff for asm.js, for two reasons:
  //    1) asm-specific opcodes are not implemented, and
  //    2) tier-up does not work with lazy compilation.
  if (env->module->origin == kAsmJsOrigin) mode = CompilationMode::kTurbofan;
  if (V8_UNLIKELY(FLAG_wasm_tier_mask_for_testing) && index < 32 &&
      (FLAG_wasm_tier_mask_for_testing & (1 << index))) {
    mode = CompilationMode::kTurbofan;
  }
  SwitchMode(mode);
}

// Declared here such that {LiftoffCompilationUnit} and
// {TurbofanWasmCompilationUnit} can be opaque in the header file.
WasmCompilationUnit::~WasmCompilationUnit() {}

void WasmCompilationUnit::ExecuteCompilation() {
  auto size_histogram = SELECT_WASM_COUNTER(counters_, env_->module->origin,
                                            wasm, function_size_bytes);
  size_histogram->AddSample(
      static_cast<int>(func_body_.end - func_body_.start));
  auto timed_histogram = SELECT_WASM_COUNTER(counters_, env_->module->origin,
                                             wasm_compile, function_time);
  TimedHistogramScope wasm_compile_function_time_scope(timed_histogram);

  if (FLAG_trace_wasm_compiler) {
    PrintF("Compiling wasm function %d with %s\n\n", func_index_,
           GetCompilationModeAsString(mode_));
  }

  switch (mode_) {
    case WasmCompilationUnit::CompilationMode::kLiftoff:
      if (liftoff_unit_->ExecuteCompilation()) break;
      // Otherwise, fall back to turbofan.
      SwitchMode(CompilationMode::kTurbofan);
      V8_FALLTHROUGH;
    case WasmCompilationUnit::CompilationMode::kTurbofan:
      turbofan_unit_->ExecuteCompilation();
      break;
  }
}

wasm::WasmCode* WasmCompilationUnit::FinishCompilation(
    wasm::ErrorThrower* thrower) {
  wasm::WasmCode* ret;
  switch (mode_) {
    case CompilationMode::kLiftoff:
      ret = liftoff_unit_->FinishCompilation(thrower);
      break;
    case CompilationMode::kTurbofan:
      ret = turbofan_unit_->FinishCompilation(thrower);
      break;
    default:
      UNREACHABLE();
  }
  if (ret == nullptr) {
    thrower->RuntimeError("Error finalizing code.");
  }
  return ret;
}

void WasmCompilationUnit::SwitchMode(CompilationMode new_mode) {
  // This method is being called in the constructor, where neither
  // {liftoff_unit_} nor {turbofan_unit_} are set, or to switch mode from
  // kLiftoff to kTurbofan, in which case {liftoff_unit_} is already set.
  mode_ = new_mode;
  switch (new_mode) {
    case CompilationMode::kLiftoff:
      DCHECK(!turbofan_unit_);
      DCHECK(!liftoff_unit_);
      liftoff_unit_.reset(new LiftoffCompilationUnit(this));
      return;
    case CompilationMode::kTurbofan:
      DCHECK(!turbofan_unit_);
      liftoff_unit_.reset();
      turbofan_unit_.reset(new compiler::TurbofanWasmCompilationUnit(this));
      return;
  }
  UNREACHABLE();
}

// static
wasm::WasmCode* WasmCompilationUnit::CompileWasmFunction(
    wasm::NativeModule* native_module, wasm::ErrorThrower* thrower,
    Isolate* isolate, ModuleEnv* env, const wasm::WasmFunction* function,
    CompilationMode mode) {
  ModuleWireBytes wire_bytes(native_module->wire_bytes());
  wasm::FunctionBody function_body{
      function->sig, function->code.offset(),
      wire_bytes.start() + function->code.offset(),
      wire_bytes.start() + function->code.end_offset()};

  WasmCompilationUnit unit(isolate, env, native_module, function_body,
                           wire_bytes.GetNameOrNull(function, env->module),
                           function->func_index, mode);
  unit.ExecuteCompilation();
  return unit.FinishCompilation(thrower);
}

}  // namespace wasm
}  // namespace internal
}  // namespace v8
