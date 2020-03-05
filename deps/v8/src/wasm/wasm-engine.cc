// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/wasm/wasm-engine.h"

#include "src/base/platform/time.h"
#include "src/diagnostics/code-tracer.h"
#include "src/diagnostics/compilation-statistics.h"
#include "src/execution/frames.h"
#include "src/execution/v8threads.h"
#include "src/logging/counters.h"
#include "src/objects/heap-number.h"
#include "src/objects/js-promise.h"
#include "src/objects/objects-inl.h"
#include "src/strings/string-hasher-inl.h"
#include "src/utils/ostreams.h"
#include "src/wasm/function-compiler.h"
#include "src/wasm/module-compiler.h"
#include "src/wasm/module-decoder.h"
#include "src/wasm/module-instantiate.h"
#include "src/wasm/streaming-decoder.h"
#include "src/wasm/wasm-objects-inl.h"

namespace v8 {
namespace internal {
namespace wasm {

#define TRACE_CODE_GC(...)                                         \
  do {                                                             \
    if (FLAG_trace_wasm_code_gc) PrintF("[wasm-gc] " __VA_ARGS__); \
  } while (false)

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

void CheckNoArchivedThreads(Isolate* isolate) {
  class ArchivedThreadsVisitor : public ThreadVisitor {
    void VisitThread(Isolate* isolate, ThreadLocalTop* top) override {
      // Archived threads are rarely used, and not combined with Wasm at the
      // moment. Implement this and test it properly once we have a use case for
      // that.
      FATAL("archived threads in combination with wasm not supported");
    }
  } archived_threads_visitor;
  isolate->thread_manager()->IterateArchivedThreads(&archived_threads_visitor);
}

class WasmGCForegroundTask : public CancelableTask {
 public:
  explicit WasmGCForegroundTask(Isolate* isolate)
      : CancelableTask(isolate->cancelable_task_manager()), isolate_(isolate) {}

  void RunInternal() final {
    WasmEngine* engine = isolate_->wasm_engine();
    // If the foreground task is executing, there is no wasm code active. Just
    // report an empty set of live wasm code.
#ifdef ENABLE_SLOW_DCHECKS
    for (StackFrameIterator it(isolate_); !it.done(); it.Advance()) {
      DCHECK_NE(StackFrame::WASM_COMPILED, it.frame()->type());
    }
#endif
    CheckNoArchivedThreads(isolate_);
    engine->ReportLiveCodeForGC(isolate_, Vector<WasmCode*>{});
  }

 private:
  Isolate* isolate_;
};

}  // namespace

std::shared_ptr<NativeModule> NativeModuleCache::MaybeGetNativeModule(
    ModuleOrigin origin, Vector<const uint8_t> wire_bytes) {
  if (origin != kWasmOrigin) return nullptr;
  base::MutexGuard lock(&mutex_);
  while (true) {
    auto it = map_.find(wire_bytes);
    if (it == map_.end()) {
      // Insert a {nullopt} entry to let other threads know that this
      // {NativeModule} is already being created on another thread.
      map_.emplace(wire_bytes, base::nullopt);
      return nullptr;
    }
    auto maybe_native_module = it->second;
    if (maybe_native_module.has_value()) {
      auto weak_ptr = maybe_native_module.value();
      if (auto shared_native_module = weak_ptr.lock()) {
        return shared_native_module;
      }
    }
    cache_cv_.Wait(&mutex_);
  }
}

void NativeModuleCache::Update(std::shared_ptr<NativeModule> native_module,
                               bool error) {
  DCHECK_NOT_NULL(native_module);
  if (native_module->module()->origin != kWasmOrigin) return;
  Vector<const uint8_t> wire_bytes = native_module->wire_bytes();
  base::MutexGuard lock(&mutex_);
  auto it = map_.find(wire_bytes);
  DCHECK_NE(it, map_.end());
  DCHECK(!it->second.has_value());
  // The lifetime of the temporary entry's bytes is unknown. Use the new native
  // module's owned copy of the bytes for the key instead.
  map_.erase(it);
  if (!error) {
    map_.emplace(wire_bytes, base::Optional<std::weak_ptr<NativeModule>>(
                                 std::move(native_module)));
  }
  cache_cv_.NotifyAll();
}

void NativeModuleCache::Erase(NativeModule* native_module) {
  base::MutexGuard lock(&mutex_);
  auto cache_it = map_.find(native_module->wire_bytes());
  // Not all native modules are stored in the cache currently. In particular
  // streaming compilation and asmjs compilation results are not. So make
  // sure that we only delete existing and expired entries.
  // Do not erase {nullopt} values either, as they indicate that the
  // {NativeModule} is currently being created in another thread.
  if (cache_it != map_.end() && cache_it->second.has_value() &&
      cache_it->second.value().expired()) {
    map_.erase(cache_it);
    cache_cv_.NotifyAll();
  }
}

size_t NativeModuleCache::WireBytesHasher::operator()(
    const Vector<const uint8_t>& bytes) const {
  return StringHasher::HashSequentialString(
      reinterpret_cast<const char*>(bytes.begin()), bytes.length(),
      kZeroHashSeed);
}

struct WasmEngine::CurrentGCInfo {
  explicit CurrentGCInfo(int8_t gc_sequence_index)
      : gc_sequence_index(gc_sequence_index) {
    DCHECK_NE(0, gc_sequence_index);
  }

