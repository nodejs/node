// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/wasm/wasm-engine.h"

#include "src/base/functional.h"
#include "src/base/platform/time.h"
#include "src/common/globals.h"
#include "src/debug/debug.h"
#include "src/diagnostics/code-tracer.h"
#include "src/diagnostics/compilation-statistics.h"
#include "src/execution/frames.h"
#include "src/execution/v8threads.h"
#include "src/handles/global-handles-inl.h"
#include "src/logging/counters.h"
#include "src/logging/metrics.h"
#include "src/objects/heap-number.h"
#include "src/objects/managed-inl.h"
#include "src/objects/objects-inl.h"
#include "src/utils/ostreams.h"
#include "src/wasm/function-compiler.h"
#include "src/wasm/module-compiler.h"
#include "src/wasm/module-decoder.h"
#include "src/wasm/module-instantiate.h"
#include "src/wasm/pgo.h"
#include "src/wasm/stacks.h"
#include "src/wasm/streaming-decoder.h"
#include "src/wasm/wasm-debug.h"
#include "src/wasm/wasm-limits.h"
#include "src/wasm/wasm-objects-inl.h"

#ifdef V8_ENABLE_WASM_GDB_REMOTE_DEBUGGING
#include "src/debug/wasm/gdb-server/gdb-server.h"
#endif  // V8_ENABLE_WASM_GDB_REMOTE_DEBUGGING

namespace v8::internal::wasm {

#define TRACE_CODE_GC(...)                                             \
  do {                                                                 \
    if (v8_flags.trace_wasm_code_gc) PrintF("[wasm-gc] " __VA_ARGS__); \
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

  ~LogCodesTask() override {
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
    // The stack can contain live frames, for instance when this is invoked
    // during a pause or a breakpoint.
    GetWasmEngine()->ReportLiveCodeFromStackForGC(isolate_);
  }

 private:
  Isolate* isolate_;
};

class WeakScriptHandle {
 public:
  explicit WeakScriptHandle(Handle<Script> script) : script_id_(script->id()) {
    DCHECK(script->name().IsString() || script->name().IsUndefined());
    if (script->name().IsString()) {
      std::unique_ptr<char[]> source_url =
          String::cast(script->name()).ToCString();
      // Convert from {unique_ptr} to {shared_ptr}.
      source_url_ = {source_url.release(), source_url.get_deleter()};
    }
    auto global_handle =
        script->GetIsolate()->global_handles()->Create(*script);
    location_ = std::make_unique<Address*>(global_handle.location());
    GlobalHandles::MakeWeak(location_.get());
  }

  // Usually the destructor of this class should always be called after the weak
  // callback because the Script keeps the NativeModule alive. So we expect the
  // handle to be destroyed and the location to be reset already.
  // We cannot check this because of one exception. When the native module is
  // freed during isolate shutdown, the destructor will be called
  // first, and the callback will never be called.
  ~WeakScriptHandle() = default;

  WeakScriptHandle(WeakScriptHandle&&) V8_NOEXCEPT = default;

  Handle<Script> handle() const { return Handle<Script>(*location_); }

  int script_id() const { return script_id_; }

  const std::shared_ptr<const char>& source_url() const { return source_url_; }

 private:
  // Store the location in a unique_ptr so that its address stays the same even
  // when this object is moved/copied.
  std::unique_ptr<Address*> location_;

  // Store the script ID independent of the weak handle, such that it's always
  // available.
  int script_id_;

  // Similar for the source URL. We cannot dereference the Handle from arbitrary
  // threads, but we need the URL available for code logging.
  // The shared pointer is kept alive by unlogged code, even if this entry is
  // collected in the meantime.
  // TODO(chromium:1132260): Revisit this for huge URLs.
  std::shared_ptr<const char> source_url_;
};

// If PGO data is being collected, keep all native modules alive, so repeated
// runs of a benchmark (with different configuration) all use the same module.
// This vector is protected by the global WasmEngine's mutex, but not defined in
// the header because it's a private implementation detail.
std::vector<std::shared_ptr<NativeModule>>* native_modules_kept_alive_for_pgo;

}  // namespace

std::shared_ptr<NativeModule> NativeModuleCache::MaybeGetNativeModule(
    ModuleOrigin origin, base::Vector<const uint8_t> wire_bytes) {
  if (!v8_flags.wasm_native_module_cache_enabled) return nullptr;
  if (origin != kWasmOrigin) return nullptr;
  base::MutexGuard lock(&mutex_);
  size_t prefix_hash = PrefixHash(wire_bytes);
  NativeModuleCache::Key key{prefix_hash, wire_bytes};
  while (true) {
    auto it = map_.find(key);
    if (it == map_.end()) {
      // Even though this exact key is not in the cache, there might be a
      // matching prefix hash indicating that a streaming compilation is
      // currently compiling a module with the same prefix. {OnFinishedStream}
      // happens on the main thread too, so waiting for streaming compilation to
      // finish would create a deadlock. Instead, compile the module twice and
      // handle the conflict in {UpdateNativeModuleCache}.

      // Insert a {nullopt} entry to let other threads know that this
      // {NativeModule} is already being created on another thread.
      auto p = map_.emplace(key, base::nullopt);
      USE(p);
      DCHECK(p.second);
      return nullptr;
    }
    if (it->second.has_value()) {
      if (auto shared_native_module = it->second.value().lock()) {
        DCHECK_EQ(shared_native_module->wire_bytes(), wire_bytes);
        return shared_native_module;
      }
    }
    // TODO(11858): This deadlocks in predictable mode, because there is only a
    // single thread.
    cache_cv_.Wait(&mutex_);
  }
}

bool NativeModuleCache::GetStreamingCompilationOwnership(size_t prefix_hash) {
  base::MutexGuard lock(&mutex_);
  auto it = map_.lower_bound(Key{prefix_hash, {}});
  if (it != map_.end() && it->first.prefix_hash == prefix_hash) {
    DCHECK_IMPLIES(!it->first.bytes.empty(),
                   PrefixHash(it->first.bytes) == prefix_hash);
    return false;
  }
  Key key{prefix_hash, {}};
  DCHECK_EQ(0, map_.count(key));
  map_.emplace(key, base::nullopt);
  return true;
}

void NativeModuleCache::StreamingCompilationFailed(size_t prefix_hash) {
  base::MutexGuard lock(&mutex_);
  Key key{prefix_hash, {}};
  DCHECK_EQ(1, map_.count(key));
  map_.erase(key);
  cache_cv_.NotifyAll();
}

