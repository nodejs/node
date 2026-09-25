// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_MODULE_COMPILER_H_
#define V8_WASM_MODULE_COMPILER_H_

#if !V8_ENABLE_WEBASSEMBLY
#error This header should only be included if WebAssembly is enabled.
#endif  // !V8_ENABLE_WEBASSEMBLY

#include <atomic>
#include <memory>

#include "include/v8-metrics.h"
#include "src/base/platform/time.h"
#include "src/common/globals.h"
#include "src/handles/maybe-handles.h"
#include "src/tasks/cancelable-task.h"
#include "src/wasm/compilation-environment.h"
#include "src/wasm/wasm-features.h"
#include "src/wasm/wasm-import-wrapper-cache.h"
#include "src/wasm/wasm-module.h"

namespace v8 {

namespace base {
template <typename T>
class Vector;
}  // namespace base

namespace internal {

class JSArrayBuffer;
class JSPromise;
class WasmModuleObject;
class WasmInstanceObject;
class WasmTrustedInstanceData;

namespace wasm {

struct CompilationEnv;
class CompilationResultResolver;
class ErrorThrower;
class ModuleCompiler;
class NativeModule;
class ProfileInformation;
class StreamingDecoder;
class WasmCode;
struct WasmModule;

V8_EXPORT_PRIVATE
std::shared_ptr<NativeModule> CompileToNativeModule(
    Isolate* isolate, WasmEnabledFeatures enabled_features,
    WasmDetectedFeatures detected_features, CompileTimeImports compile_imports,
    ErrorThrower* thrower, std::shared_ptr<const WasmModule> module,
    base::OwnedVector<const uint8_t> wire_bytes, int compilation_id,
    v8::metrics::Recorder::ContextId context_id, ProfileInformation* pgo_info);

V8_EXPORT_PRIVATE WasmError ValidateAndSetBuiltinImports(
    const WasmModule* module, base::Vector<const uint8_t> wire_bytes,
    const CompileTimeImports& imports, WasmDetectedFeatures* detected);

// Compiles the wrapper for this (kind, sig) pair and sets the corresponding
// cache entry. Assumes the key already exists in the cache but has not been
// compiled yet.
V8_EXPORT_PRIVATE
std::shared_ptr<wasm::WasmWrapperHandle> CompileImportWrapperForTest(
    Isolate* isolate, ImportCallKind kind, const CanonicalSig* sig,
    int expected_arity, Suspend suspend);

// Triggered by the WasmCompileLazy builtin. The return value indicates whether
// compilation was successful. Lazy compilation can fail only if validation is
// also lazy.
bool CompileLazy(Isolate*, NativeModule*, int func_index);

// Throws the compilation error after failed lazy compilation.
void ThrowLazyCompilationError(Isolate* isolate,
                               const NativeModule* native_module,
                               int func_index);

// Trigger tier-up of a particular function to TurboFan. If tier-up was already
// triggered, we instead increase the priority with exponential back-off.
V8_EXPORT_PRIVATE void TriggerTierUp(Isolate*, Tagged<WasmTrustedInstanceData>,
                                     int func_index);
// Synchronous version of the above.
V8_EXPORT_PRIVATE void TierUpNowForTesting(Isolate*,
                                           Tagged<WasmTrustedInstanceData>,
                                           int func_index);
// Same, but all functions.
V8_EXPORT_PRIVATE void TierUpAllForTesting(Isolate*,
                                           Tagged<WasmTrustedInstanceData>);

V8_EXPORT_PRIVATE void InitializeCompilationForTesting(
    NativeModule* native_module);

// Publish a set of detected features in a given isolate. If this is the initial
// compilation, also the "kWasmModuleCompilation" use counter is incremented to
// serve as a baseline for the other detected features.
void PublishDetectedFeatures(WasmDetectedFeatures, Isolate*,
                             bool is_initial_compilation);

// Encapsulates all the state and steps of an asynchronous compilation.
// An asynchronous compile job consists of a number of tasks that are executed
// as foreground and background tasks. Any phase that touches the V8 heap or
// allocates on the V8 heap (e.g. creating the module object) must be a
// foreground task. All other tasks (e.g. decoding and validating, the majority
// of the work of compilation) can be background tasks.
// TODO(wasm): factor out common parts of this with the synchronous pipeline.
class AsyncCompileJob {
 public:
  AsyncCompileJob(WasmEnabledFeatures enabled_features,
                  CompileTimeImports compile_imports,
                  base::OwnedVector<const uint8_t> bytes,
                  const char* api_method_name,
                  std::shared_ptr<CompilationResultResolver> resolver,
                  int compilation_id);
  ~AsyncCompileJob();

  void InitializeIsolateSpecificInfo(Isolate*);