  // Set of isolates that did not scan their stack yet for used WasmCode, and
  // their scheduled foreground task.
  std::unordered_map<Isolate*, WasmGCForegroundTask*> outstanding_isolates;

  // Set of dead code. Filled with all potentially dead code on initialization.
  // Code that is still in-use is removed by the individual isolates.
  std::unordered_set<WasmCode*> dead_code;

  // The number of GCs triggered in the native module that triggered this GC.
  // This is stored in the histogram for each participating isolate during
  // execution of that isolate's foreground task.
  const int8_t gc_sequence_index;

  // If during this GC, another GC was requested, we skipped that other GC (we
  // only run one GC at a time). Remember though to trigger another one once
  // this one finishes. {next_gc_sequence_index} is 0 if no next GC is needed,
  // and >0 otherwise. It stores the {num_code_gcs_triggered} of the native
  // module which triggered the next GC.
  int8_t next_gc_sequence_index = 0;

  // The start time of this GC; used for tracing and sampled via {Counters}.
  // Can be null ({TimeTicks::IsNull()}) if timer is not high resolution.
  base::TimeTicks start_time;
};

struct WasmEngine::IsolateInfo {
  explicit IsolateInfo(Isolate* isolate)
      : log_codes(WasmCode::ShouldBeLogged(isolate)),
        async_counters(isolate->async_counters()) {
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

  const std::shared_ptr<Counters> async_counters;
};

struct WasmEngine::NativeModuleInfo {
  // Set of isolates using this NativeModule.
  std::unordered_set<Isolate*> isolates;

  // Set of potentially dead code. This set holds one ref for each code object,
  // until code is detected to be really dead. At that point, the ref count is
  // decremented and code is move to the {dead_code} set. If the code is finally
  // deleted, it is also removed from {dead_code}.
  std::unordered_set<WasmCode*> potentially_dead_code;

  // Code that is not being executed in any isolate any more, but the ref count
  // did not drop to zero yet.
  std::unordered_set<WasmCode*> dead_code;

