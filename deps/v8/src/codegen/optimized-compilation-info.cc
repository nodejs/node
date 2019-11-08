// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/codegen/optimized-compilation-info.h"

#include "src/api/api.h"
#include "src/codegen/source-position.h"
#include "src/debug/debug.h"
#include "src/execution/isolate.h"
#include "src/objects/objects-inl.h"
#include "src/objects/shared-function-info.h"
#include "src/tracing/trace-event.h"
#include "src/tracing/traced-value.h"
#include "src/wasm/function-compiler.h"

namespace v8 {
namespace internal {

OptimizedCompilationInfo::OptimizedCompilationInfo(
    Zone* zone, Isolate* isolate, Handle<SharedFunctionInfo> shared,
    Handle<JSFunction> closure)
    : OptimizedCompilationInfo(Code::OPTIMIZED_FUNCTION, zone) {
  DCHECK_EQ(*shared, closure->shared());
  DCHECK(shared->is_compiled());
  bytecode_array_ = handle(shared->GetBytecodeArray(), isolate);
  shared_info_ = shared;
  closure_ = closure;
  optimization_id_ = isolate->NextOptimizationId();

  // Collect source positions for optimized code when profiling or if debugger
  // is active, to be able to get more precise source positions at the price of
  // more memory consumption.
  if (isolate->NeedsDetailedOptimizedCodeLineInfo()) {
    MarkAsSourcePositionsEnabled();
  }

  SetTracingFlags(shared->PassesFilter(FLAG_trace_turbo_filter));
}

OptimizedCompilationInfo::OptimizedCompilationInfo(
    Vector<const char> debug_name, Zone* zone, Code::Kind code_kind)
    : OptimizedCompilationInfo(code_kind, zone) {
  debug_name_ = debug_name;

  SetTracingFlags(
      PassesFilter(debug_name, CStrVector(FLAG_trace_turbo_filter)));
}

OptimizedCompilationInfo::OptimizedCompilationInfo(Code::Kind code_kind,
                                                   Zone* zone)
    : code_kind_(code_kind), zone_(zone) {
  ConfigureFlags();
}

void OptimizedCompilationInfo::ConfigureFlags() {
  if (FLAG_untrusted_code_mitigations) SetFlag(kUntrustedCodeMitigations);

  switch (code_kind_) {
    case Code::OPTIMIZED_FUNCTION:
      SetFlag(kCalledWithCodeStartRegister);
      SetFlag(kSwitchJumpTableEnabled);
      if (FLAG_function_context_specialization) {
        MarkAsFunctionContextSpecializing();
      }
      if (FLAG_turbo_splitting) {
        MarkAsSplittingEnabled();
      }
      if (FLAG_untrusted_code_mitigations) {
        MarkAsPoisoningRegisterArguments();
      }
      if (FLAG_analyze_environment_liveness) {
        // TODO(yangguo): Disable this in case of debugging for crbug.com/826613
        MarkAsAnalyzeEnvironmentLiveness();
      }
      break;
    case Code::BYTECODE_HANDLER:
      SetFlag(kCalledWithCodeStartRegister);
      if (FLAG_turbo_splitting) {
        MarkAsSplittingEnabled();
      }
      break;
    case Code::BUILTIN:
    case Code::STUB:
      if (FLAG_turbo_splitting) {
        MarkAsSplittingEnabled();
      }
#if ENABLE_GDB_JIT_INTERFACE && DEBUG
      MarkAsSourcePositionsEnabled();
#endif  // ENABLE_GDB_JIT_INTERFACE && DEBUG
      break;
    case Code::WASM_FUNCTION:
    case Code::WASM_TO_CAPI_FUNCTION:
      SetFlag(kSwitchJumpTableEnabled);
      break;
    default:
      break;
  }

  if (FLAG_turbo_control_flow_aware_allocation) {
    MarkAsTurboControlFlowAwareAllocation();
  } else {
    MarkAsTurboPreprocessRanges();
  }
}

OptimizedCompilationInfo::~OptimizedCompilationInfo() {
  if (GetFlag(kDisableFutureOptimization) && has_shared_info()) {
    shared_info()->DisableOptimization(bailout_reason());
  }
}

void OptimizedCompilationInfo::set_deferred_handles(
    std::unique_ptr<DeferredHandles> deferred_handles) {
  DCHECK_NULL(deferred_handles_);
  deferred_handles_ = std::move(deferred_handles);
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
    TRACE_EVENT_INSTANT2(TRACE_DISABLED_BY_DEFAULT("v8.compile"),
                         "V8.AbortOptimization", TRACE_EVENT_SCOPE_THREAD,
                         "reason", GetBailoutReason(reason), "function",
                         shared_info()->TraceIDRef());
    bailout_reason_ = reason;
  }
  SetFlag(kDisableFutureOptimization);
}

void OptimizedCompilationInfo::RetryOptimization(BailoutReason reason) {
  DCHECK_NE(reason, BailoutReason::kNoReason);
  if (GetFlag(kDisableFutureOptimization)) return;
  TRACE_EVENT_INSTANT2(TRACE_DISABLED_BY_DEFAULT("v8.compile"),
                       "V8.RetryOptimization", TRACE_EVENT_SCOPE_THREAD,
                       "reason", GetBailoutReason(reason), "function",
                       shared_info()->TraceIDRef());
  bailout_reason_ = reason;
}

std::unique_ptr<char[]> OptimizedCompilationInfo::GetDebugName() const {
  if (!shared_info().is_null()) {
    return shared_info()->DebugName().ToCString();
  }
  Vector<const char> name_vec = debug_name_;
  if (name_vec.empty()) name_vec = ArrayVector("unknown");
  std::unique_ptr<char[]> name(new char[name_vec.length() + 1]);
  memcpy(name.get(), name_vec.begin(), name_vec.length());
  name[name_vec.length()] = '\0';
  return name;
}

StackFrame::Type OptimizedCompilationInfo::GetOutputStackFrameType() const {
  switch (code_kind()) {
    case Code::STUB:
    case Code::BYTECODE_HANDLER:
    case Code::BUILTIN:
      return StackFrame::STUB;
    case Code::WASM_FUNCTION:
      return StackFrame::WASM_COMPILED;
    case Code::WASM_TO_CAPI_FUNCTION:
      return StackFrame::WASM_EXIT;
    case Code::JS_TO_WASM_FUNCTION:
      return StackFrame::JS_TO_WASM;
    case Code::WASM_TO_JS_FUNCTION:
      return StackFrame::WASM_TO_JS;
    case Code::WASM_INTERPRETER_ENTRY:
      return StackFrame::WASM_INTERPRETER_ENTRY;
    case Code::C_WASM_ENTRY:
      return StackFrame::C_WASM_ENTRY;
    default:
      UNIMPLEMENTED();
      return StackFrame::NONE;
  }
}

void OptimizedCompilationInfo::SetWasmCompilationResult(
    std::unique_ptr<wasm::WasmCompilationResult> wasm_compilation_result) {
  wasm_compilation_result_ = std::move(wasm_compilation_result);
}

std::unique_ptr<wasm::WasmCompilationResult>
OptimizedCompilationInfo::ReleaseWasmCompilationResult() {
  return std::move(wasm_compilation_result_);
}

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
  if (FLAG_trace_turbo) SetFlag(kTraceTurboJson);
  if (FLAG_trace_turbo_graph) SetFlag(kTraceTurboGraph);
  if (FLAG_trace_turbo_scheduled) SetFlag(kTraceTurboScheduled);
  if (FLAG_trace_turbo_alloc) SetFlag(kTraceTurboAllocation);
  if (FLAG_trace_heap_broker) SetFlag(kTraceHeapBroker);
}

