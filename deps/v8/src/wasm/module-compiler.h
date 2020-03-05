// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_MODULE_COMPILER_H_
#define V8_WASM_MODULE_COMPILER_H_

#include <atomic>
#include <functional>
#include <memory>

#include "src/base/optional.h"
#include "src/common/globals.h"
#include "src/tasks/cancelable-task.h"
#include "src/wasm/compilation-environment.h"
#include "src/wasm/wasm-features.h"
#include "src/wasm/wasm-import-wrapper-cache.h"
#include "src/wasm/wasm-module.h"

namespace v8 {
namespace internal {

class JSArrayBuffer;
class JSPromise;
class Counters;
class WasmModuleObject;
class WasmInstanceObject;

template <typename T>
class Vector;

namespace wasm {

struct CompilationEnv;
class CompilationResultResolver;
class ErrorThrower;
class ModuleCompiler;
class NativeModule;
class WasmCode;
struct WasmModule;

std::shared_ptr<NativeModule> CompileToNativeModule(
    Isolate* isolate, const WasmFeatures& enabled, ErrorThrower* thrower,
    std::shared_ptr<const WasmModule> module, const ModuleWireBytes& wire_bytes,
    Handle<FixedArray>* export_wrappers_out);

void RecompileNativeModule(Isolate* isolate, NativeModule* native_module,
                           ExecutionTier tier);

V8_EXPORT_PRIVATE
void CompileJsToWasmWrappers(Isolate* isolate, const WasmModule* module,
                             Handle<FixedArray>* export_wrappers_out);

// Compiles the wrapper for this (kind, sig) pair and sets the corresponding
// cache entry. Assumes the key already exists in the cache but has not been
// compiled yet.
V8_EXPORT_PRIVATE
WasmCode* CompileImportWrapper(
    WasmEngine* wasm_engine, NativeModule* native_module, Counters* counters,
    compiler::WasmImportCallKind kind, FunctionSig* sig,
    WasmImportWrapperCache::ModificationScope* cache_scope);

V8_EXPORT_PRIVATE Handle<Script> CreateWasmScript(
    Isolate* isolate, const ModuleWireBytes& wire_bytes,
    Vector<const char> source_map_url, WireBytesRef name,
    Vector<const char> source_url = {});

// Triggered by the WasmCompileLazy builtin. The return value indicates whether
// compilation was successful. Lazy compilation can fail only if validation is
// also lazy.
bool CompileLazy(Isolate*, NativeModule*, int func_index);

int GetMaxBackgroundTasks();

template <typename Key, typename Hash>
class WrapperQueue {
 public:
  // Removes an arbitrary key from the queue and returns it.
  // If the queue is empty, returns nullopt.
  // Thread-safe.
  base::Optional<Key> pop() {
    base::Optional<Key> key = base::nullopt;
    base::LockGuard<base::Mutex> lock(&mutex_);
    auto it = queue_.begin();
    if (it != queue_.end()) {
      key = *it;
      queue_.erase(it);
    }
    return key;
  }

  // Add the given key to the queue and returns true iff the insert was
  // successful.
  // Not thread-safe.
  bool insert(const Key& key) { return queue_.insert(key).second; }

 private:
  base::Mutex mutex_;
  std::unordered_set<Key, Hash> queue_;
};

// Encapsulates all the state and steps of an asynchronous compilation.
// An asynchronous compile job consists of a number of tasks that are executed
// as foreground and background tasks. Any phase that touches the V8 heap or
// allocates on the V8 heap (e.g. creating the module object) must be a
// foreground task. All other tasks (e.g. decoding and validating, the majority
// of the work of compilation) can be background tasks.
// TODO(wasm): factor out common parts of this with the synchronous pipeline.
class AsyncCompileJob {
 public:
  AsyncCompileJob(Isolate* isolate, const WasmFeatures& enabled_features,
                  std::unique_ptr<byte[]> bytes_copy, size_t length,
                  Handle<Context> context, const char* api_method_name,
                  std::shared_ptr<CompilationResultResolver> resolver);
  ~AsyncCompileJob();

  void Start();

  std::shared_ptr<StreamingDecoder> CreateStreamingDecoder();

  void Abort();
  void CancelPendingForegroundTask();

  Isolate* isolate() const { return isolate_; }

  Handle<Context> context() const { return native_context_; }

 private:
  class CompileTask;
  class CompileStep;
  class CompilationStateCallback;

  // States of the AsyncCompileJob.
  class DecodeModule;            // Step 1  (async)
  class DecodeFail;              // Step 1b (sync)
  class PrepareAndStartCompile;  // Step 2  (sync)
  class CompileFailed;           // Step 3a (sync)
  class CompileFinished;         // Step 3b (sync)

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
  void PrepareRuntimeObjects();

  void FinishCompile();

  void DecodeFailed(const WasmError&);
  void AsyncCompileFailed();

  void AsyncCompileSucceeded(Handle<WasmModuleObject> result);

  void FinishModule();

  void StartForegroundTask();
  void ExecuteForegroundTaskImmediately();

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

  // Switches to the compilation step {Step} and immediately executes that step.
  template <typename Step, typename... Args>
  void DoImmediately(Args&&... args);

  // Switches to the compilation step {Step} and starts a background task to
  // execute it.
  template <typename Step, typename... Args>
  void DoAsync(Args&&... args);

  // Switches to the compilation step {Step} but does not start a task to
  // execute it.
  template <typename Step, typename... Args>
  void NextStep(Args&&... args);

  Isolate* const isolate_;
  const char* const api_method_name_;
  const WasmFeatures enabled_features_;
  const bool wasm_lazy_compilation_;
  base::TimeTicks start_time_;
  // Copy of the module wire bytes, moved into the {native_module_} on its
  // creation.
  std::unique_ptr<byte[]> bytes_copy_;
  // Reference to the wire bytes (held in {bytes_copy_} or as part of
  // {native_module_}).
  ModuleWireBytes wire_bytes_;
  Handle<Context> native_context_;
  const std::shared_ptr<CompilationResultResolver> resolver_;

  Handle<WasmModuleObject> module_object_;
  std::shared_ptr<NativeModule> native_module_;

  std::unique_ptr<CompileStep> step_;
  CancelableTaskManager background_task_manager_;

  std::shared_ptr<v8::TaskRunner> foreground_task_runner_;

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
};

}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif  // V8_WASM_MODULE_COMPILER_H_