  // Number of code GCs triggered because code in this native module became
  // potentially dead.
  int8_t num_code_gcs_triggered = 0;
};

WasmEngine::WasmEngine() : code_manager_(FLAG_wasm_max_code_space * MB) {}

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
    Handle<HeapNumber> uses_bitset, LanguageMode language_mode) {
  ModuleOrigin origin = language_mode == LanguageMode::kSloppy
                            ? kAsmJsSloppyOrigin
                            : kAsmJsStrictOrigin;
  ModuleResult result =
      DecodeWasmModule(WasmFeatures::ForAsmjs(), bytes.start(), bytes.end(),
                       false, origin, isolate->counters(), allocator());
  if (result.failed()) {
    // This happens once in a while when we have missed some limit check
    // in the asm parser. Output an error message to help diagnose, but crash.
    std::cout << result.error().message();
    UNREACHABLE();
  }

  result.value()->asm_js_offset_information =
      std::make_unique<AsmJsOffsetInformation>(asm_js_offset_table_bytes);

  // Transfer ownership of the WasmModule to the {Managed<WasmModule>} generated
  // in {CompileToNativeModule}.
  Handle<FixedArray> export_wrappers;
  std::shared_ptr<NativeModule> native_module =
      CompileToNativeModule(isolate, WasmFeatures::ForAsmjs(), thrower,
                            std::move(result).value(), bytes, &export_wrappers);
  if (!native_module) return {};

  return AsmWasmData::New(isolate, std::move(native_module), export_wrappers,
                          uses_bitset);
}

Handle<WasmModuleObject> WasmEngine::FinalizeTranslatedAsmJs(
    Isolate* isolate, Handle<AsmWasmData> asm_wasm_data,
    Handle<Script> script) {
  std::shared_ptr<NativeModule> native_module =
      asm_wasm_data->managed_native_module().get();
  Handle<FixedArray> export_wrappers =
      handle(asm_wasm_data->export_wrappers(), isolate);
  Handle<WasmModuleObject> module_object = WasmModuleObject::New(
      isolate, std::move(native_module), script, export_wrappers);
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

  Handle<Script> script = CreateWasmScript(
      isolate, bytes, VectorOf(native_module->module()->source_map_url),
      native_module->module()->name);

  // Create the compiled module object and populate with compiled functions
  // and information needed at instantiation time. This object needs to be
  // serializable. Instantiation may occur off a deserialized version of this
  // object.
  Handle<WasmModuleObject> module_object = WasmModuleObject::New(
      isolate, std::move(native_module), script, export_wrappers);

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
  // TODO(clemensb): Avoid TryCatch, use Execution::TryCall internally to invoke
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
    const ModuleWireBytes& bytes, bool is_shared,
    const char* api_method_name_for_errors) {
  if (!FLAG_wasm_async_compilation) {
    // Asynchronous compilation disabled; fall back on synchronous compilation.
    ErrorThrower thrower(isolate, api_method_name_for_errors);
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
        StartStreamingCompilation(
            isolate, enabled, handle(isolate->context(), isolate),
            api_method_name_for_errors, std::move(resolver));
    streaming_decoder->OnBytesReceived(bytes.module_bytes());
    streaming_decoder->Finish();
    return;
  }
  // Make a copy of the wire bytes in case the user program changes them
  // during asynchronous compilation.
  std::unique_ptr<byte[]> copy(new byte[bytes.length()]);
  memcpy(copy.get(), bytes.start(), bytes.length());

  AsyncCompileJob* job =
      CreateAsyncCompileJob(isolate, enabled, std::move(copy), bytes.length(),
                            handle(isolate->context(), isolate),
                            api_method_name_for_errors, std::move(resolver));
  job->Start();
}

std::shared_ptr<StreamingDecoder> WasmEngine::StartStreamingCompilation(
    Isolate* isolate, const WasmFeatures& enabled, Handle<Context> context,
    const char* api_method_name,
    std::shared_ptr<CompilationResultResolver> resolver) {
  AsyncCompileJob* job =
      CreateAsyncCompileJob(isolate, enabled, std::unique_ptr<byte[]>(nullptr),
                            0, context, api_method_name, std::move(resolver));
  return job->CreateStreamingDecoder();
}

void WasmEngine::CompileFunction(Isolate* isolate, NativeModule* native_module,
                                 uint32_t function_index, ExecutionTier tier) {
  // Note we assume that "one-off" compilations can discard detected features.
  WasmFeatures detected = WasmFeatures::None();
  WasmCompilationUnit::CompileWasmFunction(
      isolate, native_module, &detected,
      &native_module->module()->functions[function_index], tier);
}

void WasmEngine::RecompileAllFunctions(Isolate* isolate,
                                       NativeModule* native_module,
                                       ExecutionTier tier) {
  RecompileNativeModule(isolate, native_module, tier);
}

std::shared_ptr<NativeModule> WasmEngine::ExportNativeModule(
    Handle<WasmModuleObject> module_object) {
  return module_object->shared_native_module();
}

Handle<WasmModuleObject> WasmEngine::ImportNativeModule(
    Isolate* isolate, std::shared_ptr<NativeModule> shared_native_module) {
  NativeModule* native_module = shared_native_module.get();
  ModuleWireBytes wire_bytes(native_module->wire_bytes());
  Handle<Script> script = CreateWasmScript(
      isolate, wire_bytes, VectorOf(native_module->module()->source_map_url),
      native_module->module()->name);
  Handle<FixedArray> export_wrappers;
  CompileJsToWasmWrappers(isolate, native_module->module(), &export_wrappers);
  Handle<WasmModuleObject> module_object = WasmModuleObject::New(
      isolate, std::move(shared_native_module), script, export_wrappers,
      native_module->committed_code_space());
  {
    base::MutexGuard lock(&mutex_);
    DCHECK_EQ(1, isolates_.count(isolate));
    isolates_[isolate]->native_modules.insert(native_module);
    DCHECK_EQ(1, native_modules_.count(native_module));
    native_modules_[native_module]->isolates.insert(isolate);
  }

  // Finish the Wasm script now and make it public to the debugger.
  isolate->debug()->OnAfterCompile(script);
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
    const char* api_method_name,
    std::shared_ptr<CompilationResultResolver> resolver) {
  AsyncCompileJob* job =
      new AsyncCompileJob(isolate, enabled, std::move(bytes_copy), length,
                          context, api_method_name, std::move(resolver));
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

void WasmEngine::DeleteCompileJobsOnContext(Handle<Context> context) {
  // Under the mutex get all jobs to delete. Then delete them without holding
  // the mutex, such that deletion can reenter the WasmEngine.
  std::vector<std::unique_ptr<AsyncCompileJob>> jobs_to_delete;
  {
    base::MutexGuard guard(&mutex_);
    for (auto it = async_compile_jobs_.begin();
         it != async_compile_jobs_.end();) {
      if (!it->first->context().is_identical_to(context)) {
        ++it;
        continue;
      }
      jobs_to_delete.push_back(std::move(it->second));
      it = async_compile_jobs_.erase(it);
    }
  }
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

namespace {
int GetGCTimeMicros(base::TimeTicks start) {
  DCHECK(!start.IsNull());
  int64_t duration_us = (base::TimeTicks::Now() - start).InMicroseconds();
  return static_cast<int>(
      std::min(std::max(int64_t{0}, duration_us), int64_t{kMaxInt}));
}
}  // namespace

void WasmEngine::AddIsolate(Isolate* isolate) {
  base::MutexGuard guard(&mutex_);
  DCHECK_EQ(0, isolates_.count(isolate));
  isolates_.emplace(isolate, std::make_unique<IsolateInfo>(isolate));

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
    // If there is an ongoing code GC, sample its time here. This will record
    // samples for very long-running or never ending GCs.
    if (engine->current_gc_info_ &&
        !engine->current_gc_info_->start_time.IsNull()) {
      isolate->counters()->wasm_code_gc_time()->AddSample(
          GetGCTimeMicros(engine->current_gc_info_->start_time));
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
      for (WasmCode* code : info->potentially_dead_code) {
        current_gc_info_->dead_code.erase(code);
      }
    }
  }
  if (current_gc_info_) {
    if (RemoveIsolateFromCurrentGC(isolate)) PotentiallyFinishCurrentGC();
  }
  if (auto* task = info->log_codes_task) task->Cancel();
  if (!info->code_to_log.empty()) {
    WasmCode::DecrementRefCount(VectorOf(info->code_to_log));
    info->code_to_log.clear();
  }
}

void WasmEngine::LogCode(Vector<WasmCode*> code_vec) {
  if (code_vec.empty()) return;
  base::MutexGuard guard(&mutex_);
  NativeModule* native_module = code_vec[0]->native_module();
  DCHECK_EQ(1, native_modules_.count(native_module));
  for (Isolate* isolate : native_modules_[native_module]->isolates) {
    DCHECK_EQ(1, isolates_.count(isolate));
    IsolateInfo* info = isolates_[isolate].get();
    if (info->log_codes == false) continue;
    if (info->log_codes_task == nullptr) {
      auto new_task = std::make_unique<LogCodesTask>(
          &mutex_, &info->log_codes_task, isolate, this);
      info->log_codes_task = new_task.get();
      info->foreground_task_runner->PostTask(std::move(new_task));
    }
    if (info->code_to_log.empty()) {
      isolate->stack_guard()->RequestLogWasmCode();
    }
    info->code_to_log.insert(info->code_to_log.end(), code_vec.begin(),
                             code_vec.end());
    for (WasmCode* code : code_vec) {
      DCHECK_EQ(native_module, code->native_module());
      code->IncRef();
    }
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
    Isolate* isolate, const WasmFeatures& enabled,
    std::shared_ptr<const WasmModule> module, size_t code_size_estimate) {
  // TODO(clemensb): Remove --wasm-far-jump-table and {can_request_more}.
  bool can_request_more =
      !wasm::NativeModule::kNeedsFarJumpsBetweenCodeSpaces ||
      FLAG_wasm_far_jump_table;
  std::shared_ptr<NativeModule> native_module =
      code_manager_.NewNativeModule(this, isolate, enabled, code_size_estimate,
                                    can_request_more, std::move(module));
  base::MutexGuard lock(&mutex_);
  auto pair = native_modules_.insert(std::make_pair(
      native_module.get(), std::make_unique<NativeModuleInfo>()));
  DCHECK(pair.second);  // inserted new entry.
  pair.first->second.get()->isolates.insert(isolate);
  isolates_[isolate]->native_modules.insert(native_module.get());
  return native_module;
}

std::shared_ptr<NativeModule> WasmEngine::MaybeGetNativeModule(
    ModuleOrigin origin, Vector<const uint8_t> wire_bytes) {
  return native_module_cache_.MaybeGetNativeModule(origin, wire_bytes);
}

void WasmEngine::UpdateNativeModuleCache(
    std::shared_ptr<NativeModule> native_module, bool error) {
  native_module_cache_.Update(native_module, error);
}

void WasmEngine::FreeNativeModule(NativeModule* native_module) {
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
  // If there is a GC running which has references to code contained in the
  // deleted {NativeModule}, remove those references.
  if (current_gc_info_) {
    for (auto it = current_gc_info_->dead_code.begin(),
              end = current_gc_info_->dead_code.end();
         it != end;) {
      if ((*it)->native_module() == native_module) {
        it = current_gc_info_->dead_code.erase(it);
      } else {
        ++it;
      }
    }
    TRACE_CODE_GC("Native module %p died, reducing dead code objects to %zu.\n",
                  native_module, current_gc_info_->dead_code.size());
  }
  native_module_cache_.Erase(native_module);
  native_modules_.erase(it);
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
        std::make_unique<SampleTopTierCodeSizeTask>(isolate, native_module));
  }
}

void WasmEngine::ReportLiveCodeForGC(Isolate* isolate,
                                     Vector<WasmCode*> live_code) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.wasm"), "ReportLiveCodeForGC");
  TRACE_CODE_GC("Isolate %d reporting %zu live code objects.\n", isolate->id(),
                live_code.size());
  base::MutexGuard guard(&mutex_);
  // This report might come in late (note that we trigger both a stack guard and
  // a foreground task). In that case, ignore it.
  if (current_gc_info_ == nullptr) return;
  if (!RemoveIsolateFromCurrentGC(isolate)) return;
  isolate->counters()->wasm_module_num_triggered_code_gcs()->AddSample(
      current_gc_info_->gc_sequence_index);
  for (WasmCode* code : live_code) current_gc_info_->dead_code.erase(code);
  PotentiallyFinishCurrentGC();
}

void WasmEngine::ReportLiveCodeFromStackForGC(Isolate* isolate) {
  wasm::WasmCodeRefScope code_ref_scope;
  std::unordered_set<wasm::WasmCode*> live_wasm_code;
  for (StackFrameIterator it(isolate); !it.done(); it.Advance()) {
    StackFrame* const frame = it.frame();
    if (frame->type() != StackFrame::WASM_COMPILED) continue;
    live_wasm_code.insert(WasmCompiledFrame::cast(frame)->wasm_code());
  }

  CheckNoArchivedThreads(isolate);

  ReportLiveCodeForGC(isolate,
                      OwnedVector<WasmCode*>::Of(live_wasm_code).as_vector());
}

bool WasmEngine::AddPotentiallyDeadCode(WasmCode* code) {
  base::MutexGuard guard(&mutex_);
  auto it = native_modules_.find(code->native_module());
  DCHECK_NE(native_modules_.end(), it);
  NativeModuleInfo* info = it->second.get();
  if (info->dead_code.count(code)) return false;  // Code is already dead.
  auto added = info->potentially_dead_code.insert(code);
  if (!added.second) return false;  // An entry already existed.
  new_potentially_dead_code_size_ += code->instructions().size();
  if (FLAG_wasm_code_gc) {
    // Trigger a GC if 64kB plus 10% of committed code are potentially dead.
    size_t dead_code_limit =
        FLAG_stress_wasm_code_gc
            ? 0
            : 64 * KB + code_manager_.committed_code_space() / 10;
    if (new_potentially_dead_code_size_ > dead_code_limit) {
      bool inc_gc_count =
          info->num_code_gcs_triggered < std::numeric_limits<int8_t>::max();
      if (current_gc_info_ == nullptr) {
        if (inc_gc_count) ++info->num_code_gcs_triggered;
        TRACE_CODE_GC(
            "Triggering GC (potentially dead: %zu bytes; limit: %zu bytes).\n",
            new_potentially_dead_code_size_, dead_code_limit);
        TriggerGC(info->num_code_gcs_triggered);
      } else if (current_gc_info_->next_gc_sequence_index == 0) {
        if (inc_gc_count) ++info->num_code_gcs_triggered;
        TRACE_CODE_GC(
            "Scheduling another GC after the current one (potentially dead: "
            "%zu bytes; limit: %zu bytes).\n",
            new_potentially_dead_code_size_, dead_code_limit);
        current_gc_info_->next_gc_sequence_index = info->num_code_gcs_triggered;
        DCHECK_NE(0, current_gc_info_->next_gc_sequence_index);
      }
    }
  }
  return true;
}

void WasmEngine::FreeDeadCode(const DeadCodeMap& dead_code) {
  base::MutexGuard guard(&mutex_);
  FreeDeadCodeLocked(dead_code);
}

void WasmEngine::FreeDeadCodeLocked(const DeadCodeMap& dead_code) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.wasm"), "FreeDeadCode");
  DCHECK(!mutex_.TryLock());
  for (auto& dead_code_entry : dead_code) {
    NativeModule* native_module = dead_code_entry.first;
    const std::vector<WasmCode*>& code_vec = dead_code_entry.second;
    DCHECK_EQ(1, native_modules_.count(native_module));
    auto* info = native_modules_[native_module].get();
    TRACE_CODE_GC("Freeing %zu code object%s of module %p.\n", code_vec.size(),
                  code_vec.size() == 1 ? "" : "s", native_module);
    for (WasmCode* code : code_vec) {
      DCHECK_EQ(1, info->dead_code.count(code));
      info->dead_code.erase(code);
    }
    native_module->FreeCode(VectorOf(code_vec));
  }
}

