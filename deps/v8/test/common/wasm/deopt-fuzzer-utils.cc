// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "test/common/wasm/deopt-fuzzer-utils.h"

#include <cstring>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <variant>
#include <vector>

#include "include/v8-exception.h"
#include "src/base/unique-array.h"
#include "src/codegen/cpu-features.h"
#include "src/execution/isolate.h"
#include "src/heap/heap.h"
#include "src/utils/ostreams.h"
#include "src/wasm/fuzzing/random-module-generation.h"
#include "src/wasm/module-compiler.h"
#include "src/wasm/wasm-engine.h"
#include "src/wasm/wasm-module.h"
#include "src/wasm/wasm-objects-inl.h"
#include "src/zone/accounting-allocator.h"
#include "src/zone/zone.h"
#include "test/common/flag-utils.h"
#include "test/common/wasm/fuzzer-common.h"
#include "test/common/wasm/wasm-module-runner.h"

namespace v8::internal::wasm::fuzzing {

namespace {

// The result of calling the generated "main" function for one callee: either
// the returned value or the exception message.
using ExecutionResult = std::variant<int, std::string /*exception*/>;

std::ostream& operator<<(std::ostream& out, const ExecutionResult& result) {
  std::visit([&out](auto&& val) { out << val; }, result);
  return out;
}

// Terminates execution once the heap limit is reached, so modules that need
// too much memory can be discarded.
class NearHeapLimitCallbackScope {
 public:
  explicit NearHeapLimitCallbackScope(Isolate* isolate);
  ~NearHeapLimitCallbackScope();

  bool heap_limit_reached() const { return heap_limit_reached_; }

 private:
  static size_t Callback(void* raw_data, size_t current_limit,
                         size_t initial_limit);

  Isolate* isolate_;
  bool heap_limit_reached_ = false;
  size_t initial_limit_ = 0;
};

// Puts the isolate in debugging mode for the lifetime of the scope.
class EnterDebuggingScope {
 public:
  explicit EnterDebuggingScope(Isolate* isolate);
  ~EnterDebuggingScope();

