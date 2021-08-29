// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/wasm/function-compiler.h"

#include "src/base/platform/time.h"
#include "src/base/strings.h"
#include "src/codegen/compiler.h"
#include "src/codegen/macro-assembler-inl.h"
#include "src/codegen/optimized-compilation-info.h"
#include "src/compiler/wasm-compiler.h"
#include "src/diagnostics/code-tracer.h"
#include "src/logging/counters.h"
#include "src/logging/log.h"
#include "src/utils/ostreams.h"
#include "src/wasm/baseline/liftoff-compiler.h"
#include "src/wasm/wasm-code-manager.h"
#include "src/wasm/wasm-debug.h"
#include "src/wasm/wasm-engine.h"

namespace v8 {
namespace internal {
namespace wasm {

// static
ExecutionTier WasmCompilationUnit::GetBaselineExecutionTier(
    const WasmModule* module) {
  // Liftoff does not support the special asm.js opcodes, thus always compile
  // asm.js modules with TurboFan.
  if (is_asmjs_module(module)) return ExecutionTier::kTurbofan;
  return FLAG_liftoff ? ExecutionTier::kLiftoff : ExecutionTier::kTurbofan;
}

WasmCompilationResult WasmCompilationUnit::ExecuteCompilation(
    CompilationEnv* env, const WireBytesStorage* wire_bytes_storage,
    Counters* counters, WasmFeatures* detected) {
  WasmCompilationResult result;
  if (func_index_ < static_cast<int>(env->module->num_imported_functions)) {
    result = ExecuteImportWrapperCompilation(env);
  } else {
    result =
        ExecuteFunctionCompilation(env, wire_bytes_storage, counters, detected);
  }

  if (result.succeeded() && counters) {
    counters->wasm_generated_code_size()->Increment(
        result.code_desc.instr_size);
    counters->wasm_reloc_size()->Increment(result.code_desc.reloc_size);
  }

  result.func_index = func_index_;
  result.requested_tier = tier_;

  return result;
}

WasmCompilationResult WasmCompilationUnit::ExecuteImportWrapperCompilation(
    CompilationEnv* env) {
  const FunctionSig* sig = env->module->functions[func_index_].sig;
  // Assume the wrapper is going to be a JS function with matching arity at
  // instantiation time.
  auto kind = compiler::kDefaultImportCallKind;
  bool source_positions = is_asmjs_module(env->module);
  WasmCompilationResult result = compiler::CompileWasmImportCallWrapper(
      env, kind, sig, source_positions,
      static_cast<int>(sig->parameter_count()));
  return result;
}

WasmCompilationResult WasmCompilationUnit::ExecuteFunctionCompilation(
    CompilationEnv* env, const WireBytesStorage* wire_bytes_storage,
    Counters* counters, WasmFeatures* detected) {
  auto* func = &env->module->functions[func_index_];
  base::Vector<const uint8_t> code = wire_bytes_storage->GetCode(func->code);
  wasm::FunctionBody func_body{func->sig, func->code.offset(), code.begin(),
                               code.end()};

  base::Optional<TimedHistogramScope> wasm_compile_function_time_scope;
  if (counters) {
    if ((func_body.end - func_body.start) >= 100 * KB) {
      auto huge_size_histogram = SELECT_WASM_COUNTER(
          counters, env->module->origin, wasm, huge_function_size_bytes);
      huge_size_histogram->AddSample(
          static_cast<int>(func_body.end - func_body.start));
    }
    auto timed_histogram = SELECT_WASM_COUNTER(counters, env->module->origin,
                                               wasm_compile, function_time);
    wasm_compile_function_time_scope.emplace(timed_histogram);
  }

  if (FLAG_trace_wasm_compiler) {
    PrintF("Compiling wasm function %d with %s\n", func_index_,
           ExecutionTierToString(tier_));
  }

  WasmCompilationResult result;

  switch (tier_) {
    case ExecutionTier::kNone:
      UNREACHABLE();

    case ExecutionTier::kLiftoff:
      // The --wasm-tier-mask-for-testing flag can force functions to be
      // compiled with TurboFan, and the --wasm-debug-mask-for-testing can force
      // them to be compiled for debugging, see documentation.
      if (V8_LIKELY(FLAG_wasm_tier_mask_for_testing == 0) ||
          func_index_ >= 32 ||
          ((FLAG_wasm_tier_mask_for_testing & (1 << func_index_)) == 0)) {
        if (V8_LIKELY(func_index_ >= 32 || (FLAG_wasm_debug_mask_for_testing &
                                            (1 << func_index_)) == 0)) {
          result = ExecuteLiftoffCompilation(
              env, func_body, func_index_, for_debugging_, counters, detected);
        } else {
          // We don't use the debug side table, we only pass it to cover
          // different code paths in Liftoff for testing.
          std::unique_ptr<DebugSideTable> debug_sidetable;
          result = ExecuteLiftoffCompilation(env, func_body, func_index_,
                                             kForDebugging, counters, detected,
                                             {}, &debug_sidetable);
        }
        if (result.succeeded()) break;
      }

      // If Liftoff failed, fall back to turbofan.
      // TODO(wasm): We could actually stop or remove the tiering unit for this
      // function to avoid compiling it twice with TurboFan.
      V8_FALLTHROUGH;

    case ExecutionTier::kTurbofan:
      result = compiler::ExecuteTurbofanWasmCompilation(
          env, func_body, func_index_, counters, detected);
      result.for_debugging = for_debugging_;
      break;
  }

  return result;
}

namespace {
bool must_record_function_compilation(Isolate* isolate) {
  return isolate->logger()->is_listening_to_code_events() ||
         isolate->is_profiling();
}

PRINTF_FORMAT(3, 4)
void RecordWasmHeapStubCompilation(Isolate* isolate, Handle<Code> code,
                                   const char* format, ...) {
  DCHECK(must_record_function_compilation(isolate));

  base::ScopedVector<char> buffer(128);
  va_list arguments;
  va_start(arguments, format);
  int len = base::VSNPrintF(buffer, format, arguments);
  CHECK_LT(0, len);
  va_end(arguments);
  Handle<String> name_str =
      isolate->factory()->NewStringFromAsciiChecked(buffer.begin());
  PROFILE(isolate, CodeCreateEvent(CodeEventListener::STUB_TAG,
                                   Handle<AbstractCode>::cast(code), name_str));
}
}  // namespace

// static
void WasmCompilationUnit::CompileWasmFunction(Isolate* isolate,
                                              NativeModule* native_module,
                                              WasmFeatures* detected,
                                              const WasmFunction* function,
                                              ExecutionTier tier) {
  ModuleWireBytes wire_bytes(native_module->wire_bytes());
  FunctionBody function_body{function->sig, function->code.offset(),
                             wire_bytes.start() + function->code.offset(),
                             wire_bytes.start() + function->code.end_offset()};

  DCHECK_LE(native_module->num_imported_functions(), function->func_index);
  DCHECK_LT(function->func_index, native_module->num_functions());
  WasmCompilationUnit unit(function->func_index, tier, kNoDebugging);
  CompilationEnv env = native_module->CreateCompilationEnv();
  WasmCompilationResult result = unit.ExecuteCompilation(
      &env, native_module->compilation_state()->GetWireBytesStorage().get(),
      isolate->counters(), detected);
  if (result.succeeded()) {
    WasmCodeRefScope code_ref_scope;
    native_module->PublishCode(
        native_module->AddCompiledCode(std::move(result)));
  } else {
    native_module->compilation_state()->SetError();
  }
}

namespace {
bool UseGenericWrapper(const FunctionSig* sig) {
#if V8_TARGET_ARCH_X64
  if (sig->returns().size() > 1) {
    return false;
  }
  if (sig->returns().size() == 1 && sig->GetReturn(0).kind() != kI32 &&
      sig->GetReturn(0).kind() != kI64 && sig->GetReturn(0).kind() != kF32 &&
      sig->GetReturn(0).kind() != kF64) {
    return false;
  }
  for (ValueType type : sig->parameters()) {
    if (type.kind() != kI32 && type.kind() != kI64 && type.kind() != kF32 &&
        type.kind() != kF64) {
      return false;
    }
  }
  return FLAG_wasm_generic_wrapper;
#else
  return false;
#endif
}
}  // namespace

JSToWasmWrapperCompilationUnit::JSToWasmWrapperCompilationUnit(
    Isolate* isolate, const FunctionSig* sig, const WasmModule* module,
    bool is_import, const WasmFeatures& enabled_features,
    AllowGeneric allow_generic)
    : isolate_(isolate),
      is_import_(is_import),
      sig_(sig),
      use_generic_wrapper_(allow_generic && UseGenericWrapper(sig) &&
                           !is_import),
      job_(use_generic_wrapper_
               ? nullptr
               : compiler::NewJSToWasmCompilationJob(
                     isolate, sig, module, is_import, enabled_features)) {}

JSToWasmWrapperCompilationUnit::~JSToWasmWrapperCompilationUnit() = default;

void JSToWasmWrapperCompilationUnit::Execute() {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.wasm.detailed"),
               "wasm.CompileJSToWasmWrapper");
  if (!use_generic_wrapper_) {
    CompilationJob::Status status = job_->ExecuteJob(nullptr);
    CHECK_EQ(status, CompilationJob::SUCCEEDED);
  }
}

Handle<Code> JSToWasmWrapperCompilationUnit::Finalize() {
  Handle<Code> code;
  if (use_generic_wrapper_) {
    code = isolate_->builtins()->code_handle(Builtin::kGenericJSToWasmWrapper);
  } else {
    CompilationJob::Status status = job_->FinalizeJob(isolate_);
    CHECK_EQ(status, CompilationJob::SUCCEEDED);
    code = job_->compilation_info()->code();
  }
  if (!use_generic_wrapper_ && must_record_function_compilation(isolate_)) {
    RecordWasmHeapStubCompilation(
        isolate_, code, "%s", job_->compilation_info()->GetDebugName().get());
  }
  return code;
}

// static
Handle<Code> JSToWasmWrapperCompilationUnit::CompileJSToWasmWrapper(
    Isolate* isolate, const FunctionSig* sig, const WasmModule* module,
    bool is_import) {
  // Run the compilation unit synchronously.
  WasmFeatures enabled_features = WasmFeatures::FromIsolate(isolate);
  JSToWasmWrapperCompilationUnit unit(isolate, sig, module, is_import,
                                      enabled_features, kAllowGeneric);
  unit.Execute();
  return unit.Finalize();
}

// static
Handle<Code> JSToWasmWrapperCompilationUnit::CompileSpecificJSToWasmWrapper(
    Isolate* isolate, const FunctionSig* sig, const WasmModule* module) {
  // Run the compilation unit synchronously.
  const bool is_import = false;
  WasmFeatures enabled_features = WasmFeatures::FromIsolate(isolate);
  JSToWasmWrapperCompilationUnit unit(isolate, sig, module, is_import,
                                      enabled_features, kDontAllowGeneric);
  unit.Execute();
  return unit.Finalize();
}

}  // namespace wasm
}  // namespace internal
}  // namespace v8