  // Start asynchronous decoding; this triggers the full asynchronous
  // (non-streaming) compilation.
  void StartAsyncDecoding();

  std::shared_ptr<StreamingDecoder> CreateStreamingDecoder();

  void Abort();
  void CancelPendingForegroundTask();

  // Return the isolate that this AsyncCompileJob belongs to, or `nullptr` if
  // this is not associated to any isolate (yet).
  Isolate* isolate() const {
    if (!has_isolate_specific_info_.load(std::memory_order_acquire)) return {};
    return isolate_specific_info_.isolate_;
  }

  // Return the context that this AsyncCompileJob belongs to, or an empty handle
  // if this is not associated to any isolate and context (yet).
  MaybeDirectHandle<NativeContext> context() const {
    if (!has_isolate_specific_info_.load(std::memory_order_acquire)) return {};
    return isolate_specific_info_.native_context_;
  }

 private:
  class CompileTask;
  class CompileStep;
  class CompilationStateCallback;

  // States of the AsyncCompileJob.
  // Step 1 (async). Decodes the wasm module (only called for non-streaming
  //                 compilation; streaming uses the StreamingDecoder instead).
  // --> Fail on decoding failure,
  // --> PrepareNativeModule on success.
  class DecodeModule;

  // Step 2 (async). Allocates NativeModule and potentially starts background
  // compilation.
  // --> finish directly on native module cache hit,
  // --> finish directly on validation error,
  // --> trigger eager compilation, if any; FinishCompilation is triggered when
  // done.
  class PrepareNativeModule;

  // Step 3 (sync). Compilation finished. Finalize the module and resolve the
  // promise.
  class FinishCompilation;

  // Step 4 (sync). Decoding, validation or compilation failed. Reject the
  // promise.
  class Fail;

  friend class AsyncStreamingProcessor;

  // Decrements the number of outstanding finishers. The last caller of this
  // function should finish the asynchronous compilation, see the comment on
  // {outstanding_finishers_}.
  V8_WARN_UNUSED_RESULT bool DecrementAndCheckFinisherCount() {
    DCHECK_LT(0, outstanding_finishers_.load());
    return outstanding_finishers_.fetch_sub(1) == 1;
  }

  void CreateNativeModule(std::shared_ptr<const WasmModule> module,
                          size_t code_size_estimate);
  // Return the module (cached or freshly allocated) and true for cache hit,
  // false for cache miss.
  std::tuple<std::shared_ptr<NativeModule>, bool> GetOrCreateNativeModule(
      std::shared_ptr<const WasmModule> module, size_t code_size_estimate);

  // {FinishCompile} and {Failed} invalidate the {AsyncCompileJob}, so we only
  // allow to call them on r-value references to make this clear at call sites.
  void FinishCompile(std::shared_ptr<NativeModule> final_native_module,
                     DirectHandle<WasmModuleObject> deserialized_module_object,
                     bool cache_hit) &&;
  void Failed() &&;

  void AsyncCompileSucceeded(DirectHandle<WasmModuleObject> result);

  void StartForegroundTask();
  void StartBackgroundTask();

  enum UseExistingForegroundTask : bool {
    kUseExistingForegroundTask = true,
    kAssertNoExistingForegroundTask = false
  };
  // Switches to the compilation step {Step} and starts a foreground task to
  // execute it. Most of the time we know that there cannot be a running
  // foreground task. If there might be one, then pass
  // kUseExistingForegroundTask to avoid spawning a second one.
  template <typename Step,
            UseExistingForegroundTask = kAssertNoExistingForegroundTask,
            typename... Args>
  void DoSync(Args&&... args);

  // Switches to the compilation step {Step} and starts a background task to
  // execute it.
  template <typename Step, typename... Args>
  void DoAsync(Args&&... args);

  // Switches to the compilation step {Step} but does not start a task to
  // execute it.
  template <typename Step, typename... Args>
  void NextStep(Args&&... args);

  // Some information is only provided late (after assigning an isolate). Store
  // that in a separate late-initialized field to avoid accidentally using a
  // field too early.
  struct IsolateSpecificInfo {
    Isolate* isolate_ = nullptr;
    IndirectHandle<NativeContext> native_context_;
    // TODO(clemensb): Move this out into the resolver.
    IndirectHandle<NativeContext> incumbent_context_;
    v8::metrics::Recorder::ContextId context_id_;

    std::shared_ptr<v8::TaskRunner> foreground_task_runner_;
  };

  // `isolate_specific_info_` is initialized lazily, potentially after
  // registering the `AsyncCompileJob` with the `WasmEngine`. In order to avoid
  // data races, any thread which has not read or written the field before
  // should read the `has_isolate_specific_info_` field first (with acquire
  // semantics).
  std::atomic<bool> has_isolate_specific_info_{false};
  IsolateSpecificInfo isolate_specific_info_;

