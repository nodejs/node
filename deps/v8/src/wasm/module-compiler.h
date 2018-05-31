// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_MODULE_COMPILER_H_
#define V8_WASM_MODULE_COMPILER_H_

#include <functional>

#include "src/base/atomic-utils.h"
#include "src/cancelable-task.h"
#include "src/isolate.h"

#include "src/wasm/module-decoder.h"
#include "src/wasm/streaming-decoder.h"
#include "src/wasm/wasm-module.h"
#include "src/wasm/wasm-objects.h"

namespace v8 {
namespace internal {
namespace wasm {

class ModuleCompiler;
class WasmCode;
class CompilationState;

struct CompilationStateDeleter {
  void operator()(CompilationState* compilation_state) const;
};

// Wrapper to create a CompilationState exists in order to avoid having
// the the CompilationState in the header file.
std::unique_ptr<CompilationState, CompilationStateDeleter> NewCompilationState(
    Isolate* isolate);

MaybeHandle<WasmModuleObject> CompileToModuleObject(
    Isolate* isolate, ErrorThrower* thrower, std::unique_ptr<WasmModule> module,
    const ModuleWireBytes& wire_bytes, Handle<Script> asm_js_script,
    Vector<const byte> asm_js_offset_table_bytes);

MaybeHandle<WasmInstanceObject> InstantiateToInstanceObject(
    Isolate* isolate, ErrorThrower* thrower,
    Handle<WasmModuleObject> module_object, MaybeHandle<JSReceiver> imports,
    MaybeHandle<JSArrayBuffer> memory);

V8_EXPORT_PRIVATE
void CompileJsToWasmWrappers(Isolate* isolate,
                             Handle<WasmCompiledModule> compiled_module,
                             Counters* counters);

V8_EXPORT_PRIVATE Handle<Script> CreateWasmScript(
    Isolate* isolate, const ModuleWireBytes& wire_bytes);

// Triggered by the WasmCompileLazy builtin.
// Walks the stack (top three frames) to determine the wasm instance involved
// and which function to compile.
// Then triggers WasmCompiledModule::CompileLazy, taking care of correctly
// patching the call site or indirect function tables.
// Returns either the Code object that has been lazily compiled, or Illegal if
// an error occurred. In the latter case, a pending exception has been set,
// which will be triggered when returning from the runtime function, i.e. the
// Illegal builtin will never be called.
Address CompileLazy(Isolate* isolate, Handle<WasmInstanceObject> instance);

// Encapsulates all the state and steps of an asynchronous compilation.
// An asynchronous compile job consists of a number of tasks that are executed
// as foreground and background tasks. Any phase that touches the V8 heap or
// allocates on the V8 heap (e.g. creating the module object) must be a
// foreground task. All other tasks (e.g. decoding and validating, the majority
// of the work of compilation) can be background tasks.
// TODO(wasm): factor out common parts of this with the synchronous pipeline.
class AsyncCompileJob {
 public:
  explicit AsyncCompileJob(Isolate* isolate, std::unique_ptr<byte[]> bytes_copy,
                           size_t length, Handle<Context> context,
                           Handle<JSPromise> promise);

  void Start();

  std::shared_ptr<StreamingDecoder> CreateStreamingDecoder();

  void Abort();

  ~AsyncCompileJob();

 private:
  class CompileTask;
  class CompileStep;

  // States of the AsyncCompileJob.
  class DecodeModule;
  class DecodeFail;
  class PrepareAndStartCompile;
  class CompileFailed;
  class WaitForBackgroundTasks;
  class FinishCompilationUnits;
  class FinishCompile;
  class CompileWrappers;
  class FinishModule;
  class AbortCompilation;

  const std::shared_ptr<Counters>& async_counters() const {
    return async_counters_;
  }
  Counters* counters() const { return async_counters().get(); }

  void AsyncCompileFailed(Handle<Object> error_reason);

  void AsyncCompileSucceeded(Handle<Object> result);

  void StartForegroundTask();

  void StartBackgroundTask();

  // Switches to the compilation step {Step} and starts a foreground task to
  // execute it.
  template <typename Step, typename... Args>
  void DoSync(Args&&... args);

  // Switches to the compilation step {Step} and starts a background task to
  // execute it.
  template <typename Step, typename... Args>
  void DoAsync(Args&&... args);

  // Switches to the compilation step {Step} but does not start a task to
  // execute it.
  template <typename Step, typename... Args>
  void NextStep(Args&&... args);

  Isolate* isolate() { return isolate_; }

  friend class AsyncStreamingProcessor;

  Isolate* isolate_;
  const std::shared_ptr<Counters> async_counters_;
  std::unique_ptr<byte[]> bytes_copy_;
  ModuleWireBytes wire_bytes_;
  Handle<Context> context_;
  Handle<JSPromise> module_promise_;
  std::unique_ptr<compiler::ModuleEnv> module_env_;
  std::unique_ptr<WasmModule> module_;

  std::vector<DeferredHandles*> deferred_handles_;
  Handle<WasmModuleObject> module_object_;
  Handle<WasmCompiledModule> compiled_module_;

  std::unique_ptr<CompileStep> step_;
  CancelableTaskManager background_task_manager_;
  Handle<Code> centry_stub_;

  std::shared_ptr<v8::TaskRunner> foreground_task_runner_;
  std::shared_ptr<v8::TaskRunner> background_task_runner_;

  // For async compilation the AsyncCompileJob is the only finisher. For
  // streaming compilation also the AsyncStreamingProcessor has to finish before
  // compilation can be finished.
  base::AtomicNumber<int32_t> outstanding_finishers_{1};

  // Decrements the number of outstanding finishers. The last caller of this
  // function should finish the asynchronous compilation, see the comment on
  // {outstanding_finishers_}.
  V8_WARN_UNUSED_RESULT bool DecrementAndCheckFinisherCount() {
    return outstanding_finishers_.Decrement(1) == 0;
  }

  // Counts the number of pending foreground tasks.
  int32_t num_pending_foreground_tasks_ = 0;

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