std::shared_ptr<NativeModule> NativeModuleCache::Update(
    std::shared_ptr<NativeModule> native_module, bool error) {
  DCHECK_NOT_NULL(native_module);
  if (!v8_flags.wasm_native_module_cache_enabled) return native_module;
  if (native_module->module()->origin != kWasmOrigin) return native_module;
  base::Vector<const uint8_t> wire_bytes = native_module->wire_bytes();
  DCHECK(!wire_bytes.empty());
  size_t prefix_hash = PrefixHash(native_module->wire_bytes());
  base::MutexGuard lock(&mutex_);
  map_.erase(Key{prefix_hash, {}});
  const Key key{prefix_hash, wire_bytes};
  auto it = map_.find(key);
  if (it != map_.end()) {
    if (it->second.has_value()) {
      auto conflicting_module = it->second.value().lock();
      if (conflicting_module != nullptr) {
        DCHECK_EQ(conflicting_module->wire_bytes(), wire_bytes);
        return conflicting_module;
      }
    }
    map_.erase(it);
  }
  if (!error) {
    // The key now points to the new native module's owned copy of the bytes,
    // so that it stays valid until the native module is freed and erased from
    // the map.
    auto p = map_.emplace(
        key, base::Optional<std::weak_ptr<NativeModule>>(native_module));
    USE(p);
    DCHECK(p.second);
  }
  cache_cv_.NotifyAll();
  return native_module;
}

void NativeModuleCache::Erase(NativeModule* native_module) {
  if (!v8_flags.wasm_native_module_cache_enabled) return;
  if (native_module->module()->origin != kWasmOrigin) return;
  // Happens in some tests where bytes are set directly.
  if (native_module->wire_bytes().empty()) return;
  base::MutexGuard lock(&mutex_);
  size_t prefix_hash = PrefixHash(native_module->wire_bytes());
  map_.erase(Key{prefix_hash, native_module->wire_bytes()});
  cache_cv_.NotifyAll();
}