void WasmEngine::TriggerGC(int8_t gc_sequence_index) {
  DCHECK_NULL(current_gc_info_);
  DCHECK(FLAG_wasm_code_gc);
  new_potentially_dead_code_size_ = 0;
  current_gc_info_.reset(new CurrentGCInfo(gc_sequence_index));
  if (base::TimeTicks::IsHighResolution()) {
    current_gc_info_->start_time = base::TimeTicks::Now();
  }
  // Add all potentially dead code to this GC, and trigger a GC task in each
  // isolate.
  for (auto& entry : native_modules_) {
    NativeModuleInfo* info = entry.second.get();
    if (info->potentially_dead_code.empty()) continue;
    for (auto* isolate : native_modules_[entry.first]->isolates) {
      auto& gc_task = current_gc_info_->outstanding_isolates[isolate];
      if (!gc_task) {
        auto new_task = std::make_unique<WasmGCForegroundTask>(isolate);
        gc_task = new_task.get();
        DCHECK_EQ(1, isolates_.count(isolate));
        isolates_[isolate]->foreground_task_runner->PostTask(
            std::move(new_task));
      }
      isolate->stack_guard()->RequestWasmCodeGC();
    }
    for (WasmCode* code : info->potentially_dead_code) {
      current_gc_info_->dead_code.insert(code);
    }
  }
  TRACE_CODE_GC(
      "Starting GC. Total number of potentially dead code objects: %zu\n",
      current_gc_info_->dead_code.size());
}