 private:
  Isolate* isolate_;
};

NearHeapLimitCallbackScope::NearHeapLimitCallbackScope(Isolate* isolate)
    : isolate_(isolate) {
  isolate_->heap()->AddNearHeapLimitCallback(Callback, this);
}

NearHeapLimitCallbackScope::~NearHeapLimitCallbackScope() {
  isolate_->heap()->RemoveNearHeapLimitCallback(Callback, initial_limit_);
}

// static
size_t NearHeapLimitCallbackScope::Callback(void* raw_data,
                                            size_t current_limit,
                                            size_t initial_limit) {
  NearHeapLimitCallbackScope* data =
      reinterpret_cast<NearHeapLimitCallbackScope*>(raw_data);
  data->heap_limit_reached_ = true;
  data->isolate_->TerminateExecution();
  data->initial_limit_ = initial_limit;
  // Return a slightly raised limit, just to make it to the next
  // interrupt check point, where execution will terminate.
  return initial_limit * 1.25;
}

EnterDebuggingScope::EnterDebuggingScope(Isolate* isolate) : isolate_(isolate) {
  GetWasmEngine()->EnterDebuggingForIsolate(isolate_);
}

EnterDebuggingScope::~EnterDebuggingScope() {
  GetWasmEngine()->LeaveDebuggingForIsolate(isolate_);
}

std::vector<ExecutionResult> PerformReferenceRun(
    const std::vector<std::string>& callees, ModuleWireBytes wire_bytes,
    WasmEnabledFeatures enabled_features, bool valid, Isolate* isolate) {
  std::vector<ExecutionResult> results;
  FlagScope<bool> eager_compile(&v8_flags.wasm_lazy_compilation, false);
  // Reference runs use extra compile settings (like non-determinism detection),
  // which would be removed and replaced with a new liftoff function without
  // these options.
  FlagScope<bool> no_liftoff_code_flushing(&v8_flags.flush_liftoff_code, false);
  ErrorThrower thrower(isolate, "WasmFuzzerSyncCompileReference");

  int32_t max_steps = kDefaultMaxFuzzerExecutedInstructions;

  // We aren't really debugging but this will prevent tier-up and other
  // "dynamic" behavior that we do not want to trigger during reference
  // execution. This also aligns well with the reference compilation compiling
  // with the kForDebugging liftoff option.
  EnterDebuggingScope debugging_scope(isolate);

  DirectHandle<WasmModuleObject> module_object;
  if (!CompileReferenceModule(isolate, wire_bytes.module_bytes(), &max_steps)
           .ToHandle(&module_object)) {
    return {};
  }

  thrower.Reset();
  CHECK(!isolate->has_exception());

  DirectHandle<WasmInstanceObject> instance;
  if (!GetWasmEngine()
           ->SyncInstantiate(isolate, &thrower, module_object, {})
           .ToHandle(&instance)) {
    CHECK(thrower.error());
    // The only reason to fail the second instantiation should be OOM. This can
    // happen e.g. for memories with a very big initial size especially on 32
    // bit platforms.
    CHECK(strstr(thrower.error_msg(), "Out of memory"));
    return {};
  }

  NearHeapLimitCallbackScope near_heap_limit(isolate);
  for (uint32_t i = 0; i < callees.size(); ++i) {
    // Before execution, there should be no dangling nondeterminism registered
    // on the engine.
    DCHECK(!WasmEngine::had_nondeterminism());

    DirectHandle<Object> arguments[] = {
        direct_handle(Smi::FromInt(i), isolate)};
    std::unique_ptr<const char[]> exception;
    int32_t result = testing::CallWasmFunctionForTesting(
        isolate, instance, "main", base::VectorOf(arguments), &exception);
    // If there is nondeterminism, we cannot guarantee the behavior of the test
    // module, and in particular it may not terminate.
    if (WasmEngine::clear_nondeterminism()) break;
    // Reached max steps, do not try to execute the test module as it might
    // never terminate.
    if (max_steps < 0) break;
    // Similar to max steps reached, also discard modules that need too much
    // memory.
    if (near_heap_limit.heap_limit_reached()) {
      isolate->CancelTerminateExecution();
      break;
    }

    if (exception) {
      isolate->CancelTerminateExecution();
      if (strcmp(exception.get(),
                 "RangeError: Maximum call stack size exceeded") == 0) {
        // There was a stack overflow, which may happen nondeterministically. We
        // cannot guarantee the behavior of the test module, and in particular
        // it may not terminate.
        break;
      }
      results.emplace_back(exception.get());
    } else {
      results.emplace_back(result);
    }
  }
  thrower.Reset();
  isolate->clear_exception();
  return results;
}

}  // namespace

void ConfigureDeoptFuzzerFlags(v8::Isolate* isolate) {
  struct FlagConfiguration {
    FlagConfiguration() {
      // Disable the NativeModule cache. Different fuzzer iterations should not
      // interact with each other. Rerunning a fuzzer input (e.g. with
      // libfuzzer's "-runs=x" argument) should repeatedly test deoptimizations.
      // When caching the optimized code, only the first run will execute any
      // deopts.
      v8_flags.wasm_native_module_cache.Set(false);
      // We switch it to synchronous mode to avoid the nondeterminism of
      // background jobs finishing at random times.
      v8_flags.wasm_sync_tier_up.Set(true);
      // Enable the experimental features we want to fuzz. (Note that
      // EnableExperimentalWasmFeatures only enables pre-staged and staged
      // features.)
      v8_flags.wasm_deopt.Set(true);
      v8_flags.wasm_inlining_call_indirect.Set(true);
      // Make inlining more aggressive.
      v8_flags.wasm_inlining_ignore_call_counts.Set(true);
      v8_flags.wasm_inlining_budget.Set(v8_flags.wasm_inlining_budget * 5);
      v8_flags.wasm_inlining_max_size.Set(v8_flags.wasm_inlining_max_size * 5);
      v8_flags.wasm_inlining_factor.Set(v8_flags.wasm_inlining_factor * 5);
      // Allow mixed old and new EH instructions in the same module for fuzzing,
      // to help us test the interaction between the two EH proposals without
      // requiring multiple modules.
      v8_flags.wasm_allow_mixed_eh_for_testing.Set(true);

      // ResetTypeCanonicalizer is called by FuzzWasmDeopt, which requires
      // expose-gc. This is set in libFuzzer with FuzzerSupport, but not in
      // FuzzTest.
      v8_flags.expose_gc.Set(true);
    }
  };

  static FlagConfiguration config;

  // Enable other staged or experimental features and enforce flag
  // implications.
  EnableExperimentalWasmFeatures(isolate);
}

int FuzzWasmDeopt(base::Vector<const uint8_t> data, v8::Isolate* isolate) {
  if (!CpuFeatures::SupportsSimd128()) {
    return -1;
  }

  Isolate* i_isolate = reinterpret_cast<Isolate*>(isolate);

  ConfigureDeoptFuzzerFlags(isolate);

  // Clear recursive groups: The fuzzer creates random types in every run.
  // These are saved as recursive groups as part of the type canonicalizer,
  // but types from previous runs just waste memory.
  ResetTypeCanonicalizer(isolate);

  v8::TryCatch try_catch(isolate);
  HandleScope scope(i_isolate);

  AccountingAllocator allocator;
  Zone zone(&allocator, ZONE_NAME);

  std::vector<std::string> callees;
  std::vector<std::string> inlinees;
  base::Vector<const uint8_t> buffer =
      GenerateWasmModuleForDeopt(&zone, data, callees, inlinees);
  ModuleWireBytes wire_bytes(buffer.begin(), buffer.end());

  auto enabled_features = WasmEnabledFeatures::FromIsolate(i_isolate);
  bool valid = GetWasmEngine()->SyncValidate(
      i_isolate, enabled_features, CompileTimeImportsForFuzzing(), buffer);

  // Choose whether to optimize the main function or the outermost callee.
  // As the main function has a fixed signature, it doesn't provide great
  // coverage to always optimize and deopt the main function. Instead by
  // optimizing an inner wasm function, there can be a large amount of
  // parameters and returns with all kinds of types.
  const bool optimize_main_function =
      inlinees.empty() || data.empty() || !(data.back() & 1);
#if defined(DEBUG) && defined(V8_USE_ADDRESS_SANITIZER)
  // Disable register allocator verification on slow builds (Debug + ASan) to
  // avoid timeouts in TurboFan/Turboshaft compilation on pathological inputs
  // (see crbug.com/527760872).
  FlagScope<bool> no_verify_allocator(&v8_flags.turbo_verify_allocation, false);
  // Disable type assertions on slow builds (Debug + ASan) to avoid timeouts
  // in TurboFan compilation (see crbug.com/520317061).
  const bool assert_types = false;
#else
  const bool assert_types = !data.empty() && (data.back() & 2);
#endif
  FlagScope<bool> assert_types_scope(&v8_flags.wasm_assert_types, assert_types);

  if (v8_flags.wasm_fuzzer_gen_test) {
    StdoutStream os;
    std::stringstream flags;
    flags << " --allow-natives-syntax --wasm-deopt "
             "--wasm-inlining-ignore-call-counts --wasm-sync-tier-up "
             "--wasm-inlining-budget="
          << v8_flags.wasm_inlining_budget
          << " --wasm-inlining-max-size=" << v8_flags.wasm_inlining_max_size
          << " --wasm-inlining-factor=" << v8_flags.wasm_inlining_factor;
    // The deopt fuzzer generates a different call sequence.
    bool emit_call_main = false;
    GenerateTestCase(os, i_isolate, wire_bytes, valid, emit_call_main,
                     flags.str());
    // Emit helper.
    os << "let maybeThrows = (fct, ...args) => {\n"
          "  try {\n"
          "    print(fct(...args));\n"
          "  } catch (e) {\n"
          "    print(`caught ${e}`);\n"
          "  }\n"
          "};\n\n";
    // Emit calls for the different call target indices each followed by an
    // explicit tier-up.
    std::string fct("instance.exports.");
    fct.append(optimize_main_function ? "main" : inlinees.back());
    for (size_t i = 0; i < callees.size(); ++i) {
      os << "maybeThrows(instance.exports.main, " << i
         << ");\n"
            "%WasmTierUpFunction("
         << fct << ");\n";
    }
    os.flush();
  }

  ErrorThrower thrower(i_isolate, "WasmFuzzerSyncCompile");
  MaybeDirectHandle<WasmModuleObject> compiled = GetWasmEngine()->SyncCompile(
      i_isolate, enabled_features, CompileTimeImportsForFuzzing(), &thrower,
      base::UniqueCopyOf(buffer));
  if (!valid) {
    FATAL("Generated module should validate, but got: %s\n",
          thrower.error_msg());
  }

  std::vector<ExecutionResult> reference_results = PerformReferenceRun(
      callees, wire_bytes, enabled_features, valid, i_isolate);

  if (reference_results.size() < 2) {
    // If the first run already included non-determinism or aborted before
    // executing at least 2 callees (e.g. due to reaching max steps or stack
    // overflow), there isn't any value in compiling it as we need at least 2
    // callees to trigger a deopt.
    // Return -1 to not add this case to the corpus.
    return -1;
  }

  DirectHandle<WasmModuleObject> module_object = compiled.ToHandleChecked();
  DirectHandle<WasmInstanceObject> instance;
  if (!GetWasmEngine()
           ->SyncInstantiate(i_isolate, &thrower, module_object, {})
           .ToHandle(&instance)) {
    DCHECK(thrower.error());
    // The only reason to fail the second instantiation should be OOM. This
    // can happen e.g. for memories with a very big initial size especially on
    // 32 bit platforms.
    if (strstr(thrower.error_msg(), "Out of memory")) {
      return -1;  // Return -1 to not add this case to the corpus.
    }
    FATAL("Second instantiation failed unexpectedly: %s", thrower.error_msg());
  }
  DCHECK(!thrower.error());

  DirectHandle<WasmExportedFunction> main_function =
      testing::GetExportedFunction(i_isolate, instance, "main")
          .ToHandleChecked();
  int function_to_optimize =
      main_function->shared()->wasm_exported_function_data()->function_index();
  if (!optimize_main_function) {
    function_to_optimize--;
  }

  int deopt_count_begin = GetWasmEngine()->GetDeoptsExecutedCount();
  int deopt_count_previous_iteration = deopt_count_begin;
  size_t num_callees = reference_results.size();
  for (uint32_t i = 0; i < num_callees; ++i) {
    DirectHandle<Object> arguments[] = {
        direct_handle(Smi::FromInt(i), i_isolate)};
    std::unique_ptr<const char[]> exception;

    DCHECK(!WasmEngine::had_nondeterminism());  // No dangling nondeterminism.
    int32_t result_value = testing::CallWasmFunctionForTesting(
        i_isolate, instance, "main", base::VectorOf(arguments), &exception);
    // Also the second run can hit nondeterminism which was not hit before
    // (when growing memory). In that case, do not compare results.
    // TODO(384781857): Due to nondeterminism, the second run could even not
    // terminate. If this happens often enough we should do something about
    // this.
    if (WasmEngine::clear_nondeterminism()) return -1;

    ExecutionResult actual_result;
    if (exception) {
      actual_result = exception.get();
    } else {
      actual_result = result_value;
    }
    if (actual_result != reference_results[i]) {
      std::cerr << "Different results vs. reference run for callee "
                << callees[i] << ": \nReference: " << reference_results[i]
                << "\nActual: " << actual_result << std::endl;
      CHECK_EQ(actual_result, reference_results[i]);
      UNREACHABLE();
    }

    int deopt_count = GetWasmEngine()->GetDeoptsExecutedCount();
    if (i != 0 && deopt_count == deopt_count_previous_iteration) {
      // No deopt triggered. Skip the rest of the run as it won't provide
      // meaningful coverage for the deoptimizer.
      // Return -1 to prevent adding this case to the corpus if not a single
      // deopt was executed.
      return deopt_count == deopt_count_begin ? -1 : 0;
    }
    deopt_count_previous_iteration = deopt_count;

    TierUpNowForTesting(i_isolate, instance->trusted_data(i_isolate),
                        function_to_optimize);
  }

  return 0;
}

}  // namespace v8::internal::wasm::fuzzing
