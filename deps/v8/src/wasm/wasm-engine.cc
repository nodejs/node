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
// A task to log a set of {WasmCode} objects in an isolate. It does not own any
// data itself, since it is owned by the platform, so lifetime is not really
// bound to the wasm engine.
class LogCodesTask : public Task {
 public:
  LogCodesTask(base::Mutex* mutex, LogCodesTask** task_slot, Isolate* isolate,
               WasmEngine* engine)
      : mutex_(mutex),
        task_slot_(task_slot),
        isolate_(isolate),
        engine_(engine) {
    DCHECK_NOT_NULL(task_slot);
    DCHECK_NOT_NULL(isolate);
  }

  ~LogCodesTask() {
    // If the platform deletes this task before executing it, we also deregister
    // it to avoid use-after-free from still-running background threads.
    if (!cancelled()) DeregisterTask();
  }

  void Run() override {
    if (cancelled()) return;
    DeregisterTask();
    engine_->LogOutstandingCodesForIsolate(isolate_);
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
  WasmEngine* const engine_;
};

class WasmGCForegroundTask : public Task {
 public:
  explicit WasmGCForegroundTask(Isolate* isolate) : isolate_(isolate) {
    DCHECK_NOT_NULL(isolate);
  }

  void Run() final {
    if (isolate_ == nullptr) return;  // cancelled.
    WasmEngine* engine = isolate_->wasm_engine();
    // If the foreground task is executing, there is no wasm code active. Just
    // report an empty set of live wasm code.
    engine->ReportLiveCodeForGC(isolate_, Vector<WasmCode*>{});
  }

  void Cancel() { isolate_ = nullptr; }

 private:
  Isolate* isolate_;
};

}  // namespace

struct WasmEngine::CurrentGCInfo {
  // Set of isolates that did not scan their stack yet for used WasmCode, and
  // their scheduled foreground task.
  std::unordered_map<Isolate*, WasmGCForegroundTask*> outstanding_isolates;

  // Set of dead code. Filled with all potentially dead code on initialization.
  // Code that is still in-use is removed by the individual isolates.
  std::unordered_set<WasmCode*> dead_code;
};

struct WasmEngine::IsolateInfo {
  explicit IsolateInfo(Isolate* isolate)
      : log_codes(WasmCode::ShouldBeLogged(isolate)) {
    v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);
    v8::Platform* platform = V8::GetCurrentPlatform();
    foreground_task_runner = platform->GetForegroundTaskRunner(v8_isolate);
  }

#ifdef DEBUG
  ~IsolateInfo() {
    // Before destructing, the {WasmEngine} must have cleared outstanding code
    // to log.
    DCHECK_EQ(0, code_to_log.size());
  }
#endif

  // All native modules that are being used by this Isolate (currently only
  // grows, never shrinks).
  std::set<NativeModule*> native_modules;

  // Caches whether code needs to be logged on this isolate.
  bool log_codes;

  // The currently scheduled LogCodesTask.
  LogCodesTask* log_codes_task = nullptr;

  // The vector of code objects that still need to be logged in this isolate.
  std::vector<WasmCode*> code_to_log;

  // The foreground task runner of the isolate (can be called from background).
  std::shared_ptr<v8::TaskRunner> foreground_task_runner;
};

struct WasmEngine::NativeModuleInfo {
  // Set of isolates using this NativeModule.
  std::unordered_set<Isolate*> isolates;

  // Set of potentially dead code. The ref-count of these code objects was
  // incremented for each Isolate that might still execute the code, and is
  // decremented on {RemoveIsolate} or on a GC.
  std::unordered_set<WasmCode*> potentially_dead_code;
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
  DCHECK(native_modules_.empty());
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
  if (result.failed()) {
    // This happens once in a while when we have missed some limit check
    // in the asm parser. Output an error message to help diagnose, but crash.
    std::cout << result.error().message();
    UNREACHABLE();
  }

  // Transfer ownership of the WasmModule to the {Managed<WasmModule>} generated
  // in {CompileToNativeModule}.
  Handle<FixedArray> export_wrappers;
  std::shared_ptr<NativeModule> native_module =
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
  std::shared_ptr<NativeModule> native_module =
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
    DCHECK_EQ(1, native_modules_.count(native_module));
    native_modules_[native_module]->isolates.insert(isolate);
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
    Counters* counters = isolate->counters();
    WasmEngine* engine = isolate->wasm_engine();
    base::MutexGuard lock(&engine->mutex_);
    DCHECK_EQ(1, engine->isolates_.count(isolate));
    for (auto* native_module : engine->isolates_[isolate]->native_modules) {
      native_module->SampleCodeSize(counters, NativeModule::kSampling);
    }
  };
  isolate->heap()->AddGCEpilogueCallback(callback, v8::kGCTypeMarkSweepCompact,
                                         nullptr);
}