// static
size_t NativeModuleCache::PrefixHash(base::Vector<const uint8_t> wire_bytes) {
  // Compute the hash as a combined hash of the sections up to the code section
  // header, to mirror the way streaming compilation does it.
  Decoder decoder(wire_bytes.begin(), wire_bytes.end());
  decoder.consume_bytes(8, "module header");
  size_t hash = GetWireBytesHash(wire_bytes.SubVector(0, 8));
  SectionCode section_id = SectionCode::kUnknownSectionCode;
  while (decoder.ok() && decoder.more()) {
    section_id = static_cast<SectionCode>(decoder.consume_u8());
    uint32_t section_size = decoder.consume_u32v("section size");
    if (section_id == SectionCode::kCodeSectionCode) {
      hash = base::hash_combine(hash, section_size);
      break;
    }
    const uint8_t* payload_start = decoder.pc();
    decoder.consume_bytes(section_size, "section payload");
    size_t section_hash =
        GetWireBytesHash(base::VectorOf(payload_start, section_size));
    hash = base::hash_combine(hash, section_hash);
  }
  return hash;
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
        async_counters(isolate->async_counters()),
        wrapper_compilation_barrier_(std::make_shared<OperationsBarrier>()) {
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

  // All native modules that are being used by this Isolate.
  std::unordered_set<NativeModule*> native_modules;

  // Scripts created for each native module in this isolate.
  std::unordered_map<NativeModule*, WeakScriptHandle> scripts;

  // Caches whether code needs to be logged on this isolate.
  bool log_codes;

  // The currently scheduled LogCodesTask.
  LogCodesTask* log_codes_task = nullptr;

  // Maps script ID to vector of code objects that still need to be logged, and
  // the respective source URL.
  struct CodeToLogPerScript {
    std::vector<WasmCode*> code;
    std::shared_ptr<const char> source_url;
  };
  std::unordered_map<int, CodeToLogPerScript> code_to_log;

  // The foreground task runner of the isolate (can be called from background).
  std::shared_ptr<v8::TaskRunner> foreground_task_runner;

  const std::shared_ptr<Counters> async_counters;

  // Keep new modules in tiered down state.
  bool keep_tiered_down = false;

  // Keep track whether we already added a sample for PKU support (we only want
  // one sample per Isolate).
  bool pku_support_sampled = false;

  // Elapsed time since last throw/rethrow/catch event.
  base::ElapsedTimer throw_timer;
  base::ElapsedTimer rethrow_timer;
  base::ElapsedTimer catch_timer;

  // Total number of exception events in this isolate.
  int throw_count = 0;
  int rethrow_count = 0;
  int catch_count = 0;

  // Operations barrier to synchronize on wrapper compilation on isolate
  // shutdown.
  // TODO(wasm): Remove this once we can use the generic js-to-wasm wrapper
  // everywhere.
  std::shared_ptr<OperationsBarrier> wrapper_compilation_barrier_;
};

struct WasmEngine::NativeModuleInfo {
  explicit NativeModuleInfo(std::weak_ptr<NativeModule> native_module)
      : weak_ptr(std::move(native_module)) {}

  // Weak pointer, to gain back a shared_ptr if needed.
  std::weak_ptr<NativeModule> weak_ptr;

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

WasmEngine::WasmEngine() = default;

WasmEngine::~WasmEngine() {
#ifdef V8_ENABLE_WASM_GDB_REMOTE_DEBUGGING
  // Synchronize on the GDB-remote thread, if running.
  gdb_server_.reset();
#endif  // V8_ENABLE_WASM_GDB_REMOTE_DEBUGGING

  // Free all modules that were kept alive for collecting PGO. This is to avoid
  // memory leaks.
  if (V8_UNLIKELY(native_modules_kept_alive_for_pgo)) {
    delete native_modules_kept_alive_for_pgo;
  }

  operations_barrier_->CancelAndWait();

  // All AsyncCompileJobs have been canceled.
  DCHECK(async_compile_jobs_.empty());
  // All Isolates have been deregistered.
  DCHECK(isolates_.empty());
  // All NativeModules did die.
  DCHECK(native_modules_.empty());
  // Native module cache does not leak.
  DCHECK(native_module_cache_.empty());
}

bool WasmEngine::SyncValidate(Isolate* isolate, const WasmFeatures& enabled,
                              const ModuleWireBytes& bytes,
                              std::string* error_message) {
  TRACE_EVENT0("v8.wasm", "wasm.SyncValidate");
  // TODO(titzer): remove dependency on the isolate.
  if (bytes.start() == nullptr || bytes.length() == 0) {
    if (error_message) *error_message = "empty module wire bytes";
    return false;
  }
  auto result = DecodeWasmModule(
      enabled, bytes.start(), bytes.end(), true, kWasmOrigin,
      isolate->counters(), isolate->metrics_recorder(),
      isolate->GetOrRegisterRecorderContextId(isolate->native_context()),
      DecodingMethod::kSync, allocator());
  if (result.failed() && error_message) {
    *error_message = result.error().message();
  }
  return result.ok();
}

MaybeHandle<AsmWasmData> WasmEngine::SyncCompileTranslatedAsmJs(
    Isolate* isolate, ErrorThrower* thrower, const ModuleWireBytes& bytes,
    base::Vector<const byte> asm_js_offset_table_bytes,
    Handle<HeapNumber> uses_bitset, LanguageMode language_mode) {
  int compilation_id = next_compilation_id_.fetch_add(1);
  TRACE_EVENT1("v8.wasm", "wasm.SyncCompileTranslatedAsmJs", "id",
               compilation_id);
  ModuleOrigin origin = language_mode == LanguageMode::kSloppy
                            ? kAsmJsSloppyOrigin
                            : kAsmJsStrictOrigin;
  // TODO(leszeks): If we want asm.js in UKM, we should figure out a way to pass
  // the context id in here.
  v8::metrics::Recorder::ContextId context_id =
      v8::metrics::Recorder::ContextId::Empty();
  ModuleResult result = DecodeWasmModule(
      WasmFeatures::ForAsmjs(), bytes.start(), bytes.end(), false, origin,
      isolate->counters(), isolate->metrics_recorder(), context_id,
      DecodingMethod::kSync, allocator());
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
  std::shared_ptr<NativeModule> native_module = CompileToNativeModule(
      isolate, WasmFeatures::ForAsmjs(), thrower, std::move(result).value(),
      bytes, compilation_id, context_id);
  if (!native_module) return {};

  return AsmWasmData::New(isolate, std::move(native_module), uses_bitset);
}

Handle<WasmModuleObject> WasmEngine::FinalizeTranslatedAsmJs(
    Isolate* isolate, Handle<AsmWasmData> asm_wasm_data,
    Handle<Script> script) {
  std::shared_ptr<NativeModule> native_module =
      asm_wasm_data->managed_native_module().get();
  Handle<WasmModuleObject> module_object =
      WasmModuleObject::New(isolate, std::move(native_module), script);
  return module_object;
}

MaybeHandle<WasmModuleObject> WasmEngine::SyncCompile(
    Isolate* isolate, const WasmFeatures& enabled, ErrorThrower* thrower,
    const ModuleWireBytes& bytes) {
  int compilation_id = next_compilation_id_.fetch_add(1);
  TRACE_EVENT1("v8.wasm", "wasm.SyncCompile", "id", compilation_id);
  v8::metrics::Recorder::ContextId context_id =
      isolate->GetOrRegisterRecorderContextId(isolate->native_context());
  std::shared_ptr<WasmModule> module;
  {
    ModuleResult result = DecodeWasmModule(
        enabled, bytes.start(), bytes.end(), false, kWasmOrigin,
        isolate->counters(), isolate->metrics_recorder(), context_id,
        DecodingMethod::kSync, allocator());
    if (result.failed()) {
      thrower->CompileFailed(result.error());
      return {};
    }
    module = std::move(result).value();
  }

  // If experimental PGO via files is enabled, load profile information now.
  if (V8_UNLIKELY(FLAG_experimental_wasm_pgo_from_file)) {
    LoadProfileFromFile(module.get(), bytes.module_bytes());
  }

  // Transfer ownership of the WasmModule to the {Managed<WasmModule>} generated
  // in {CompileToNativeModule}.
  std::shared_ptr<NativeModule> native_module =
      CompileToNativeModule(isolate, enabled, thrower, std::move(module), bytes,
                            compilation_id, context_id);
  if (!native_module) return {};

#ifdef DEBUG
  // Ensure that code GC will check this isolate for live code.
  {
    base::MutexGuard lock(&mutex_);
    DCHECK_EQ(1, isolates_.count(isolate));
    DCHECK_EQ(1, isolates_[isolate]->native_modules.count(native_module.get()));
    DCHECK_EQ(1, native_modules_.count(native_module.get()));
    DCHECK_EQ(1, native_modules_[native_module.get()]->isolates.count(isolate));
  }
#endif

  constexpr base::Vector<const char> kNoSourceUrl;
  Handle<Script> script =
      GetOrCreateScript(isolate, native_module, kNoSourceUrl);

  native_module->LogWasmCodes(isolate, *script);

  // Create the compiled module object and populate with compiled functions
  // and information needed at instantiation time. This object needs to be
  // serializable. Instantiation may occur off a deserialized version of this
  // object.
  Handle<WasmModuleObject> module_object =
      WasmModuleObject::New(isolate, std::move(native_module), script);

  // Finish the Wasm script now and make it public to the debugger.
  isolate->debug()->OnAfterCompile(script);
  return module_object;
}

MaybeHandle<WasmInstanceObject> WasmEngine::SyncInstantiate(
    Isolate* isolate, ErrorThrower* thrower,
    Handle<WasmModuleObject> module_object, MaybeHandle<JSReceiver> imports,
    MaybeHandle<JSArrayBuffer> memory) {
  TRACE_EVENT0("v8.wasm", "wasm.SyncInstantiate");
  return InstantiateToInstanceObject(isolate, thrower, module_object, imports,
                                     memory);
}

void WasmEngine::AsyncInstantiate(
    Isolate* isolate, std::unique_ptr<InstantiationResultResolver> resolver,
    Handle<WasmModuleObject> module_object, MaybeHandle<JSReceiver> imports) {
  ErrorThrower thrower(isolate, "WebAssembly.instantiate()");
  TRACE_EVENT0("v8.wasm", "wasm.AsyncInstantiate");
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
  int compilation_id = next_compilation_id_.fetch_add(1);
  TRACE_EVENT1("v8.wasm", "wasm.AsyncCompile", "id", compilation_id);
  if (!v8_flags.wasm_async_compilation) {
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

  if (v8_flags.wasm_test_streaming) {
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

  AsyncCompileJob* job = CreateAsyncCompileJob(
      isolate, enabled, std::move(copy), bytes.length(),
      handle(isolate->context(), isolate), api_method_name_for_errors,
      std::move(resolver), compilation_id);
  job->Start();
}

std::shared_ptr<StreamingDecoder> WasmEngine::StartStreamingCompilation(
    Isolate* isolate, const WasmFeatures& enabled, Handle<Context> context,
    const char* api_method_name,
    std::shared_ptr<CompilationResultResolver> resolver) {
  int compilation_id = next_compilation_id_.fetch_add(1);
  TRACE_EVENT1("v8.wasm", "wasm.StartStreamingCompilation", "id",
               compilation_id);
  if (v8_flags.wasm_async_compilation) {
    AsyncCompileJob* job = CreateAsyncCompileJob(
        isolate, enabled, std::unique_ptr<byte[]>(nullptr), 0, context,
        api_method_name, std::move(resolver), compilation_id);
    return job->CreateStreamingDecoder();
  }
  return StreamingDecoder::CreateSyncStreamingDecoder(
      isolate, enabled, context, api_method_name, std::move(resolver));
}

void WasmEngine::CompileFunction(Isolate* isolate, NativeModule* native_module,
                                 uint32_t function_index, ExecutionTier tier) {
  // Note we assume that "one-off" compilations can discard detected features.
  WasmFeatures detected = WasmFeatures::None();
  WasmCompilationUnit::CompileWasmFunction(
      isolate, native_module, &detected,
      &native_module->module()->functions[function_index], tier);
}

void WasmEngine::TierDownAllModulesPerIsolate(Isolate* isolate) {
  std::vector<std::shared_ptr<NativeModule>> native_modules;
  {
    base::MutexGuard lock(&mutex_);
    if (isolates_[isolate]->keep_tiered_down) return;
    isolates_[isolate]->keep_tiered_down = true;
    for (auto* native_module : isolates_[isolate]->native_modules) {
      native_module->SetTieringState(kTieredDown);
      DCHECK_EQ(1, native_modules_.count(native_module));
      if (auto shared_ptr = native_modules_[native_module]->weak_ptr.lock()) {
        native_modules.emplace_back(std::move(shared_ptr));
      }
    }
  }
  for (auto& native_module : native_modules) {
    native_module->RecompileForTiering();
  }
}

void WasmEngine::TierUpAllModulesPerIsolate(Isolate* isolate) {
  // Only trigger recompilation after releasing the mutex, otherwise we risk
  // deadlocks because of lock inversion. The bool tells whether the module
  // needs recompilation for tier up.
  std::vector<std::pair<std::shared_ptr<NativeModule>, bool>> native_modules;
  {
    base::MutexGuard lock(&mutex_);
    isolates_[isolate]->keep_tiered_down = false;
    auto test_can_tier_up = [this](NativeModule* native_module) {
      DCHECK_EQ(1, native_modules_.count(native_module));
      for (auto* isolate : native_modules_[native_module]->isolates) {
        DCHECK_EQ(1, isolates_.count(isolate));
        if (isolates_[isolate]->keep_tiered_down) return false;
      }
      return true;
    };
    for (auto* native_module : isolates_[isolate]->native_modules) {
      DCHECK_EQ(1, native_modules_.count(native_module));
      auto shared_ptr = native_modules_[native_module]->weak_ptr.lock();
      if (!shared_ptr) continue;  // The module is not used any more.
      if (!native_module->IsTieredDown()) continue;
      // Only start tier-up if no other isolate needs this module in tiered
      // down state.
      bool tier_up = test_can_tier_up(native_module);
      if (tier_up) native_module->SetTieringState(kTieredUp);
      native_modules.emplace_back(std::move(shared_ptr), tier_up);
    }
  }
  for (auto& entry : native_modules) {
    auto& native_module = entry.first;
    bool tier_up = entry.second;
    // Remove all breakpoints set by this isolate.
    if (native_module->HasDebugInfo()) {
      native_module->GetDebugInfo()->RemoveIsolate(isolate);
    }
    if (tier_up) native_module->RecompileForTiering();
  }
}

std::shared_ptr<NativeModule> WasmEngine::ExportNativeModule(
    Handle<WasmModuleObject> module_object) {
  return module_object->shared_native_module();
}

namespace {
Handle<Script> CreateWasmScript(Isolate* isolate,
                                std::shared_ptr<NativeModule> native_module,
                                base::Vector<const char> source_url) {
  Handle<Script> script =
      isolate->factory()->NewScript(isolate->factory()->undefined_value());
  script->set_compilation_state(Script::COMPILATION_STATE_COMPILED);
  script->set_context_data(isolate->native_context()->debug_context_id());
  script->set_type(Script::TYPE_WASM);

  base::Vector<const uint8_t> wire_bytes = native_module->wire_bytes();

  // The source URL of the script is
  // - the original source URL if available (from the streaming API),
  // - wasm://wasm/<module name>-<hash> if a module name has been set, or
  // - wasm://wasm/<hash> otherwise.
  const WasmModule* module = native_module->module();
  Handle<String> url_str;
  if (!source_url.empty()) {
    url_str = isolate->factory()
                  ->NewStringFromUtf8(source_url, AllocationType::kOld)
                  .ToHandleChecked();
  } else {
    // Limit the printed hash to 8 characters.
    uint32_t hash = static_cast<uint32_t>(GetWireBytesHash(wire_bytes));
    base::EmbeddedVector<char, 32> buffer;
    if (module->name.is_empty()) {
      // Build the URL in the form "wasm://wasm/<hash>".
      int url_len = SNPrintF(buffer, "wasm://wasm/%08x", hash);
      DCHECK(url_len >= 0 && url_len < buffer.length());
      url_str = isolate->factory()
                    ->NewStringFromUtf8(buffer.SubVector(0, url_len),
                                        AllocationType::kOld)
                    .ToHandleChecked();
    } else {
      // Build the URL in the form "wasm://wasm/<module name>-<hash>".
      int hash_len = SNPrintF(buffer, "-%08x", hash);
      DCHECK(hash_len >= 0 && hash_len < buffer.length());
      Handle<String> prefix =
          isolate->factory()->NewStringFromStaticChars("wasm://wasm/");
      Handle<String> module_name =
          WasmModuleObject::ExtractUtf8StringFromModuleBytes(
              isolate, wire_bytes, module->name, kNoInternalize);
      Handle<String> hash_str =
          isolate->factory()
              ->NewStringFromUtf8(buffer.SubVector(0, hash_len))
              .ToHandleChecked();
      // Concatenate the three parts.
      url_str = isolate->factory()
                    ->NewConsString(prefix, module_name)
                    .ToHandleChecked();
      url_str = isolate->factory()
                    ->NewConsString(url_str, hash_str)
                    .ToHandleChecked();
    }
  }
  script->set_name(*url_str);

  const WasmDebugSymbols& debug_symbols = module->debug_symbols;
  if (debug_symbols.type == WasmDebugSymbols::Type::SourceMap &&
      !debug_symbols.external_url.is_empty()) {
    base::Vector<const char> external_url =
        ModuleWireBytes(wire_bytes).GetNameOrNull(debug_symbols.external_url);
    MaybeHandle<String> src_map_str = isolate->factory()->NewStringFromUtf8(
        external_url, AllocationType::kOld);
    script->set_source_mapping_url(*src_map_str.ToHandleChecked());
  }

  // Use the given shared {NativeModule}, but increase its reference count by
  // allocating a new {Managed<T>} that the {Script} references.
  size_t code_size_estimate = native_module->committed_code_space();
  size_t memory_estimate =
      code_size_estimate +
      wasm::WasmCodeManager::EstimateNativeModuleMetaDataSize(module);
  Handle<Managed<wasm::NativeModule>> managed_native_module =
      Managed<wasm::NativeModule>::FromSharedPtr(isolate, memory_estimate,
                                                 std::move(native_module));
  script->set_wasm_managed_native_module(*managed_native_module);
  script->set_wasm_breakpoint_infos(ReadOnlyRoots(isolate).empty_fixed_array());
  script->set_wasm_weak_instance_list(
      ReadOnlyRoots(isolate).empty_weak_array_list());
  return script;
}
}  // namespace

Handle<WasmModuleObject> WasmEngine::ImportNativeModule(
    Isolate* isolate, std::shared_ptr<NativeModule> shared_native_module,
    base::Vector<const char> source_url) {
  NativeModule* native_module = shared_native_module.get();
  ModuleWireBytes wire_bytes(native_module->wire_bytes());
  Handle<Script> script =
      GetOrCreateScript(isolate, shared_native_module, source_url);
  Handle<WasmModuleObject> module_object =
      WasmModuleObject::New(isolate, std::move(shared_native_module), script);
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

std::shared_ptr<CompilationStatistics>
WasmEngine::GetOrCreateTurboStatistics() {
  base::MutexGuard guard(&mutex_);
  if (compilation_stats_ == nullptr) {
    compilation_stats_.reset(new CompilationStatistics());
  }
  return compilation_stats_;
}

void WasmEngine::DumpAndResetTurboStatistics() {
  base::MutexGuard guard(&mutex_);
  if (compilation_stats_ != nullptr) {
    StdoutStream os;
    os << AsPrintableStatistics{*compilation_stats_.get(), false} << std::endl;
  }
  compilation_stats_.reset();
}

void WasmEngine::DumpTurboStatistics() {
  base::MutexGuard guard(&mutex_);
  if (compilation_stats_ != nullptr) {
    StdoutStream os;
    os << AsPrintableStatistics{*compilation_stats_.get(), false} << std::endl;
  }
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
    std::shared_ptr<CompilationResultResolver> resolver, int compilation_id) {
  Handle<Context> incumbent_context = isolate->GetIncumbentContext();
  AsyncCompileJob* job = new AsyncCompileJob(
      isolate, enabled, std::move(bytes_copy), length, context,
      incumbent_context, api_method_name, std::move(resolver), compilation_id);
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
  std::vector<std::weak_ptr<NativeModule>> modules_in_isolate;
  std::shared_ptr<OperationsBarrier> wrapper_compilation_barrier;
  {
    base::MutexGuard guard(&mutex_);
    for (auto it = async_compile_jobs_.begin();
         it != async_compile_jobs_.end();) {
      if (it->first->isolate() != isolate) {
        ++it;
        continue;
      }
      jobs_to_delete.push_back(std::move(it->second));
      it = async_compile_jobs_.erase(it);
    }
    DCHECK_EQ(1, isolates_.count(isolate));
    auto* isolate_info = isolates_[isolate].get();
    wrapper_compilation_barrier = isolate_info->wrapper_compilation_barrier_;
    for (auto* native_module : isolate_info->native_modules) {
      DCHECK_EQ(1, native_modules_.count(native_module));
      modules_in_isolate.emplace_back(native_modules_[native_module]->weak_ptr);
    }
  }

  // All modules that have not finished initial compilation yet cannot be
  // shared with other isolates. Hence we cancel their compilation. In
  // particular, this will cancel wrapper compilation which is bound to this
  // isolate (this would be a UAF otherwise).
  for (auto& weak_module : modules_in_isolate) {
    if (auto shared_module = weak_module.lock()) {
      shared_module->compilation_state()->CancelInitialCompilation();
    }
  }

  // After cancelling, wait for all current wrapper compilation to actually
  // finish.
  wrapper_compilation_barrier->CancelAndWait();
}

OperationsBarrier::Token WasmEngine::StartWrapperCompilation(Isolate* isolate) {
  base::MutexGuard guard(&mutex_);
  auto isolate_info_it = isolates_.find(isolate);
  if (isolate_info_it == isolates_.end()) return {};
  return isolate_info_it->second->wrapper_compilation_barrier_->TryLock();
}

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
    WasmEngine* engine = GetWasmEngine();
    base::MutexGuard lock(&engine->mutex_);
    DCHECK_EQ(1, engine->isolates_.count(isolate));
    for (auto* native_module : engine->isolates_[isolate]->native_modules) {
      native_module->SampleCodeSize(counters, NativeModule::kSampling);
    }
  };
  isolate->heap()->AddGCEpilogueCallback(callback, v8::kGCTypeMarkSweepCompact,
                                         nullptr);
#ifdef V8_ENABLE_WASM_GDB_REMOTE_DEBUGGING
  if (gdb_server_) {
    gdb_server_->AddIsolate(isolate);
  }
#endif  // V8_ENABLE_WASM_GDB_REMOTE_DEBUGGING
}

void WasmEngine::RemoveIsolate(Isolate* isolate) {
#ifdef V8_ENABLE_WASM_GDB_REMOTE_DEBUGGING
  if (gdb_server_) {
    gdb_server_->RemoveIsolate(isolate);
  }
#endif  // V8_ENABLE_WASM_GDB_REMOTE_DEBUGGING

  base::MutexGuard guard(&mutex_);
  auto it = isolates_.find(isolate);
  DCHECK_NE(isolates_.end(), it);
  std::unique_ptr<IsolateInfo> info = std::move(it->second);
  isolates_.erase(it);
  for (auto* native_module : info->native_modules) {
    DCHECK_EQ(1, native_modules_.count(native_module));
    DCHECK_EQ(1, native_modules_[native_module]->isolates.count(isolate));
    auto* module = native_modules_[native_module].get();
    module->isolates.erase(isolate);
    if (current_gc_info_) {
      for (WasmCode* code : module->potentially_dead_code) {
        current_gc_info_->dead_code.erase(code);
      }
    }
    if (native_module->HasDebugInfo()) {
      native_module->GetDebugInfo()->RemoveIsolate(isolate);
    }
  }
  if (current_gc_info_) {
    if (RemoveIsolateFromCurrentGC(isolate)) PotentiallyFinishCurrentGC();
  }
  if (auto* task = info->log_codes_task) {
    task->Cancel();
    for (auto& log_entry : info->code_to_log) {
      WasmCode::DecrementRefCount(base::VectorOf(log_entry.second.code));
    }
    info->code_to_log.clear();
  }
  DCHECK(info->code_to_log.empty());
}

void WasmEngine::LogCode(base::Vector<WasmCode*> code_vec) {
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
    for (WasmCode* code : code_vec) {
      DCHECK_EQ(native_module, code->native_module());
      code->IncRef();
    }

    auto script_it = info->scripts.find(native_module);
    // If the script does not yet exist, logging will happen later. If the weak
    // handle is cleared already, we also don't need to log any more.
    if (script_it == info->scripts.end()) continue;
    auto& log_entry = info->code_to_log[script_it->second.script_id()];
    if (!log_entry.source_url) {
      log_entry.source_url = script_it->second.source_url();
    }
    log_entry.code.insert(log_entry.code.end(), code_vec.begin(),
                          code_vec.end());
  }
}

void WasmEngine::EnableCodeLogging(Isolate* isolate) {
  base::MutexGuard guard(&mutex_);
  auto it = isolates_.find(isolate);
  DCHECK_NE(isolates_.end(), it);
  it->second->log_codes = true;
}

void WasmEngine::LogOutstandingCodesForIsolate(Isolate* isolate) {
  // Under the mutex, get the vector of wasm code to log. Then log and decrement
  // the ref count without holding the mutex.
  std::unordered_map<int, IsolateInfo::CodeToLogPerScript> code_to_log;
  {
    base::MutexGuard guard(&mutex_);
    DCHECK_EQ(1, isolates_.count(isolate));
    code_to_log.swap(isolates_[isolate]->code_to_log);
  }

  // Check again whether we still need to log code.
  bool should_log = WasmCode::ShouldBeLogged(isolate);

  TRACE_EVENT0("v8.wasm", "wasm.LogCode");
  for (auto& pair : code_to_log) {
    for (WasmCode* code : pair.second.code) {
      if (should_log) {
        code->LogCode(isolate, pair.second.source_url.get(), pair.first);
      }
    }
    WasmCode::DecrementRefCount(base::VectorOf(pair.second.code));
  }
}

std::shared_ptr<NativeModule> WasmEngine::NewNativeModule(
    Isolate* isolate, const WasmFeatures& enabled,
    std::shared_ptr<const WasmModule> module, size_t code_size_estimate) {
#ifdef V8_ENABLE_WASM_GDB_REMOTE_DEBUGGING
  if (v8_flags.wasm_gdb_remote && !gdb_server_) {
    gdb_server_ = gdb_server::GdbServer::Create();
    gdb_server_->AddIsolate(isolate);
  }
#endif  // V8_ENABLE_WASM_GDB_REMOTE_DEBUGGING

  std::shared_ptr<NativeModule> native_module =
      GetWasmCodeManager()->NewNativeModule(
          isolate, enabled, code_size_estimate, std::move(module));
  base::MutexGuard lock(&mutex_);
  if (V8_UNLIKELY(v8_flags.experimental_wasm_pgo_to_file)) {
    if (!native_modules_kept_alive_for_pgo) {
      native_modules_kept_alive_for_pgo =
          new std::vector<std::shared_ptr<NativeModule>>;
    }
    native_modules_kept_alive_for_pgo->emplace_back(native_module);
  }
  auto pair = native_modules_.insert(std::make_pair(
      native_module.get(), std::make_unique<NativeModuleInfo>(native_module)));
  DCHECK(pair.second);  // inserted new entry.
  pair.first->second.get()->isolates.insert(isolate);
  auto* isolate_info = isolates_[isolate].get();
  isolate_info->native_modules.insert(native_module.get());
  if (isolate_info->keep_tiered_down) {
    native_module->SetTieringState(kTieredDown);
  }

  // Record memory protection key support.
  if (!isolate_info->pku_support_sampled) {
    isolate_info->pku_support_sampled = true;
    auto* histogram =
        isolate->counters()->wasm_memory_protection_keys_support();
    bool has_mpk = WasmCodeManager::HasMemoryProtectionKeySupport();
    histogram->AddSample(has_mpk ? 1 : 0);
  }

  isolate->counters()->wasm_modules_per_isolate()->AddSample(
      static_cast<int>(isolate_info->native_modules.size()));
  isolate->counters()->wasm_modules_per_engine()->AddSample(
      static_cast<int>(native_modules_.size()));
  return native_module;
}

std::shared_ptr<NativeModule> WasmEngine::MaybeGetNativeModule(
    ModuleOrigin origin, base::Vector<const uint8_t> wire_bytes,
    Isolate* isolate) {
  TRACE_EVENT1("v8.wasm", "wasm.GetNativeModuleFromCache", "wire_bytes",
               wire_bytes.size());
  std::shared_ptr<NativeModule> native_module =
      native_module_cache_.MaybeGetNativeModule(origin, wire_bytes);
  bool recompile_module = false;
  if (native_module) {
    TRACE_EVENT0("v8.wasm", "CacheHit");
    base::MutexGuard guard(&mutex_);
    auto& native_module_info = native_modules_[native_module.get()];
    if (!native_module_info) {
      native_module_info = std::make_unique<NativeModuleInfo>(native_module);
    }
    native_module_info->isolates.insert(isolate);
    isolates_[isolate]->native_modules.insert(native_module.get());
    if (isolates_[isolate]->keep_tiered_down) {
      native_module->SetTieringState(kTieredDown);
      recompile_module = true;
    }
  }
  // Potentially recompile the module for tier down, after releasing the mutex.
  if (recompile_module) native_module->RecompileForTiering();
  return native_module;
}

bool WasmEngine::UpdateNativeModuleCache(
    bool error, std::shared_ptr<NativeModule>* native_module,
    Isolate* isolate) {
  // Pass {native_module} by value here to keep it alive until at least after
  // we returned from {Update}. Otherwise, we might {Erase} it inside {Update}
  // which would lock the mutex twice.
  auto prev = native_module->get();
  *native_module = native_module_cache_.Update(*native_module, error);

  if (prev == native_module->get()) return true;

  bool recompile_module = false;
  {
    base::MutexGuard guard(&mutex_);
    DCHECK_EQ(1, native_modules_.count(native_module->get()));
    native_modules_[native_module->get()]->isolates.insert(isolate);
    DCHECK_EQ(1, isolates_.count(isolate));
    isolates_[isolate]->native_modules.insert(native_module->get());
    if (isolates_[isolate]->keep_tiered_down) {
      native_module->get()->SetTieringState(kTieredDown);
      recompile_module = true;
    }
  }
  // Potentially recompile the module for tier down, after releasing the mutex.
  if (recompile_module) native_module->get()->RecompileForTiering();
  return false;
}

bool WasmEngine::GetStreamingCompilationOwnership(size_t prefix_hash) {
  TRACE_EVENT0("v8.wasm", "wasm.GetStreamingCompilationOwnership");
  if (native_module_cache_.GetStreamingCompilationOwnership(prefix_hash)) {
    return true;
  }
  // This is only a marker, not for tracing execution time. There should be a
  // later "wasm.GetNativeModuleFromCache" event for trying to get the module
  // from the cache.
  TRACE_EVENT0("v8.wasm", "CacheHit");
  return false;
}

void WasmEngine::StreamingCompilationFailed(size_t prefix_hash) {
  native_module_cache_.StreamingCompilationFailed(prefix_hash);
}

void WasmEngine::FreeNativeModule(NativeModule* native_module) {
  base::MutexGuard guard(&mutex_);
  auto module = native_modules_.find(native_module);
  DCHECK_NE(native_modules_.end(), module);
  for (Isolate* isolate : module->second->isolates) {
    DCHECK_EQ(1, isolates_.count(isolate));
    IsolateInfo* info = isolates_[isolate].get();
    DCHECK_EQ(1, info->native_modules.count(native_module));
    info->native_modules.erase(native_module);
    info->scripts.erase(native_module);
    // If there are {WasmCode} objects of the deleted {NativeModule}
    // outstanding to be logged in this isolate, remove them. Decrementing the
    // ref count is not needed, since the {NativeModule} dies anyway.
    for (auto& log_entry : info->code_to_log) {
      auto part_of_native_module = [native_module](WasmCode* code) {
        return code->native_module() == native_module;
      };
      std::vector<WasmCode*>& code = log_entry.second.code;
      auto new_end =
          std::remove_if(code.begin(), code.end(), part_of_native_module);
      code.erase(new_end, code.end());
    }
    // Now remove empty entries in {code_to_log}.
    for (auto it = info->code_to_log.begin(), end = info->code_to_log.end();
         it != end;) {
      if (it->second.code.empty()) {
        it = info->code_to_log.erase(it);
      } else {
        ++it;
      }
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
  native_modules_.erase(module);
}

void WasmEngine::ReportLiveCodeForGC(Isolate* isolate,
                                     base::Vector<WasmCode*> live_code) {
  TRACE_EVENT0("v8.wasm", "wasm.ReportLiveCodeForGC");
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

namespace {
void ReportLiveCodeFromFrameForGC(
    StackFrame* frame, std::unordered_set<wasm::WasmCode*>& live_wasm_code) {
  if (frame->type() != StackFrame::WASM) return;
  live_wasm_code.insert(WasmFrame::cast(frame)->wasm_code());
#if V8_TARGET_ARCH_X64
    if (WasmFrame::cast(frame)->wasm_code()->for_debugging()) {
      Address osr_target = base::Memory<Address>(WasmFrame::cast(frame)->fp() -
                                                 kOSRTargetOffset);
      if (osr_target) {
        WasmCode* osr_code = GetWasmCodeManager()->LookupCode(osr_target);
        DCHECK_NOT_NULL(osr_code);
        live_wasm_code.insert(osr_code);
      }
    }
#endif
}
}  // namespace

void WasmEngine::ReportLiveCodeFromStackForGC(Isolate* isolate) {
  wasm::WasmCodeRefScope code_ref_scope;
  std::unordered_set<wasm::WasmCode*> live_wasm_code;
  if (v8_flags.experimental_wasm_stack_switching) {
    wasm::StackMemory* current = isolate->wasm_stacks();
    DCHECK_NOT_NULL(current);
    do {
      if (current->IsActive()) {
        // The active stack's jump buffer does not match the current state, use
        // the thread info below instead.
        current = current->next();
        continue;
      }
      for (StackFrameIterator it(isolate, current); !it.done(); it.Advance()) {
        StackFrame* const frame = it.frame();
        ReportLiveCodeFromFrameForGC(frame, live_wasm_code);
      }
      current = current->next();
    } while (current != isolate->wasm_stacks());
  }
  for (StackFrameIterator it(isolate); !it.done(); it.Advance()) {
    StackFrame* const frame = it.frame();
    ReportLiveCodeFromFrameForGC(frame, live_wasm_code);
  }

  CheckNoArchivedThreads(isolate);

  ReportLiveCodeForGC(
      isolate, base::OwnedVector<WasmCode*>::Of(live_wasm_code).as_vector());
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
  if (v8_flags.wasm_code_gc) {
    // Trigger a GC if 64kB plus 10% of committed code are potentially dead.
    size_t dead_code_limit =
        v8_flags.stress_wasm_code_gc
            ? 0
            : 64 * KB + GetWasmCodeManager()->committed_code_space() / 10;
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
  TRACE_EVENT0("v8.wasm", "wasm.FreeDeadCode");
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
    native_module->FreeCode(base::VectorOf(code_vec));
  }
}

Handle<Script> WasmEngine::GetOrCreateScript(
    Isolate* isolate, const std::shared_ptr<NativeModule>& native_module,
    base::Vector<const char> source_url) {
  {
    base::MutexGuard guard(&mutex_);
    DCHECK_EQ(1, isolates_.count(isolate));
    auto& scripts = isolates_[isolate]->scripts;
    auto it = scripts.find(native_module.get());
    if (it != scripts.end()) {
      Handle<Script> weak_global_handle = it->second.handle();
      if (weak_global_handle.is_null()) {
        scripts.erase(it);
      } else {
        return Handle<Script>::New(*weak_global_handle, isolate);
      }
    }
  }
  // Temporarily release the mutex to let the GC collect native modules.
  auto script = CreateWasmScript(isolate, native_module, source_url);
  {
    base::MutexGuard guard(&mutex_);
    DCHECK_EQ(1, isolates_.count(isolate));
    auto& scripts = isolates_[isolate]->scripts;
    DCHECK_EQ(0, scripts.count(native_module.get()));
    scripts.emplace(native_module.get(), WeakScriptHandle(script));
    return script;
  }
}

std::shared_ptr<OperationsBarrier>
WasmEngine::GetBarrierForBackgroundCompile() {
  return operations_barrier_;
}

namespace {
void SampleExceptionEvent(base::ElapsedTimer* timer, TimedHistogram* counter) {
  if (!timer->IsStarted()) {
    timer->Start();
    return;
  }
  counter->AddSample(static_cast<int>(timer->Elapsed().InMilliseconds()));
  timer->Restart();
}
}  // namespace

void WasmEngine::SampleThrowEvent(Isolate* isolate) {
  base::MutexGuard guard(&mutex_);
  IsolateInfo* isolate_info = isolates_[isolate].get();
  int& throw_count = isolate_info->throw_count;
  // To avoid an int overflow, clip the count to the histogram's max value.
  throw_count =
      std::min(throw_count + 1, isolate->counters()->wasm_throw_count()->max());
  isolate->counters()->wasm_throw_count()->AddSample(throw_count);
  SampleExceptionEvent(&isolate_info->throw_timer,
                       isolate->counters()->wasm_time_between_throws());
}

void WasmEngine::SampleRethrowEvent(Isolate* isolate) {
  base::MutexGuard guard(&mutex_);
  IsolateInfo* isolate_info = isolates_[isolate].get();
  int& rethrow_count = isolate_info->rethrow_count;
  // To avoid an int overflow, clip the count to the histogram's max value.
  rethrow_count = std::min(rethrow_count + 1,
                           isolate->counters()->wasm_rethrow_count()->max());
  isolate->counters()->wasm_rethrow_count()->AddSample(rethrow_count);
  SampleExceptionEvent(&isolate_info->rethrow_timer,
                       isolate->counters()->wasm_time_between_rethrows());
}

void WasmEngine::SampleCatchEvent(Isolate* isolate) {
  base::MutexGuard guard(&mutex_);
  IsolateInfo* isolate_info = isolates_[isolate].get();
  int& catch_count = isolate_info->catch_count;
  // To avoid an int overflow, clip the count to the histogram's max value.
  catch_count =
      std::min(catch_count + 1, isolate->counters()->wasm_catch_count()->max());
  isolate->counters()->wasm_catch_count()->AddSample(catch_count);
  SampleExceptionEvent(&isolate_info->catch_timer,
                       isolate->counters()->wasm_time_between_catch());
}

void WasmEngine::TriggerGC(int8_t gc_sequence_index) {
  DCHECK(!mutex_.TryLock());
  DCHECK_NULL(current_gc_info_);
  DCHECK(v8_flags.wasm_code_gc);
  new_potentially_dead_code_size_ = 0;
  current_gc_info_.reset(new CurrentGCInfo(gc_sequence_index));
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
      "Starting GC (nr %d). Number of potentially dead code objects: %zu\n",
      current_gc_info_->gc_sequence_index, current_gc_info_->dead_code.size());
  // Ensure that there are outstanding isolates that will eventually finish this
  // GC. If there are no outstanding isolates, we finish the GC immediately.
  PotentiallyFinishCurrentGC();
  DCHECK(current_gc_info_ == nullptr ||
         !current_gc_info_->outstanding_isolates.empty());
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

  TRACE_CODE_GC("Found %zu dead code objects, freed %zu.\n",
                current_gc_info_->dead_code.size(), num_freed);
  USE(num_freed);

  int8_t next_gc_sequence_index = current_gc_info_->next_gc_sequence_index;
  current_gc_info_.reset();
  if (next_gc_sequence_index != 0) TriggerGC(next_gc_sequence_index);
}

namespace {

struct GlobalWasmState {
  // Note: The order of fields is important here, as the WasmEngine's destructor
  // must run first. It contains a barrier which ensures that background threads
  // finished, and that has to happen before the WasmCodeManager gets destroyed.
  WasmCodeManager code_manager;
  WasmEngine engine;
};

GlobalWasmState* global_wasm_state = nullptr;

}  // namespace

// static
void WasmEngine::InitializeOncePerProcess() {
  DCHECK_NULL(global_wasm_state);
  global_wasm_state = new GlobalWasmState();
}

// static
void WasmEngine::GlobalTearDown() {
  // Note: This can be called multiple times in a row (see
  // test-api/InitializeAndDisposeMultiple). This is fine, as
  // {global_wasm_engine} will be nullptr then.
  delete global_wasm_state;
  global_wasm_state = nullptr;
}

WasmEngine* GetWasmEngine() {
  DCHECK_NOT_NULL(global_wasm_state);
  return &global_wasm_state->engine;
}

WasmCodeManager* GetWasmCodeManager() {
  DCHECK_NOT_NULL(global_wasm_state);
  return &global_wasm_state->code_manager;
}

// {max_mem_pages} is declared in wasm-limits.h.
uint32_t max_mem32_pages() {
  static_assert(
      kV8MaxWasmMemory32Pages * kWasmPageSize <= JSArrayBuffer::kMaxByteLength,
      "Wasm memories must not be bigger than JSArrayBuffers");
  static_assert(kV8MaxWasmMemory32Pages <= kMaxUInt32);
  return std::min(uint32_t{kV8MaxWasmMemory32Pages},
                  v8_flags.wasm_max_mem_pages.value());
}

uint32_t max_mem64_pages() {
  static_assert(
      kV8MaxWasmMemory64Pages * kWasmPageSize <= JSArrayBuffer::kMaxByteLength,
      "Wasm memories must not be bigger than JSArrayBuffers");
  static_assert(kV8MaxWasmMemory64Pages <= kMaxUInt32);
  return std::min(uint32_t{kV8MaxWasmMemory64Pages},
                  v8_flags.wasm_max_mem_pages.value());
}

// {max_table_init_entries} is declared in wasm-limits.h.
uint32_t max_table_init_entries() {
  return std::min(uint32_t{kV8MaxWasmTableInitEntries},
                  v8_flags.wasm_max_table_size.value());
}

// {max_module_size} is declared in wasm-limits.h.
size_t max_module_size() {
  // Clamp the value of --wasm-max-module-size between 16 and just below 2GB.
  constexpr size_t kMin = 16;
  constexpr size_t kMax = RoundDown<kSystemPointerSize>(size_t{kMaxInt});
  static_assert(kMin <= kV8MaxWasmModuleSize && kV8MaxWasmModuleSize <= kMax);
  return std::clamp(v8_flags.wasm_max_module_size.value(), kMin, kMax);
}

#undef TRACE_CODE_GC

}  // namespace v8::internal::wasm
