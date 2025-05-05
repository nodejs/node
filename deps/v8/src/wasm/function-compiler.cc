// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/wasm/function-compiler.h"

#include <optional>

#include "src/base/fpu.h"
#include "src/codegen/compiler.h"
#include "src/codegen/optimized-compilation-info.h"
#include "src/compiler/turboshaft/wasm-turboshaft-compiler.h"
#include "src/compiler/wasm-compiler.h"
#include "src/handles/handles-inl.h"
#include "src/logging/counters-scopes.h"
#include "src/logging/log.h"
#include "src/objects/code-inl.h"
#include "src/wasm/baseline/liftoff-compiler.h"
#include "src/wasm/compilation-environment-inl.h"
#include "src/wasm/turboshaft-graph-interface.h"
#include "src/wasm/wasm-code-manager.h"
#include "src/wasm/wasm-debug.h"
#include "src/wasm/wasm-engine.h"

namespace v8::internal::wasm {

WasmCompilationResult WasmCompilationUnit::ExecuteCompilation(
    CompilationEnv* env, const WireBytesStorage* wire_bytes_storage,
    Counters* counters, WasmDetectedFeatures* detected) {
  DCHECK_GE(func_index_, static_cast<int>(env->module->num_imported_functions));
  const WasmFunction* func = &env->module->functions[func_index_];
  base::Vector<const uint8_t> code = wire_bytes_storage->GetCode(func->code);
  bool is_shared = env->module->type(func->sig_index).is_shared;
  wasm::FunctionBody func_body{func->sig, func->code.offset(), code.begin(),
                               code.end(), is_shared};

  std::optional<TimedHistogramScope> wasm_compile_function_time_scope;
  std::optional<TimedHistogramScope> wasm_compile_huge_function_time_scope;
  if (counters && base::TimeTicks::IsHighResolution()) {
    if (func_body.end - func_body.start >= 100 * KB) {
      auto huge_size_histogram = SELECT_WASM_COUNTER(
          counters, env->module->origin, wasm, huge_function_size_bytes);
      huge_size_histogram->AddSample(
          static_cast<int>(func_body.end - func_body.start));
      wasm_compile_huge_function_time_scope.emplace(
          counters->wasm_compile_huge_function_time());
    }
    auto timed_histogram = SELECT_WASM_COUNTER(counters, env->module->origin,
                                               wasm_compile, function_time);
    wasm_compile_function_time_scope.emplace(timed_histogram);
  }

  // Before executing compilation, make sure that the function was validated.
  // Both Liftoff and TurboFan compilation do not perform validation, so can
  // only run on valid functions.
  if (V8_UNLIKELY(!env->module->function_was_validated(func_index_))) {
    // This code path can only be reached in
    // - eager compilation mode,
    // - with lazy validation,
    // - with PGO (which compiles some functions eagerly), or
    // - with compilation hints (which also compiles some functions eagerly).
    DCHECK(!v8_flags.wasm_lazy_compilation || v8_flags.wasm_lazy_validation ||
           v8_flags.experimental_wasm_pgo_from_file ||
           v8_flags.experimental_wasm_compilation_hints);
    Zone validation_zone{GetWasmEngine()->allocator(), ZONE_NAME};
    if (ValidateFunctionBody(&validation_zone, env->enabled_features,
                             env->module, detected, func_body)
            .failed()) {
      return {};
    }
    env->module->set_function_validated(func_index_);
  }

  if (v8_flags.trace_wasm_compiler) {
    PrintF("Compiling wasm function %d with %s\n", func_index_,
           ExecutionTierToString(tier_));
  }

  WasmCompilationResult result;
  int declared_index = declared_function_index(env->module, func_index_);

  switch (tier_) {
    case ExecutionTier::kNone:
#if V8_ENABLE_DRUMBRAKE
    case ExecutionTier::kInterpreter:
#endif  // V8_ENABLE_DRUMBRAKE
      UNREACHABLE();

    case ExecutionTier::kLiftoff: {
      // The --wasm-tier-mask-for-testing flag can force functions to be
      // compiled with TurboFan, and the --wasm-debug-mask-for-testing can force
      // them to be compiled for debugging, see documentation.
      bool try_liftoff = true;
      if (V8_UNLIKELY(v8_flags.wasm_tier_mask_for_testing != 0)) {
        bool must_use_liftoff =
            v8_flags.liftoff_only ||
            for_debugging_ != ForDebugging::kNotForDebugging;
        bool tiering_requested =
            declared_index < 32 &&
            (v8_flags.wasm_tier_mask_for_testing & (1 << declared_index));
        if (!must_use_liftoff && tiering_requested) try_liftoff = false;
      }

      if (V8_LIKELY(try_liftoff)) {
        auto options = LiftoffOptions{}
                           .set_func_index(func_index_)
                           .set_for_debugging(for_debugging_)
                           .set_counters(counters)
                           .set_detected_features(detected);
        // We do not use the debug side table, we only (optionally) pass it to
        // cover different code paths in Liftoff for testing.
        std::unique_ptr<DebugSideTable> unused_debug_sidetable;
        if (V8_UNLIKELY(declared_index < 32 &&
                        (v8_flags.wasm_debug_mask_for_testing &
                         (1 << declared_index)) != 0)) {
          options.set_debug_sidetable(&unused_debug_sidetable);
          if (!for_debugging_) options.set_for_debugging(kForDebugging);
        }
        result = ExecuteLiftoffCompilation(env, func_body, options);
        if (result.succeeded()) break;
      }

      // If --liftoff-only, do not fall back to turbofan, even if compilation
      // failed.
      if (v8_flags.liftoff_only) break;

      // If Liftoff failed, fall back to TurboFan.
      // TODO(wasm): We could actually stop or remove the tiering unit for this
      // function to avoid compiling it twice with TurboFan.
      [[fallthrough]];
    }
    case ExecutionTier::kTurbofan: {
      compiler::WasmCompilationData data(func_body);
      data.func_index = func_index_;
      data.wire_bytes_storage = wire_bytes_storage;
      result = compiler::turboshaft::ExecuteTurboshaftWasmCompilation(
          env, data, detected, counters);
      // In exceptional cases it can happen that compilation requests for
      // debugging end up being executed by Turbofan, e.g. if Liftoff bails out
      // because of unsupported features or the --wasm-tier-mask-for-testing is
      // set. In that case we set the for_debugging field for the TurboFan
      // result to match the requested for_debugging_.
      result.for_debugging = for_debugging_;
      break;
    }
  }

  DCHECK(result.succeeded());
  if (counters) {
    counters->wasm_generated_code_size()->Increment(
        result.code_desc.instr_size);
    counters->wasm_reloc_size()->Increment(result.code_desc.reloc_size);
    counters->wasm_deopt_data_size()->Increment(
        static_cast<int>(result.deopt_data.size()));
  }

  result.func_index = func_index_;

  return result;
}

// static
void WasmCompilationUnit::CompileWasmFunction(Counters* counters,
                                              NativeModule* native_module,
                                              WasmDetectedFeatures* detected,
                                              const WasmFunction* function,
                                              ExecutionTier tier) {
  ModuleWireBytes wire_bytes(native_module->wire_bytes());
  bool is_shared = native_module->module()->type(function->sig_index).is_shared;
  FunctionBody function_body{function->sig, function->code.offset(),
                             wire_bytes.start() + function->code.offset(),
                             wire_bytes.start() + function->code.end_offset(),
                             is_shared};

  DCHECK_LE(native_module->num_imported_functions(), function->func_index);
  DCHECK_LT(function->func_index, native_module->num_functions());
  WasmCompilationUnit unit(function->func_index, tier, kNotForDebugging);
  CompilationEnv env = CompilationEnv::ForModule(native_module);
  base::FlushDenormalsScope disable_denormals(
      tier == ExecutionTier::kTurbofan &&
      native_module->compile_imports().contains(
          CompileTimeImport::kDisableDenormalFloats));
  WasmCompilationResult result = unit.ExecuteCompilation(
      &env, native_module->compilation_state()->GetWireBytesStorage().get(),
      counters, detected);
  if (result.succeeded()) {
    WasmCodeRefScope code_ref_scope;
    native_module->PublishCode(native_module->AddCompiledCode(result));
  } else {
    native_module->compilation_state()->SetError();
  }
}

JSToWasmWrapperCompilationUnit::JSToWasmWrapperCompilationUnit(
    Isolate* isolate, const CanonicalSig* sig, CanonicalTypeIndex sig_index)
    : isolate_(isolate),
      sig_(sig),
      sig_index_(sig_index),
      job_(v8_flags.wasm_jitless
               ? nullptr
               : compiler::NewJSToWasmCompilationJob(isolate, sig)) {
  if (!v8_flags.wasm_jitless) {
    OptimizedCompilationInfo* info =
        static_cast<compiler::turboshaft::TurboshaftCompilationJob*>(job_.get())
            ->compilation_info();
    if (info->trace_turbo_graph()) {
      // Make sure that code tracer is initialized on the main thread if tracing
      // is enabled.
      isolate->GetCodeTracer();
    }
  }
}

JSToWasmWrapperCompilationUnit::~JSToWasmWrapperCompilationUnit() = default;

void JSToWasmWrapperCompilationUnit::Execute() {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.wasm.detailed"),
               "wasm.CompileJSToWasmWrapper");
  if (!v8_flags.wasm_jitless) {
    CompilationJob::Status status = job_->ExecuteJob(nullptr);
    CHECK_EQ(status, CompilationJob::SUCCEEDED);
  }
}