void WasmEngine::RemoveIsolate(Isolate* isolate) {
  base::MutexGuard guard(&mutex_);
  auto it = isolates_.find(isolate);
  DCHECK_NE(isolates_.end(), it);
  std::unique_ptr<IsolateInfo> info = std::move(it->second);
  isolates_.erase(it);
  for (NativeModule* native_module : info->native_modules) {
    DCHECK_EQ(1, native_modules_.count(native_module));
    DCHECK_EQ(1, native_modules_[native_module]->isolates.count(isolate));
    auto* info = native_modules_[native_module].get();
    info->isolates.erase(isolate);
    if (current_gc_info_) {
      auto it = current_gc_info_->outstanding_isolates.find(isolate);
      if (it != current_gc_info_->outstanding_isolates.end()) {
        if (auto* gc_task = it->second) gc_task->Cancel();
        current_gc_info_->outstanding_isolates.erase(it);
      }
      for (WasmCode* code : info->potentially_dead_code) {
        current_gc_info_->dead_code.erase(code);
      }
    }
  }
  if (auto* task = info->log_codes_task) task->Cancel();
  if (!info->code_to_log.empty()) {
    WasmCode::DecrementRefCount(VectorOf(info->code_to_log));
    info->code_to_log.clear();
  }
}

void WasmEngine::LogCode(WasmCode* code) {
  base::MutexGuard guard(&mutex_);
  NativeModule* native_module = code->native_module();
  DCHECK_EQ(1, native_modules_.count(native_module));
  for (Isolate* isolate : native_modules_[native_module]->isolates) {
    DCHECK_EQ(1, isolates_.count(isolate));
    IsolateInfo* info = isolates_[isolate].get();
    if (info->log_codes == false) continue;
    if (info->log_codes_task == nullptr) {
      auto new_task = base::make_unique<LogCodesTask>(
          &mutex_, &info->log_codes_task, isolate, this);
      info->log_codes_task = new_task.get();
      info->foreground_task_runner->PostTask(std::move(new_task));
      isolate->stack_guard()->RequestLogWasmCode();
    }
    info->code_to_log.push_back(code);
    code->IncRef();
  }
}

void WasmEngine::EnableCodeLogging(Isolate* isolate) {
  base::MutexGuard guard(&mutex_);
  auto it = isolates_.find(isolate);
  DCHECK_NE(isolates_.end(), it);
  it->second->log_codes = true;
}

void WasmEngine::LogOutstandingCodesForIsolate(Isolate* isolate) {
  // If by now we should not log code any more, do not log it.
  if (!WasmCode::ShouldBeLogged(isolate)) return;

  // Under the mutex, get the vector of wasm code to log. Then log and decrement
  // the ref count without holding the mutex.
  std::vector<WasmCode*> code_to_log;
  {
    base::MutexGuard guard(&mutex_);
    DCHECK_EQ(1, isolates_.count(isolate));
    code_to_log.swap(isolates_[isolate]->code_to_log);
  }
  if (code_to_log.empty()) return;
  for (WasmCode* code : code_to_log) {
    code->LogCode(isolate);
  }
  WasmCode::DecrementRefCount(VectorOf(code_to_log));
}

std::shared_ptr<NativeModule> WasmEngine::NewNativeModule(
    Isolate* isolate, const WasmFeatures& enabled, size_t code_size_estimate,
    bool can_request_more, std::shared_ptr<const WasmModule> module) {
  std::shared_ptr<NativeModule> native_module =
      code_manager_.NewNativeModule(this, isolate, enabled, code_size_estimate,
                                    can_request_more, std::move(module));
  base::MutexGuard lock(&mutex_);
  auto pair = native_modules_.insert(std::make_pair(
      native_module.get(), base::make_unique<NativeModuleInfo>()));
  DCHECK(pair.second);  // inserted new entry.
  pair.first->second.get()->isolates.insert(isolate);
  isolates_[isolate]->native_modules.insert(native_module.get());
  return native_module;
}

void WasmEngine::FreeNativeModule(NativeModule* native_module) {
  {
    base::MutexGuard guard(&mutex_);
    auto it = native_modules_.find(native_module);
    DCHECK_NE(native_modules_.end(), it);
    for (Isolate* isolate : it->second->isolates) {
      DCHECK_EQ(1, isolates_.count(isolate));
      IsolateInfo* info = isolates_[isolate].get();
      DCHECK_EQ(1, info->native_modules.count(native_module));
      info->native_modules.erase(native_module);
      // If there are {WasmCode} objects of the deleted {NativeModule}
      // outstanding to be logged in this isolate, remove them. Decrementing the
      // ref count is not needed, since the {NativeModule} dies anyway.
      size_t remaining = info->code_to_log.size();
      if (remaining > 0) {
        for (size_t i = 0; i < remaining; ++i) {
          while (i < remaining &&
                 info->code_to_log[i]->native_module() == native_module) {
            // Move the last remaining item to this slot (this can be the same
            // as {i}, which is OK).
            info->code_to_log[i] = info->code_to_log[--remaining];
          }
        }
        info->code_to_log.resize(remaining);
      }
    }
    native_modules_.erase(it);
  }
  code_manager_.FreeNativeModule(native_module);
}