OptimizedCompilationInfo::InlinedFunctionHolder::InlinedFunctionHolder(
    Handle<SharedFunctionInfo> inlined_shared_info,
    Handle<BytecodeArray> inlined_bytecode, SourcePosition pos)
    : shared_info(inlined_shared_info), bytecode_array(inlined_bytecode) {
  position.position = pos;
  // initialized when generating the deoptimization literals
  position.inlined_function_id = DeoptimizationData::kNotInlinedIndex;
}

std::unique_ptr<v8::tracing::TracedValue>
OptimizedCompilationInfo::ToTracedValue() {
  auto value = v8::tracing::TracedValue::Create();
  value->SetBoolean("osr", is_osr());
  value->SetBoolean("functionContextSpecialized",
                    is_function_context_specializing());
  if (has_shared_info()) {
    value->SetValue("function", shared_info()->TraceIDRef());
  }
  if (bailout_reason() != BailoutReason::kNoReason) {
    value->SetString("bailoutReason", GetBailoutReason(bailout_reason()));
    value->SetBoolean("disableFutureOptimization",
                      is_disable_future_optimization());
  } else {
    value->SetInteger("optimizationId", optimization_id());
    value->BeginArray("inlinedFunctions");
    for (auto const& inlined_function : inlined_functions()) {
      value->BeginDictionary();
      value->SetValue("function", inlined_function.shared_info->TraceIDRef());
      // TODO(bmeurer): Also include the source position from the
      // {inlined_function} here as dedicated "sourcePosition" field.
      value->EndDictionary();
    }
    value->EndArray();
  }
  return value;
}

}  // namespace internal
}  // namespace v8
