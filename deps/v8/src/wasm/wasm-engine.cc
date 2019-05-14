// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/wasm/wasm-engine.h"

#include "src/code-tracer.h"
#include "src/compilation-statistics.h"
#include "src/counters.h"
#include "src/objects-inl.h"
#include "src/objects/heap-number.h"
#include "src/objects/js-promise.h"
#include "src/ostreams.h"
#include "src/wasm/function-compiler.h"
#include "src/wasm/module-compiler.h"
#include "src/wasm/module-decoder.h"
#include "src/wasm/module-instantiate.h"
#include "src/wasm/streaming-decoder.h"
#include "src/wasm/wasm-objects-inl.h"

namespace v8 {
namespace internal {
namespace wasm {

namespace {
class LogCodesTask : public Task {
 public:
  LogCodesTask(base::Mutex* mutex, LogCodesTask** task_slot, Isolate* isolate)
      : mutex_(mutex), task_slot_(task_slot), isolate_(isolate) {
    DCHECK_NOT_NULL(task_slot);
    DCHECK_NOT_NULL(isolate);
  }

  ~LogCodesTask() {
    // If the platform deletes this task before executing it, we also deregister
    // it to avoid use-after-free from still-running background threads.
    if (!cancelled()) DeregisterTask();
  }

  // Hold the {mutex_} when calling this method.
  void AddCode(WasmCode* code) { code_to_log_.push_back(code); }

  void Run() override {
    if (cancelled()) return;
    DeregisterTask();
    // If by now we should not log code any more, do not log it.
    if (!WasmCode::ShouldBeLogged(isolate_)) return;
    for (WasmCode* code : code_to_log_) {
      code->LogCode(isolate_);
    }
  }

  void Cancel() {
    // Cancel will only be called on Isolate shutdown, which happens on the
    // Isolate's foreground thread. Thus no synchronization needed.
    isolate_ = nullptr;
  }

  bool cancelled() const { return isolate_ == nullptr; }

  void DeregisterTask() {
    // The task will only be deregistered from the foreground thread (executing
    // this task or calling its destructor), thus we do not need synchronization
    // on this field access.
    if (task_slot_ == nullptr) return;  // already deregistered.
    // Remove this task from the {IsolateInfo} in the engine. The next
    // logging request will allocate and schedule a new task.
    base::MutexGuard guard(mutex_);
    DCHECK_EQ(this, *task_slot_);
    *task_slot_ = nullptr;
    task_slot_ = nullptr;
  }

 private:
  // The mutex of the WasmEngine.
  base::Mutex* const mutex_;
  // The slot in the WasmEngine where this LogCodesTask is stored. This is
  // cleared by this task before execution or on task destruction.
  LogCodesTask** task_slot_;
  Isolate* isolate_;
  std::vector<WasmCode*> code_to_log_;
};
}  // namespace

struct WasmEngine::IsolateInfo {
  explicit IsolateInfo(Isolate* isolate)
      : log_codes(WasmCode::ShouldBeLogged(isolate)) {
    v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);
    v8::Platform* platform = V8::GetCurrentPlatform();
    foreground_task_runner = platform->GetForegroundTaskRunner(v8_isolate);
  }

  // All native modules that are being used by this Isolate (currently only
  // grows, never shrinks).
  std::set<NativeModule*> native_modules;

  // Caches whether code needs to be logged on this isolate.
  bool log_codes;

  // The currently scheduled LogCodesTask.
  LogCodesTask* log_codes_task = nullptr;