DirectHandle<Code> JSToWasmWrapperCompilationUnit::Finalize() {
#if V8_ENABLE_DRUMBRAKE
  if (v8_flags.wasm_jitless) {
    return isolate_->builtins()->code_handle(
        Builtin::kGenericJSToWasmInterpreterWrapper);
  }
#endif  // V8_ENABLE_DRUMBRAKE

  CompilationJob::Status status = job_->FinalizeJob(isolate_);
  CHECK_EQ(status, CompilationJob::SUCCEEDED);
  OptimizedCompilationInfo* info =
      static_cast<compiler::turboshaft::TurboshaftCompilationJob*>(job_.get())
          ->compilation_info();
  DirectHandle<Code> code = info->code();
  if (isolate_->IsLoggingCodeCreation()) {
    DirectHandle<String> name = isolate_->factory()->NewStringFromAsciiChecked(
        info->GetDebugName().get());
    PROFILE(isolate_, CodeCreateEvent(LogEventListener::CodeTag::kStub,
                                      Cast<AbstractCode>(code), name));
  }
  // We should always have checked the cache before compiling a wrapper.
  Tagged<WeakFixedArray> cache = isolate_->heap()->js_to_wasm_wrappers();
  DCHECK(cache->get(sig_index_.index).IsCleared());
  // Install the compiled wrapper in the cache now.
  cache->set(sig_index_.index, MakeWeak(code->wrapper()));
  Counters* counters = isolate_->counters();
  counters->wasm_generated_code_size()->Increment(code->body_size());
  counters->wasm_reloc_size()->Increment(code->relocation_size());
  counters->wasm_compiled_export_wrapper()->Increment(1);
  return code;
}

// static
DirectHandle<Code> JSToWasmWrapperCompilationUnit::CompileJSToWasmWrapper(
    Isolate* isolate, const CanonicalSig* sig, CanonicalTypeIndex sig_index) {
  // Run the compilation unit synchronously.
  JSToWasmWrapperCompilationUnit unit(isolate, sig, sig_index);
  unit.Execute();
  return unit.Finalize();
}

}  // namespace v8::internal::wasm
