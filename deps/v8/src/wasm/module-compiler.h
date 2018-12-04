// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_MODULE_COMPILER_H_
#define V8_WASM_MODULE_COMPILER_H_

#include <atomic>
#include <functional>
#include <memory>

#include "src/cancelable-task.h"
#include "src/globals.h"
#include "src/wasm/wasm-features.h"
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

class CompilationResultResolver;
class CompilationState;
class ErrorThrower;
class ModuleCompiler;
class NativeModule;
class WasmCode;
struct ModuleEnv;
struct WasmModule;

struct CompilationStateDeleter {
  void operator()(CompilationState* compilation_state) const;
};

// Wrapper to create a CompilationState exists in order to avoid having
// the CompilationState in the header file.
std::unique_ptr<CompilationState, CompilationStateDeleter> NewCompilationState(
    Isolate* isolate, const ModuleEnv& env);

ModuleEnv* GetModuleEnv(CompilationState* compilation_state);

MaybeHandle<WasmModuleObject> CompileToModuleObject(
    Isolate* isolate, const WasmFeatures& enabled, ErrorThrower* thrower,
    std::shared_ptr<const WasmModule> module, const ModuleWireBytes& wire_bytes,
    Handle<Script> asm_js_script, Vector<const byte> asm_js_offset_table_bytes);

MaybeHandle<WasmInstanceObject> InstantiateToInstanceObject(
    Isolate* isolate, ErrorThrower* thrower,
    Handle<WasmModuleObject> module_object, MaybeHandle<JSReceiver> imports,
    MaybeHandle<JSArrayBuffer> memory);

V8_EXPORT_PRIVATE
void CompileJsToWasmWrappers(Isolate* isolate,
                             Handle<WasmModuleObject> module_object);

V8_EXPORT_PRIVATE Handle<Script> CreateWasmScript(
    Isolate* isolate, const ModuleWireBytes& wire_bytes,
    const std::string& source_map_url);

// Triggered by the WasmCompileLazy builtin.
// Returns the instruction start of the compiled code object.
Address CompileLazy(Isolate*, NativeModule*, uint32_t func_index);

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
                  Handle<Context> context,
                  std::shared_ptr<CompilationResultResolver> resolver);
  ~AsyncCompileJob();

  void Start();

  std::shared_ptr<StreamingDecoder> CreateStreamingDecoder();

  void Abort();
  void CancelPendingForegroundTask();

  Isolate* isolate() const { return isolate_; }

 private:
  class CompileTask;
  class CompileStep;

  // States of the AsyncCompileJob.
  class DecodeModule;            // Step 1  (async)
  class DecodeFail;              // Step 1b (sync)
  class PrepareAndStartCompile;  // Step 2  (sync)
  class CompileFailed;           // Step 4b (sync)
  class CompileWrappers;         // Step 5  (sync)
  class FinishModule;            // Step 6  (sync)

  const std::shared_ptr<Counters>& async_counters() const {
    return async_counters_;
  }
  Counters* counters() const { return async_counters().get(); }

  void FinishCompile();

  void AsyncCompileFailed(Handle<Object> error_reason);

  void AsyncCompileSucceeded(Handle<WasmModuleObject> result);

  void StartForegroundTask();
  void ExecuteForegroundTaskImmediately();

  void StartBackgroundTask();

  // Switches to the compilation step {Step} and starts a foreground task to
  // execute it.
  template <typename Step, typename... Args>
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

  friend class AsyncStreamingProcessor;

  Isolate* isolate_;
  const WasmFeatures enabled_features_;
  const std::shared_ptr<Counters> async_counters_;
  // Copy of the module wire bytes, moved into the {native_module_} on its
  // creation.
  std::unique_ptr<byte[]> bytes_copy_;
  // Reference to the wire bytes (hold in {bytes_copy_} or as part of
  // {native_module_}).
  ModuleWireBytes wire_bytes_;
  Handle<Context> native_context_;
  std::shared_ptr<CompilationResultResolver> resolver_;

  std::vector<DeferredHandles*> deferred_handles_;
  Handle<WasmModuleObject> module_object_;
  NativeModule* native_module_ = nullptr;

  std::unique_ptr<CompileStep> step_;
  CancelableTaskManager background_task_manager_;

  std::shared_ptr<v8::TaskRunner> foreground_task_runner_;

  // For async compilation the AsyncCompileJob is the only finisher. For
  // streaming compilation also the AsyncStreamingProcessor has to finish before
  // compilation can be finished.
  std::atomic<int32_t> outstanding_finishers_{1};

  // Decrements the number of outstanding finishers. The last caller of this
  // function should finish the asynchronous compilation, see the comment on
  // {outstanding_finishers_}.
  V8_WARN_UNUSED_RESULT bool DecrementAndCheckFinisherCount() {
    return outstanding_finishers_.fetch_sub(1) == 1;
  }

  // A reference to a pending foreground task, or {nullptr} if none is pending.
  CompileTask* pending_foreground_task_ = nullptr;

  // The AsyncCompileJob owns the StreamingDecoder because the StreamingDecoder
  // contains data which is needed by the AsyncCompileJob for streaming
  // compilation. The AsyncCompileJob does not actively use the
  // StreamingDecoder.
  std::shared_ptr<StreamingDecoder> stream_;

  bool tiering_completed_ = false;
};
}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif  // V8_WASM_MODULE_COMPILER_H_