  const char* const api_method_name_;
  const WasmEnabledFeatures enabled_features_;
  WasmDetectedFeatures detected_features_;
  CompileTimeImports compile_imports_;
  base::TimeTicks start_time_;
  base::TimeTicks compilation_finished_time_;
  // Copy of the module wire bytes, moved into the {new_native_module_} on its
  // creation.
  base::OwnedVector<const uint8_t> bytes_copy_;
  // Reference to the wire bytes (held in {bytes_copy_} or as part of
  // {new_native_module_}).
  ModuleWireBytes wire_bytes_;
  const std::shared_ptr<CompilationResultResolver> resolver_;

  // The {NativeModule} which was created for this async compilation.
  // This is only set once, and stays alive as long as this job stays alive.
  // Note that the finally used module can be different, if we find a module in
  // the cache.
  std::shared_ptr<NativeModule> new_native_module_;

  // Outstanding counter updates / metrics events, to be published when
  // finishing (in a foreground task).
  DelayedCounterUpdates delayed_counters_;
  std::optional<v8::metrics::WasmModuleDecoded> decoding_metrics_event_;

  std::unique_ptr<CompileStep> step_;
  CancelableTaskManager background_task_manager_;

  // For async compilation the AsyncCompileJob is the only finisher. For
  // streaming compilation also the AsyncStreamingProcessor has to finish before
  // compilation can be finished.
  std::atomic<int32_t> outstanding_finishers_{1};

  // A reference to a pending foreground task, or {nullptr} if none is pending.
  CompileTask* pending_foreground_task_ = nullptr;

  // The AsyncCompileJob owns the StreamingDecoder because the StreamingDecoder
  // contains data which is needed by the AsyncCompileJob for streaming
  // compilation. The AsyncCompileJob does not actively use the
  // StreamingDecoder.
  std::shared_ptr<StreamingDecoder> stream_;

  // The compilation id to identify trace events linked to this compilation.
  const int compilation_id_;
};

// The main purpose of this class is to copy the feedback vectors that live in
// `FixedArray`s on the JavaScript heap to a C++ datastructure on the `module`
// that is accessible to the background compilation threads.
// While we are at it, we also do some light processing here, e.g., mapping the
// feedback to functions, identified by their function index, and filtering out
// feedback for calls to imported functions (which we currently don't inline).
class V8_EXPORT_PRIVATE TransitiveTypeFeedbackProcessor {
 public:
  static void Process(Isolate* isolate,
                      Tagged<WasmTrustedInstanceData> trusted_instance_data,
                      int func_index) {
    TransitiveTypeFeedbackProcessor ttfp{isolate, trusted_instance_data};
    ttfp.queue_.insert(func_index);
    ttfp.ProcessQueue();
  }

  static void ProcessAll(Isolate* isolate,
                         Tagged<WasmTrustedInstanceData> trusted_instance_data);

 private:
  TransitiveTypeFeedbackProcessor(
      Isolate* isolate, Tagged<WasmTrustedInstanceData> trusted_instance_data);

  ~TransitiveTypeFeedbackProcessor() { DCHECK(queue_.empty()); }

  void ProcessQueue() {
    while (!queue_.empty()) {
      auto next = queue_.cbegin();
      ProcessFunction(*next);
      queue_.erase(next);
    }
  }

  void ProcessFunction(int func_index);

  void EnqueueCallees(base::Vector<CallSiteFeedback> feedback) {
    for (const CallSiteFeedback& csf : feedback) {
      for (int j = 0; j < csf.num_cases(); j++) {
        int func = csf.function_index(j);
        // Don't spend time on calls that have never been executed.
        if (csf.call_count(j) == 0) continue;
        // Don't recompute feedback that has already been processed.
        auto existing = feedback_for_function_.find(func);
        if (existing != feedback_for_function_.end() &&
            !existing->second.feedback_vector.empty()) {
          if (!existing->second.needs_reprocessing_after_deopt) {
            continue;
          }
          DCHECK(v8_flags.wasm_deopt);
          existing->second.needs_reprocessing_after_deopt = false;
        }
        queue_.insert(func);
      }
    }
  }

  DisallowGarbageCollection no_gc_scope_;
  Isolate* const isolate_;
  const Tagged<WasmTrustedInstanceData> instance_data_;
  const WasmModule* const module_;
  // TODO(jkummerow): Check if it makes a difference to apply any updates
  // as a single batch at the end.
  base::MutexGuard mutex_guard;
  std::unordered_map<uint32_t, FunctionTypeFeedback>& feedback_for_function_;
  std::set<int> queue_;
};

}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif  // V8_WASM_MODULE_COMPILER_H_
