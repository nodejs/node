// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_WASM_ENGINE_H_
#define V8_WASM_WASM_ENGINE_H_

#include <memory>

#include "src/wasm/wasm-code-manager.h"
#include "src/wasm/wasm-memory.h"
#include "src/zone/accounting-allocator.h"

namespace v8 {
namespace internal {

class CodeTracer;
class CompilationStatistics;
class WasmModuleObject;
class WasmInstanceObject;

namespace wasm {

class ErrorThrower;
struct ModuleWireBytes;

class V8_EXPORT_PRIVATE CompilationResultResolver {
 public:
  virtual void OnCompilationSucceeded(Handle<WasmModuleObject> result) = 0;
  virtual void OnCompilationFailed(Handle<Object> error_reason) = 0;
  virtual ~CompilationResultResolver() {}
};

class V8_EXPORT_PRIVATE InstantiationResultResolver {
 public:
  virtual void OnInstantiationSucceeded(Handle<WasmInstanceObject> result) = 0;
  virtual void OnInstantiationFailed(Handle<Object> error_reason) = 0;
  virtual ~InstantiationResultResolver() {}
};

// The central data structure that represents an engine instance capable of
// loading, instantiating, and executing WASM code.
class V8_EXPORT_PRIVATE WasmEngine {
 public:
  explicit WasmEngine(std::unique_ptr<WasmCodeManager> code_manager);
  ~WasmEngine();

  // Synchronously validates the given bytes that represent an encoded WASM
  // module.
  bool SyncValidate(Isolate* isolate, const ModuleWireBytes& bytes);

  // Synchronously compiles the given bytes that represent a translated
  // asm.js module.
  MaybeHandle<WasmModuleObject> SyncCompileTranslatedAsmJs(
      Isolate* isolate, ErrorThrower* thrower, const ModuleWireBytes& bytes,
      Handle<Script> asm_js_script,
      Vector<const byte> asm_js_offset_table_bytes);

  // Synchronously compiles the given bytes that represent an encoded WASM
  // module.
  MaybeHandle<WasmModuleObject> SyncCompile(Isolate* isolate,
                                            ErrorThrower* thrower,
                                            const ModuleWireBytes& bytes);

  // Synchronously instantiate the given WASM module with the given imports.
  // If the module represents an asm.js module, then the supplied {memory}
  // should be used as the memory of the instance.
  MaybeHandle<WasmInstanceObject> SyncInstantiate(
      Isolate* isolate, ErrorThrower* thrower,
      Handle<WasmModuleObject> module_object, MaybeHandle<JSReceiver> imports,
      MaybeHandle<JSArrayBuffer> memory);

  // Begin an asynchronous compilation of the given bytes that represent an
  // encoded WASM module.
  // The {is_shared} flag indicates if the bytes backing the module could
  // be shared across threads, i.e. could be concurrently modified.
  void AsyncCompile(Isolate* isolate,
                    std::unique_ptr<CompilationResultResolver> resolver,
                    const ModuleWireBytes& bytes, bool is_shared);

  // Begin an asynchronous instantiation of the given WASM module.
  void AsyncInstantiate(Isolate* isolate,
                        std::unique_ptr<InstantiationResultResolver> resolver,
                        Handle<WasmModuleObject> module_object,
                        MaybeHandle<JSReceiver> imports);

  std::shared_ptr<StreamingDecoder> StartStreamingCompilation(
      Isolate* isolate, Handle<Context> context,
      std::unique_ptr<CompilationResultResolver> resolver);

  WasmCodeManager* code_manager() const { return code_manager_.get(); }

  WasmMemoryTracker* memory_tracker() { return &memory_tracker_; }

  AccountingAllocator* allocator() { return &allocator_; }

  // Compilation statistics for TurboFan compilations.
  CompilationStatistics* GetOrCreateTurboStatistics();

  // Prints the gathered compilation statistics, then resets them.
  void DumpAndResetTurboStatistics();

  // Used to redirect tracing output from {stdout} to a file.
  CodeTracer* GetCodeTracer();

  // We register and unregister CancelableTaskManagers that run engine-dependent
  // tasks. These tasks need to be shutdown if the engine is shut down.
  void Register(CancelableTaskManager* task_manager);
  void Unregister(CancelableTaskManager* task_manager);

  // Remove {job} from the list of active compile jobs.
  std::unique_ptr<AsyncCompileJob> RemoveCompileJob(AsyncCompileJob* job);

  // Returns true if at lease one AsyncCompileJob is currently running.
  bool HasRunningCompileJob() const { return !jobs_.empty(); }

  // Cancel all AsyncCompileJobs that belong to the given Isolate. Their
  // deletion is delayed until all tasks accessing the AsyncCompileJob finish
  // their execution. This is used to clean-up the isolate to be reused.
  void AbortCompileJobsOnIsolate(Isolate*);

  void TearDown();

 private:
  AsyncCompileJob* CreateAsyncCompileJob(
      Isolate* isolate, std::unique_ptr<byte[]> bytes_copy, size_t length,
      Handle<Context> context,
      std::unique_ptr<CompilationResultResolver> resolver);

  // We use an AsyncCompileJob as the key for itself so that we can delete the
  // job from the map when it is finished.
  std::unordered_map<AsyncCompileJob*, std::unique_ptr<AsyncCompileJob>> jobs_;
  std::unique_ptr<WasmCodeManager> code_manager_;
  WasmMemoryTracker memory_tracker_;
  AccountingAllocator allocator_;

  // Contains all CancelableTaskManagers that run tasks that are dependent
  // on the isolate.
  std::list<CancelableTaskManager*> task_managers_;

  // This mutex protects all information which is mutated concurrently or
  // fields that are initialized lazily on the first access.
  base::Mutex mutex_;

  //////////////////////////////////////////////////////////////////////////////
  // Protected by {mutex_}:

  std::unique_ptr<CompilationStatistics> compilation_stats_;
  std::unique_ptr<CodeTracer> code_tracer_;

  // End of fields protected by {mutex_}.
  //////////////////////////////////////////////////////////////////////////////

  DISALLOW_COPY_AND_ASSIGN(WasmEngine);
};

}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif  // V8_WASM_WASM_ENGINE_H_