  // The foreground task runner of the isolate (can be called from background).
  std::shared_ptr<v8::TaskRunner> foreground_task_runner;
};

WasmEngine::WasmEngine()
    : code_manager_(&memory_tracker_, FLAG_wasm_max_code_space * MB) {}

WasmEngine::~WasmEngine() {
  // Synchronize on all background compile tasks.
  background_compile_task_manager_.CancelAndWait();
  // All AsyncCompileJobs have been canceled.
  DCHECK(async_compile_jobs_.empty());
  // All Isolates have been deregistered.
  DCHECK(isolates_.empty());
  // All NativeModules did die.
  DCHECK(isolates_per_native_module_.empty());
}

bool WasmEngine::SyncValidate(Isolate* isolate, const WasmFeatures& enabled,
                              const ModuleWireBytes& bytes) {
  // TODO(titzer): remove dependency on the isolate.
  if (bytes.start() == nullptr || bytes.length() == 0) return false;
  ModuleResult result =
      DecodeWasmModule(enabled, bytes.start(), bytes.end(), true, kWasmOrigin,
                       isolate->counters(), allocator());
  return result.ok();
}

MaybeHandle<AsmWasmData> WasmEngine::SyncCompileTranslatedAsmJs(
    Isolate* isolate, ErrorThrower* thrower, const ModuleWireBytes& bytes,
    Vector<const byte> asm_js_offset_table_bytes,
    Handle<HeapNumber> uses_bitset) {
  ModuleResult result =
      DecodeWasmModule(kAsmjsWasmFeatures, bytes.start(), bytes.end(), false,
                       kAsmJsOrigin, isolate->counters(), allocator());
  CHECK(!result.failed());

  // Transfer ownership of the WasmModule to the {Managed<WasmModule>} generated
  // in {CompileToNativeModule}.
  Handle<FixedArray> export_wrappers;
  std::unique_ptr<NativeModule> native_module =
      CompileToNativeModule(isolate, kAsmjsWasmFeatures, thrower,
                            std::move(result).value(), bytes, &export_wrappers);
  if (!native_module) return {};

  // Create heap objects for asm.js offset table to be stored in the module
  // object.
  Handle<ByteArray> asm_js_offset_table =
      isolate->factory()->NewByteArray(asm_js_offset_table_bytes.length());
  asm_js_offset_table->copy_in(0, asm_js_offset_table_bytes.start(),
                               asm_js_offset_table_bytes.length());

  return AsmWasmData::New(isolate, std::move(native_module), export_wrappers,
                          asm_js_offset_table, uses_bitset);
}

Handle<WasmModuleObject> WasmEngine::FinalizeTranslatedAsmJs(
    Isolate* isolate, Handle<AsmWasmData> asm_wasm_data,
    Handle<Script> script) {
  std::shared_ptr<NativeModule> native_module =
      asm_wasm_data->managed_native_module()->get();
  Handle<FixedArray> export_wrappers =
      handle(asm_wasm_data->export_wrappers(), isolate);
  size_t code_size_estimate =
      wasm::WasmCodeManager::EstimateNativeModuleCodeSize(
          native_module->module());

  Handle<WasmModuleObject> module_object =
      WasmModuleObject::New(isolate, std::move(native_module), script,
                            export_wrappers, code_size_estimate);
  module_object->set_asm_js_offset_table(asm_wasm_data->asm_js_offset_table());
  return module_object;
}

MaybeHandle<WasmModuleObject> WasmEngine::SyncCompile(
    Isolate* isolate, const WasmFeatures& enabled, ErrorThrower* thrower,
    const ModuleWireBytes& bytes) {
  ModuleResult result =
      DecodeWasmModule(enabled, bytes.start(), bytes.end(), false, kWasmOrigin,
                       isolate->counters(), allocator());
  if (result.failed()) {
    thrower->CompileFailed(result.error());
    return {};
  }

  // Transfer ownership of the WasmModule to the {Managed<WasmModule>} generated
  // in {CompileToModuleObject}.
  Handle<FixedArray> export_wrappers;
  std::unique_ptr<NativeModule> native_module =
      CompileToNativeModule(isolate, enabled, thrower,
                            std::move(result).value(), bytes, &export_wrappers);
  if (!native_module) return {};

  Handle<Script> script =
      CreateWasmScript(isolate, bytes, native_module->module()->source_map_url);
  size_t code_size_estimate =
      wasm::WasmCodeManager::EstimateNativeModuleCodeSize(
          native_module->module());

  // Create the module object.
  // TODO(clemensh): For the same module (same bytes / same hash), we should
  // only have one WasmModuleObject. Otherwise, we might only set
  // breakpoints on a (potentially empty) subset of the instances.

  // Create the compiled module object and populate with compiled functions
  // and information needed at instantiation time. This object needs to be
  // serializable. Instantiation may occur off a deserialized version of this
  // object.
  Handle<WasmModuleObject> module_object =
      WasmModuleObject::New(isolate, std::move(native_module), script,
                            export_wrappers, code_size_estimate);

  // Finish the Wasm script now and make it public to the debugger.
  isolate->debug()->OnAfterCompile(script);
  return module_object;
}

MaybeHandle<WasmInstanceObject> WasmEngine::SyncInstantiate(
    Isolate* isolate, ErrorThrower* thrower,
    Handle<WasmModuleObject> module_object, MaybeHandle<JSReceiver> imports,
    MaybeHandle<JSArrayBuffer> memory) {
  return InstantiateToInstanceObject(isolate, thrower, module_object, imports,
                                     memory);
}

void WasmEngine::AsyncInstantiate(
    Isolate* isolate, std::unique_ptr<InstantiationResultResolver> resolver,
    Handle<WasmModuleObject> module_object, MaybeHandle<JSReceiver> imports) {
  ErrorThrower thrower(isolate, "WebAssembly.instantiate()");
  // Instantiate a TryCatch so that caught exceptions won't progagate out.
  // They will still be set as pending exceptions on the isolate.
  // TODO(clemensh): Avoid TryCatch, use Execution::TryCall internally to invoke
  // start function and report thrown exception explicitly via out argument.
  v8::TryCatch catcher(reinterpret_cast<v8::Isolate*>(isolate));
  catcher.SetVerbose(false);
  catcher.SetCaptureMessage(false);

  MaybeHandle<WasmInstanceObject> instance_object = SyncInstantiate(
      isolate, &thrower, module_object, imports, Handle<JSArrayBuffer>::null());

  if (!instance_object.is_null()) {
    resolver->OnInstantiationSucceeded(instance_object.ToHandleChecked());
    return;
  }

  if (isolate->has_pending_exception()) {
    // The JS code executed during instantiation has thrown an exception.
    // We have to move the exception to the promise chain.
    Handle<Object> exception(isolate->pending_exception(), isolate);
    isolate->clear_pending_exception();
    DCHECK(*isolate->external_caught_exception_address());
    *isolate->external_caught_exception_address() = false;
    resolver->OnInstantiationFailed(exception);
    thrower.Reset();
  } else {
    DCHECK(thrower.error());
    resolver->OnInstantiationFailed(thrower.Reify());
  }
}

void WasmEngine::AsyncCompile(
    Isolate* isolate, const WasmFeatures& enabled,
    std::shared_ptr<CompilationResultResolver> resolver,
    const ModuleWireBytes& bytes, bool is_shared) {
  if (!FLAG_wasm_async_compilation) {
    // Asynchronous compilation disabled; fall back on synchronous compilation.
    ErrorThrower thrower(isolate, "WasmCompile");
    MaybeHandle<WasmModuleObject> module_object;
    if (is_shared) {
      // Make a copy of the wire bytes to avoid concurrent modification.
      std::unique_ptr<uint8_t[]> copy(new uint8_t[bytes.length()]);
      memcpy(copy.get(), bytes.start(), bytes.length());
      ModuleWireBytes bytes_copy(copy.get(), copy.get() + bytes.length());
      module_object = SyncCompile(isolate, enabled, &thrower, bytes_copy);
    } else {
      // The wire bytes are not shared, OK to use them directly.
      module_object = SyncCompile(isolate, enabled, &thrower, bytes);
    }
    if (thrower.error()) {
      resolver->OnCompilationFailed(thrower.Reify());
      return;
    }
    Handle<WasmModuleObject> module = module_object.ToHandleChecked();
    resolver->OnCompilationSucceeded(module);
    return;
  }

  if (FLAG_wasm_test_streaming) {
    std::shared_ptr<StreamingDecoder> streaming_decoder =
        StartStreamingCompilation(isolate, enabled,
                                  handle(isolate->context(), isolate),
                                  std::move(resolver));
    streaming_decoder->OnBytesReceived(bytes.module_bytes());
    streaming_decoder->Finish();
    return;
  }
  // Make a copy of the wire bytes in case the user program changes them
  // during asynchronous compilation.
  std::unique_ptr<byte[]> copy(new byte[bytes.length()]);
  memcpy(copy.get(), bytes.start(), bytes.length());

  AsyncCompileJob* job = CreateAsyncCompileJob(
      isolate, enabled, std::move(copy), bytes.length(),
      handle(isolate->context(), isolate), std::move(resolver));
  job->Start();
}

std::shared_ptr<StreamingDecoder> WasmEngine::StartStreamingCompilation(
    Isolate* isolate, const WasmFeatures& enabled, Handle<Context> context,
    std::shared_ptr<CompilationResultResolver> resolver) {
  AsyncCompileJob* job =
      CreateAsyncCompileJob(isolate, enabled, std::unique_ptr<byte[]>(nullptr),
                            0, context, std::move(resolver));
  return job->CreateStreamingDecoder();
}

void WasmEngine::CompileFunction(Isolate* isolate, NativeModule* native_module,
                                 uint32_t function_index, ExecutionTier tier) {
  // Note we assume that "one-off" compilations can discard detected features.
  WasmFeatures detected = kNoWasmFeatures;
  WasmCompilationUnit::CompileWasmFunction(
      isolate, native_module, &detected,
      &native_module->module()->functions[function_index], tier);
}

std::shared_ptr<NativeModule> WasmEngine::ExportNativeModule(
    Handle<WasmModuleObject> module_object) {
  return module_object->shared_native_module();
}

Handle<WasmModuleObject> WasmEngine::ImportNativeModule(
    Isolate* isolate, std::shared_ptr<NativeModule> shared_native_module) {
  NativeModule* native_module = shared_native_module.get();
  ModuleWireBytes wire_bytes(native_module->wire_bytes());
  const WasmModule* module = native_module->module();
  Handle<Script> script =
      CreateWasmScript(isolate, wire_bytes, module->source_map_url);
  size_t code_size = native_module->committed_code_space();
  Handle<WasmModuleObject> module_object = WasmModuleObject::New(
      isolate, std::move(shared_native_module), script, code_size);
  CompileJsToWasmWrappers(isolate, native_module->module(),
                          handle(module_object->export_wrappers(), isolate));
  {
    base::MutexGuard lock(&mutex_);
    DCHECK_EQ(1, isolates_.count(isolate));
    isolates_[isolate]->native_modules.insert(native_module);
    DCHECK_EQ(1, isolates_per_native_module_.count(native_module));
    isolates_per_native_module_[native_module].insert(isolate);
  }
  return module_object;
}

CompilationStatistics* WasmEngine::GetOrCreateTurboStatistics() {
  base::MutexGuard guard(&mutex_);
  if (compilation_stats_ == nullptr) {
    compilation_stats_.reset(new CompilationStatistics());
  }
  return compilation_stats_.get();
}

void WasmEngine::DumpAndResetTurboStatistics() {
  base::MutexGuard guard(&mutex_);
  if (compilation_stats_ != nullptr) {
    StdoutStream os;
    os << AsPrintableStatistics{*compilation_stats_.get(), false} << std::endl;
  }
  compilation_stats_.reset();
}

CodeTracer* WasmEngine::GetCodeTracer() {
  base::MutexGuard guard(&mutex_);
  if (code_tracer_ == nullptr) code_tracer_.reset(new CodeTracer(-1));
  return code_tracer_.get();
}

AsyncCompileJob* WasmEngine::CreateAsyncCompileJob(
    Isolate* isolate, const WasmFeatures& enabled,
    std::unique_ptr<byte[]> bytes_copy, size_t length, Handle<Context> context,
    std::shared_ptr<CompilationResultResolver> resolver) {
  AsyncCompileJob* job =
      new AsyncCompileJob(isolate, enabled, std::move(bytes_copy), length,
                          context, std::move(resolver));
  // Pass ownership to the unique_ptr in {async_compile_jobs_}.
  base::MutexGuard guard(&mutex_);
  async_compile_jobs_[job] = std::unique_ptr<AsyncCompileJob>(job);
  return job;
}

std::unique_ptr<AsyncCompileJob> WasmEngine::RemoveCompileJob(
    AsyncCompileJob* job) {
  base::MutexGuard guard(&mutex_);
  auto item = async_compile_jobs_.find(job);
  DCHECK(item != async_compile_jobs_.end());
  std::unique_ptr<AsyncCompileJob> result = std::move(item->second);
  async_compile_jobs_.erase(item);
  return result;
}

bool WasmEngine::HasRunningCompileJob(Isolate* isolate) {
  base::MutexGuard guard(&mutex_);
  DCHECK_EQ(1, isolates_.count(isolate));
  for (auto& entry : async_compile_jobs_) {
    if (entry.first->isolate() == isolate) return true;
  }
  return false;
}

void WasmEngine::DeleteCompileJobsOnIsolate(Isolate* isolate) {
  // Under the mutex get all jobs to delete. Then delete them without holding
  // the mutex, such that deletion can reenter the WasmEngine.
  std::vector<std::unique_ptr<AsyncCompileJob>> jobs_to_delete;
  {
    base::MutexGuard guard(&mutex_);
    DCHECK_EQ(1, isolates_.count(isolate));
    for (auto it = async_compile_jobs_.begin();
         it != async_compile_jobs_.end();) {
      if (it->first->isolate() != isolate) {
        ++it;
        continue;
      }
      jobs_to_delete.push_back(std::move(it->second));
      it = async_compile_jobs_.erase(it);
    }
  }
}

void WasmEngine::AddIsolate(Isolate* isolate) {
  base::MutexGuard guard(&mutex_);
  DCHECK_EQ(0, isolates_.count(isolate));
  isolates_.emplace(isolate, base::make_unique<IsolateInfo>(isolate));

  // Install sampling GC callback.
  // TODO(v8:7424): For now we sample module sizes in a GC callback. This will
  // bias samples towards apps with high memory pressure. We should switch to
  // using sampling based on regular intervals independent of the GC.
  auto callback = [](v8::Isolate* v8_isolate, v8::GCType type,
                     v8::GCCallbackFlags flags, void* data) {
    Isolate* isolate = reinterpret_cast<Isolate*>(v8_isolate);
    WasmEngine* engine = isolate->wasm_engine();
    base::MutexGuard lock(&engine->mutex_);
    DCHECK_EQ(1, engine->isolates_.count(isolate));
    for (NativeModule* native_module :
         engine->isolates_[isolate]->native_modules) {
      int code_size =
          static_cast<int>(native_module->committed_code_space() / MB);
      isolate->counters()->wasm_module_code_size_mb()->AddSample(code_size);
    }
  };
  isolate->heap()->AddGCEpilogueCallback(callback, v8::kGCTypeMarkSweepCompact,
                                         nullptr);
}

void WasmEngine::RemoveIsolate(Isolate* isolate) {
  base::MutexGuard guard(&mutex_);
  auto it = isolates_.find(isolate);
  DCHECK_NE(isolates_.end(), it);
  for (NativeModule* native_module : it->second->native_modules) {
    DCHECK_EQ(1, isolates_per_native_module_[native_module].count(isolate));
    isolates_per_native_module_[native_module].erase(isolate);
  }
  if (auto* task = it->second->log_codes_task) task->Cancel();
  isolates_.erase(it);
}

void WasmEngine::LogCode(WasmCode* code) {
  base::MutexGuard guard(&mutex_);
  NativeModule* native_module = code->native_module();
  DCHECK_EQ(1, isolates_per_native_module_.count(native_module));
  for (Isolate* isolate : isolates_per_native_module_[native_module]) {
    DCHECK_EQ(1, isolates_.count(isolate));
    IsolateInfo* info = isolates_[isolate].get();
    if (info->log_codes == false) continue;
    if (info->log_codes_task == nullptr) {
      auto new_task = base::make_unique<LogCodesTask>(
          &mutex_, &info->log_codes_task, isolate);
      info->log_codes_task = new_task.get();
      info->foreground_task_runner->PostTask(std::move(new_task));
    }
    info->log_codes_task->AddCode(code);
  }
}

void WasmEngine::EnableCodeLogging(Isolate* isolate) {
  base::MutexGuard guard(&mutex_);
  auto it = isolates_.find(isolate);
  DCHECK_NE(isolates_.end(), it);
  it->second->log_codes = true;
}

std::unique_ptr<NativeModule> WasmEngine::NewNativeModule(
    Isolate* isolate, const WasmFeatures& enabled, size_t code_size_estimate,
    bool can_request_more, std::shared_ptr<const WasmModule> module) {
  std::unique_ptr<NativeModule> native_module =
      code_manager_.NewNativeModule(this, isolate, enabled, code_size_estimate,
                                    can_request_more, std::move(module));
  base::MutexGuard lock(&mutex_);
  isolates_per_native_module_[native_module.get()].insert(isolate);
  DCHECK_EQ(1, isolates_.count(isolate));
  isolates_[isolate]->native_modules.insert(native_module.get());
  return native_module;
}

void WasmEngine::FreeNativeModule(NativeModule* native_module) {
  {
    base::MutexGuard guard(&mutex_);
    auto it = isolates_per_native_module_.find(native_module);
    DCHECK_NE(isolates_per_native_module_.end(), it);
    for (Isolate* isolate : it->second) {
      DCHECK_EQ(1, isolates_.count(isolate));
      DCHECK_EQ(1, isolates_[isolate]->native_modules.count(native_module));
      isolates_[isolate]->native_modules.erase(native_module);
    }
    isolates_per_native_module_.erase(it);
  }
  code_manager_.FreeNativeModule(native_module);
}

namespace {

DEFINE_LAZY_LEAKY_OBJECT_GETTER(std::shared_ptr<WasmEngine>,
                                GetSharedWasmEngine)

}  // namespace

// static
void WasmEngine::InitializeOncePerProcess() {
  if (!FLAG_wasm_shared_engine) return;
  *GetSharedWasmEngine() = std::make_shared<WasmEngine>();
}

// static
void WasmEngine::GlobalTearDown() {
  if (!FLAG_wasm_shared_engine) return;
  GetSharedWasmEngine()->reset();
}

// static
std::shared_ptr<WasmEngine> WasmEngine::GetWasmEngine() {
  if (FLAG_wasm_shared_engine) return *GetSharedWasmEngine();
  return std::make_shared<WasmEngine>();
}

// {max_mem_pages} is declared in wasm-limits.h.
uint32_t max_mem_pages() {
  STATIC_ASSERT(kV8MaxWasmMemoryPages <= kMaxUInt32);
  return std::min(uint32_t{kV8MaxWasmMemoryPages}, FLAG_wasm_max_mem_pages);
}

// {max_table_init_entries} is declared in wasm-limits.h.
uint32_t max_table_init_entries() {
  return std::min(uint32_t{kV8MaxWasmTableInitEntries},
                  FLAG_wasm_max_table_size);
}

}  // namespace wasm
}  // namespace internal
}  // namespace v8