bool WasmEngine::RemoveIsolateFromCurrentGC(Isolate* isolate) {
  DCHECK(!mutex_.TryLock());
  DCHECK_NOT_NULL(current_gc_info_);
  return current_gc_info_->outstanding_isolates.erase(isolate) != 0;
}

void WasmEngine::PotentiallyFinishCurrentGC() {
  DCHECK(!mutex_.TryLock());
  TRACE_CODE_GC(
      "Remaining dead code objects: %zu; outstanding isolates: %zu.\n",
      current_gc_info_->dead_code.size(),
      current_gc_info_->outstanding_isolates.size());

  // If there are more outstanding isolates, return immediately.
  if (!current_gc_info_->outstanding_isolates.empty()) return;

  // All remaining code in {current_gc_info->dead_code} is really dead.
  // Move it from the set of potentially dead code to the set of dead code,
  // and decrement its ref count.
  size_t num_freed = 0;
  DeadCodeMap dead_code;
  for (WasmCode* code : current_gc_info_->dead_code) {
    DCHECK_EQ(1, native_modules_.count(code->native_module()));
    auto* native_module_info = native_modules_[code->native_module()].get();
    DCHECK_EQ(1, native_module_info->potentially_dead_code.count(code));
    native_module_info->potentially_dead_code.erase(code);
    DCHECK_EQ(0, native_module_info->dead_code.count(code));
    native_module_info->dead_code.insert(code);
    if (code->DecRefOnDeadCode()) {
      dead_code[code->native_module()].push_back(code);
      ++num_freed;
    }
  }

  FreeDeadCodeLocked(dead_code);

  int duration_us = 0;
  if (!current_gc_info_->start_time.IsNull()) {
    duration_us = GetGCTimeMicros(current_gc_info_->start_time);
    for (auto& entry : isolates_) {
      entry.second->async_counters->wasm_code_gc_time()->AddSample(duration_us);
    }
  }

  TRACE_CODE_GC("Took %d us; found %zu dead code objects, freed %zu.\n",
                duration_us, current_gc_info_->dead_code.size(), num_freed);
  USE(num_freed);

  int8_t next_gc_sequence_index = current_gc_info_->next_gc_sequence_index;
  current_gc_info_.reset();
  if (next_gc_sequence_index != 0) TriggerGC(next_gc_sequence_index);
}

namespace {

DEFINE_LAZY_LEAKY_OBJECT_GETTER(std::shared_ptr<WasmEngine>,
                                GetSharedWasmEngine)

}  // namespace

// static
void WasmEngine::InitializeOncePerProcess() {
  *GetSharedWasmEngine() = std::make_shared<WasmEngine>();
}

// static
void WasmEngine::GlobalTearDown() {
  GetSharedWasmEngine()->reset();
}

// static
std::shared_ptr<WasmEngine> WasmEngine::GetWasmEngine() {
  return *GetSharedWasmEngine();
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

#undef TRACE_CODE_GC

}  // namespace wasm
}  // namespace internal
}  // namespace v8