namespace {
class SampleTopTierCodeSizeTask : public CancelableTask {
 public:
  SampleTopTierCodeSizeTask(Isolate* isolate,
                            std::weak_ptr<NativeModule> native_module)
      : CancelableTask(isolate),
        isolate_(isolate),
        native_module_(std::move(native_module)) {}

  void RunInternal() override {
    if (std::shared_ptr<NativeModule> native_module = native_module_.lock()) {
      native_module->SampleCodeSize(isolate_->counters(),
                                    NativeModule::kAfterTopTier);
    }
  }

 private:
  Isolate* const isolate_;
  const std::weak_ptr<NativeModule> native_module_;
};
}  // namespace

void WasmEngine::SampleTopTierCodeSizeInAllIsolates(
    const std::shared_ptr<NativeModule>& native_module) {
  base::MutexGuard lock(&mutex_);
  DCHECK_EQ(1, native_modules_.count(native_module.get()));
  for (Isolate* isolate : native_modules_[native_module.get()]->isolates) {
    DCHECK_EQ(1, isolates_.count(isolate));
    IsolateInfo* info = isolates_[isolate].get();
    info->foreground_task_runner->PostTask(
        base::make_unique<SampleTopTierCodeSizeTask>(isolate, native_module));
  }
}

void WasmEngine::ReportLiveCodeForGC(Isolate* isolate,
                                     Vector<WasmCode*> live_code) {
  base::MutexGuard guard(&mutex_);
  DCHECK_NOT_NULL(current_gc_info_);
  auto outstanding_isolate_it =
      current_gc_info_->outstanding_isolates.find(isolate);
  DCHECK_NE(current_gc_info_->outstanding_isolates.end(),
            outstanding_isolate_it);
  auto* fg_task = outstanding_isolate_it->second;
  if (fg_task) fg_task->Cancel();
  current_gc_info_->outstanding_isolates.erase(outstanding_isolate_it);
  for (WasmCode* code : live_code) current_gc_info_->dead_code.erase(code);

  if (current_gc_info_->outstanding_isolates.empty()) {
    std::unordered_map<NativeModule*, std::vector<WasmCode*>>
        dead_code_per_native_module;
    for (WasmCode* code : current_gc_info_->dead_code) {
      dead_code_per_native_module[code->native_module()].push_back(code);
    }
    for (auto& entry : dead_code_per_native_module) {
      entry.first->FreeCode(VectorOf(entry.second));
    }
    current_gc_info_.reset();
  }
}

bool WasmEngine::AddPotentiallyDeadCode(WasmCode* code) {
  base::MutexGuard guard(&mutex_);
  auto it = native_modules_.find(code->native_module());
  DCHECK_NE(native_modules_.end(), it);
  auto added = it->second->potentially_dead_code.insert(code);
  if (!added.second) return false;  // An entry already existed.
  new_potentially_dead_code_size_ += code->instructions().size();
  // Trigger a GC if 1MiB plus 10% of committed code are potentially dead.
  size_t dead_code_limit = 1 * MB + code_manager_.committed_code_space() / 10;
  if (FLAG_wasm_code_gc && new_potentially_dead_code_size_ > dead_code_limit &&
      !current_gc_info_) {
    TriggerGC();
  }
  return true;
}

void WasmEngine::TriggerGC() {
  DCHECK_NULL(current_gc_info_);
  DCHECK(FLAG_wasm_code_gc);
  current_gc_info_.reset(new CurrentGCInfo());
  // Add all potentially dead code to this GC, and trigger a GC task in each
  // isolate.
  // TODO(clemensh): Also trigger a stack check interrupt.
  for (auto& entry : native_modules_) {
    NativeModuleInfo* info = entry.second.get();
    if (info->potentially_dead_code.empty()) continue;
    for (auto* isolate : native_modules_[entry.first]->isolates) {
      auto& gc_task = current_gc_info_->outstanding_isolates[isolate];
      if (!gc_task) {
        auto new_task = base::make_unique<WasmGCForegroundTask>(isolate);
        gc_task = new_task.get();
        DCHECK_EQ(1, isolates_.count(isolate));
        isolates_[isolate]->foreground_task_runner->PostTask(
            std::move(new_task));
      }
    }
    for (WasmCode* code : info->potentially_dead_code) {
      current_gc_info_->dead_code.insert(code);
    }
  }
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
