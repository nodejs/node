// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/codegen/optimized-compilation-info.h"

#include "src/api/api.h"
#include "src/base/platform/wrappers.h"
#include "src/codegen/source-position.h"
#include "src/debug/debug.h"
#include "src/execution/isolate.h"
#include "src/objects/objects-inl.h"
#include "src/objects/shared-function-info.h"
#include "src/tracing/trace-event.h"
#include "src/tracing/traced-value.h"

#if V8_ENABLE_WEBASSEMBLY
#include "src/wasm/function-compiler.h"
#endif  // V8_ENABLE_WEBASSEMBLY

namespace v8 {
namespace internal {

OptimizedCompilationInfo::OptimizedCompilationInfo(
    Zone* zone, Isolate* isolate, Handle<SharedFunctionInfo> shared,
    Handle<JSFunction> closure, CodeKind code_kind)
    : code_kind_(code_kind),
      zone_(zone),
      optimization_id_(isolate->NextOptimizationId()) {
  DCHECK_EQ(*shared, closure->shared());
  DCHECK(shared->is_compiled());
  bytecode_array_ = handle(shared->GetBytecodeArray(isolate), isolate);
  shared_info_ = shared;
  closure_ = closure;

  // Collect source positions for optimized code when profiling or if debugger
  // is active, to be able to get more precise source positions at the price of
  // more memory consumption.
  if (isolate->NeedsDetailedOptimizedCodeLineInfo()) {
    set_source_positions();
  }

  SetTracingFlags(shared->PassesFilter(FLAG_trace_turbo_filter));
  ConfigureFlags();

  if (isolate->node_observer()) {
    SetNodeObserver(isolate->node_observer());
  }
}

OptimizedCompilationInfo::OptimizedCompilationInfo(
    Vector<const char> debug_name, Zone* zone, CodeKind code_kind)
    : code_kind_(code_kind),
      zone_(zone),
      optimization_id_(kNoOptimizationId),
      debug_name_(debug_name) {
  SetTracingFlags(
      PassesFilter(debug_name, CStrVector(FLAG_trace_turbo_filter)));
  ConfigureFlags();
}

#ifdef DEBUG
bool OptimizedCompilationInfo::FlagSetIsValid(Flag flag) const {
  switch (flag) {
    case kPoisonRegisterArguments:
      return untrusted_code_mitigations();
    case kFunctionContextSpecializing:
      return !IsNativeContextIndependent();
    default:
      return true;
  }
  UNREACHABLE();
}

bool OptimizedCompilationInfo::FlagGetIsValid(Flag flag) const {
  switch (flag) {
    case kPoisonRegisterArguments:
      if (!GetFlag(kPoisonRegisterArguments)) return true;
      return untrusted_code_mitigations() && called_with_code_start_register();
    default:
      return true;
  }
  UNREACHABLE();
}
#endif  // DEBUG

void OptimizedCompilationInfo::ConfigureFlags() {
  if (FLAG_untrusted_code_mitigations) set_untrusted_code_mitigations();

  switch (code_kind_) {
    case CodeKind::TURBOFAN:
      if (FLAG_function_context_specialization) {
        set_function_context_specializing();
      }
      V8_FALLTHROUGH;
    case CodeKind::TURBOPROP:
    case CodeKind::NATIVE_CONTEXT_INDEPENDENT:
      set_called_with_code_start_register();
      set_switch_jump_table();
      if (FLAG_turbo_splitting) set_splitting();
      if (FLAG_untrusted_code_mitigations) set_poison_register_arguments();
      // TODO(yangguo): Disable this in case of debugging for crbug.com/826613
      if (FLAG_analyze_environment_liveness) set_analyze_environment_liveness();
      break;
    case CodeKind::BYTECODE_HANDLER:
      set_called_with_code_start_register();
      if (FLAG_turbo_splitting) set_splitting();
      break;
    case CodeKind::BUILTIN:
    case CodeKind::FOR_TESTING:
      if (FLAG_turbo_splitting) set_splitting();
#if ENABLE_GDB_JIT_INTERFACE && DEBUG
      set_source_positions();
#endif  // ENABLE_GDB_JIT_INTERFACE && DEBUG
      break;
    case CodeKind::WASM_FUNCTION:
    case CodeKind::WASM_TO_CAPI_FUNCTION:
      set_switch_jump_table();
      break;
    default:
      break;
  }
}

OptimizedCompilationInfo::~OptimizedCompilationInfo() {
  if (disable_future_optimization() && has_shared_info()) {
    shared_info()->DisableOptimization(bailout_reason());
  }
}

void OptimizedCompilationInfo::ReopenHandlesInNewHandleScope(Isolate* isolate) {
  if (!shared_info_.is_null()) {
    shared_info_ = Handle<SharedFunctionInfo>(*shared_info_, isolate);
  }
  if (!bytecode_array_.is_null()) {
    bytecode_array_ = Handle<BytecodeArray>(*bytecode_array_, isolate);
  }
  if (!closure_.is_null()) {
    closure_ = Handle<JSFunction>(*closure_, isolate);
  }
  DCHECK(code_.is_null());
}

void OptimizedCompilationInfo::AbortOptimization(BailoutReason reason) {
  DCHECK_NE(reason, BailoutReason::kNoReason);
  if (bailout_reason_ == BailoutReason::kNoReason) {
    bailout_reason_ = reason;
  }
  set_disable_future_optimization();
}

void OptimizedCompilationInfo::RetryOptimization(BailoutReason reason) {
  DCHECK_NE(reason, BailoutReason::kNoReason);
  if (disable_future_optimization()) return;
  bailout_reason_ = reason;
}

std::unique_ptr<char[]> OptimizedCompilationInfo::GetDebugName() const {
  if (!shared_info().is_null()) {
    return shared_info()->DebugNameCStr();
  }
  Vector<const char> name_vec = debug_name_;
  if (name_vec.empty()) name_vec = ArrayVector("unknown");
  std::unique_ptr<char[]> name(new char[name_vec.length() + 1]);
  base::Memcpy(name.get(), name_vec.begin(), name_vec.length());
  name[name_vec.length()] = '\0';
  return name;
}

StackFrame::Type OptimizedCompilationInfo::GetOutputStackFrameType() const {
  switch (code_kind()) {
    case CodeKind::FOR_TESTING:
    case CodeKind::BYTECODE_HANDLER:
    case CodeKind::BUILTIN:
      return StackFrame::STUB;
#if V8_ENABLE_WEBASSEMBLY
    case CodeKind::WASM_FUNCTION:
      return StackFrame::WASM;
    case CodeKind::WASM_TO_CAPI_FUNCTION:
      return StackFrame::WASM_EXIT;
    case CodeKind::JS_TO_WASM_FUNCTION:
      return StackFrame::JS_TO_WASM;
    case CodeKind::WASM_TO_JS_FUNCTION:
      return StackFrame::WASM_TO_JS;
    case CodeKind::C_WASM_ENTRY:
      return StackFrame::C_WASM_ENTRY;
#endif  // V8_ENABLE_WEBASSEMBLY
    default:
      UNIMPLEMENTED();
      return StackFrame::NONE;
  }
}

void OptimizedCompilationInfo::SetCode(Handle<Code> code) {
  DCHECK_EQ(code->kind(), code_kind());
  code_ = code;
}

#if V8_ENABLE_WEBASSEMBLY
void OptimizedCompilationInfo::SetWasmCompilationResult(
    std::unique_ptr<wasm::WasmCompilationResult> wasm_compilation_result) {
  wasm_compilation_result_ = std::move(wasm_compilation_result);
}

std::unique_ptr<wasm::WasmCompilationResult>
OptimizedCompilationInfo::ReleaseWasmCompilationResult() {
  return std::move(wasm_compilation_result_);
}
#endif  // V8_ENABLE_WEBASSEMBLY

bool OptimizedCompilationInfo::has_context() const {
  return !closure().is_null();
}

Context OptimizedCompilationInfo::context() const {
  DCHECK(has_context());
  return closure()->context();
}

bool OptimizedCompilationInfo::has_native_context() const {
  return !closure().is_null() && !closure()->native_context().is_null();
}

NativeContext OptimizedCompilationInfo::native_context() const {
  DCHECK(has_native_context());
  return closure()->native_context();
}

bool OptimizedCompilationInfo::has_global_object() const {
  return has_native_context();
}

JSGlobalObject OptimizedCompilationInfo::global_object() const {
  DCHECK(has_global_object());
  return native_context().global_object();
}

int OptimizedCompilationInfo::AddInlinedFunction(
    Handle<SharedFunctionInfo> inlined_function,
    Handle<BytecodeArray> inlined_bytecode, SourcePosition pos) {
  int id = static_cast<int>(inlined_functions_.size());
  inlined_functions_.push_back(
      InlinedFunctionHolder(inlined_function, inlined_bytecode, pos));
  return id;
}

void OptimizedCompilationInfo::SetTracingFlags(bool passes_filter) {
  if (!passes_filter) return;
  if (FLAG_trace_turbo) set_trace_turbo_json();
  if (FLAG_trace_turbo_graph) set_trace_turbo_graph();
  if (FLAG_trace_turbo_scheduled) set_trace_turbo_scheduled();
  if (FLAG_trace_turbo_alloc) set_trace_turbo_allocation();
  if (FLAG_trace_heap_broker) set_trace_heap_broker();
}

OptimizedCompilationInfo::InlinedFunctionHolder::InlinedFunctionHolder(
    Handle<SharedFunctionInfo> inlined_shared_info,
    Handle<BytecodeArray> inlined_bytecode, SourcePosition pos)
    : shared_info(inlined_shared_info), bytecode_array(inlined_bytecode) {
  position.position = pos;
  // initialized when generating the deoptimization literals
  position.inlined_function_id = DeoptimizationData::kNotInlinedIndex;
}

}  // namespace internal
}  // namespace v8
