// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/wasm/wasm-engine.h"

#include "src/code-tracer.h"
#include "src/compilation-statistics.h"
#include "src/objects-inl.h"
#include "src/objects/js-promise.h"
#include "src/wasm/function-compiler.h"
#include "src/wasm/module-compiler.h"
#include "src/wasm/module-decoder.h"
#include "src/wasm/streaming-decoder.h"
#include "src/wasm/wasm-objects-inl.h"

namespace v8 {
namespace internal {
namespace wasm {

WasmEngine::WasmEngine()
    : code_manager_(&memory_tracker_, kMaxWasmCodeMemory) {}

WasmEngine::~WasmEngine() {
  // All AsyncCompileJobs have been canceled.
  DCHECK(jobs_.empty());
  // All Isolates have been deregistered.
  DCHECK(isolates_.empty());
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

MaybeHandle<WasmModuleObject> WasmEngine::SyncCompileTranslatedAsmJs(
    Isolate* isolate, ErrorThrower* thrower, const ModuleWireBytes& bytes,
    Handle<Script> asm_js_script,
    Vector<const byte> asm_js_offset_table_bytes) {
  ModuleResult result =
      DecodeWasmModule(kAsmjsWasmFeatures, bytes.start(), bytes.end(), false,
                       kAsmJsOrigin, isolate->counters(), allocator());
  CHECK(!result.failed());

  // Transfer ownership of the WasmModule to the {Managed<WasmModule>} generated
  // in {CompileToModuleObject}.
  return CompileToModuleObject(isolate, kAsmjsWasmFeatures, thrower,
                               std::move(result.val), bytes, asm_js_script,
                               asm_js_offset_table_bytes);
}

MaybeHandle<WasmModuleObject> WasmEngine::SyncCompile(
    Isolate* isolate, const WasmFeatures& enabled, ErrorThrower* thrower,
    const ModuleWireBytes& bytes) {
  ModuleResult result =
      DecodeWasmModule(enabled, bytes.start(), bytes.end(), false, kWasmOrigin,
                       isolate->counters(), allocator());
  if (result.failed()) {
    thrower->CompileFailed("Wasm decoding failed", result);
    return {};
  }

  // Transfer ownership of the WasmModule to the {Managed<WasmModule>} generated
  // in {CompileToModuleObject}.
  return CompileToModuleObject(isolate, enabled, thrower, std::move(result.val),
                               bytes, Handle<Script>(), Vector<const byte>());
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
  ErrorThrower thrower(isolate, "WebAssembly Instantiation");
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

bool WasmEngine::CompileFunction(Isolate* isolate, NativeModule* native_module,
                                 uint32_t function_index, ExecutionTier tier) {
  ErrorThrower thrower(isolate, "Manually requested tier up");
  // Note we assume that "one-off" compilations can discard detected features.
  WasmFeatures detected = kNoWasmFeatures;
  WasmCode* ret = WasmCompilationUnit::CompileWasmFunction(
      isolate, native_module, &detected, &thrower,
      GetModuleEnv(native_module->compilation_state()),
      &native_module->module()->functions[function_index], tier);
  return ret != nullptr;
}

std::shared_ptr<NativeModule> WasmEngine::ExportNativeModule(
    Handle<WasmModuleObject> module_object) {
  return module_object->managed_native_module()->get();
}

Handle<WasmModuleObject> WasmEngine::ImportNativeModule(
    Isolate* isolate, std::shared_ptr<NativeModule> shared_module) {
  CHECK_EQ(code_manager(), shared_module->code_manager());
  Vector<const byte> wire_bytes = shared_module->wire_bytes();
  const WasmModule* module = shared_module->module();
  Handle<Script> script =
      CreateWasmScript(isolate, wire_bytes, module->source_map_url);
  Handle<WasmModuleObject> module_object =
      WasmModuleObject::New(isolate, std::move(shared_module), script);

  // TODO(6792): Wrappers below might be cloned using {Factory::CopyCode}.
  // This requires unlocking the code space here. This should eventually be
  // moved into the allocator.
  CodeSpaceMemoryModificationScope modification_scope(isolate->heap());
  CompileJsToWasmWrappers(isolate, module_object);
  return module_object;
}

CompilationStatistics* WasmEngine::GetOrCreateTurboStatistics() {
  base::LockGuard<base::Mutex> guard(&mutex_);
  if (compilation_stats_ == nullptr) {
    compilation_stats_.reset(new CompilationStatistics());
  }
  return compilation_stats_.get();
}

void WasmEngine::DumpAndResetTurboStatistics() {
  base::LockGuard<base::Mutex> guard(&mutex_);
  if (compilation_stats_ != nullptr) {
    StdoutStream os;
    os << AsPrintableStatistics{*compilation_stats_.get(), false} << std::endl;
  }
  compilation_stats_.reset();
}

CodeTracer* WasmEngine::GetCodeTracer() {
  base::LockGuard<base::Mutex> guard(&mutex_);
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
  // Pass ownership to the unique_ptr in {jobs_}.
  base::LockGuard<base::Mutex> guard(&mutex_);
  jobs_[job] = std::unique_ptr<AsyncCompileJob>(job);
  return job;
}

std::unique_ptr<AsyncCompileJob> WasmEngine::RemoveCompileJob(
    AsyncCompileJob* job) {
  base::LockGuard<base::Mutex> guard(&mutex_);
  auto item = jobs_.find(job);
  DCHECK(item != jobs_.end());
  std::unique_ptr<AsyncCompileJob> result = std::move(item->second);
  jobs_.erase(item);
  return result;
}

bool WasmEngine::HasRunningCompileJob(Isolate* isolate) {
  base::LockGuard<base::Mutex> guard(&mutex_);
  DCHECK_EQ(1, isolates_.count(isolate));
  for (auto& entry : jobs_) {
    if (entry.first->isolate() == isolate) return true;
  }
  return false;
}

void WasmEngine::DeleteCompileJobsOnIsolate(Isolate* isolate) {
  base::LockGuard<base::Mutex> guard(&mutex_);
  DCHECK_EQ(1, isolates_.count(isolate));
  for (auto it = jobs_.begin(); it != jobs_.end();) {
    if (it->first->isolate() == isolate) {
      it = jobs_.erase(it);
    } else {
      ++it;
    }
  }
}

void WasmEngine::AddIsolate(Isolate* isolate) {
  base::LockGuard<base::Mutex> guard(&mutex_);
  DCHECK_EQ(0, isolates_.count(isolate));
  isolates_.insert(isolate);
}

void WasmEngine::RemoveIsolate(Isolate* isolate) {
  base::LockGuard<base::Mutex> guard(&mutex_);
  DCHECK_EQ(1, isolates_.count(isolate));
  isolates_.erase(isolate);
}

namespace {

struct WasmEnginePointerConstructTrait final {
  static void Construct(void* raw_ptr) {
    auto engine_ptr = reinterpret_cast<std::shared_ptr<WasmEngine>*>(raw_ptr);
    *engine_ptr = std::shared_ptr<WasmEngine>();
  }
};

// Holds the global shared pointer to the single {WasmEngine} that is intended
// to be shared among Isolates within the same process. The {LazyStaticInstance}
// here is required because {std::shared_ptr} has a non-trivial initializer.
base::LazyStaticInstance<std::shared_ptr<WasmEngine>,
                         WasmEnginePointerConstructTrait>::type
    global_wasm_engine;

}  // namespace

// static
void WasmEngine::InitializeOncePerProcess() {
  if (!FLAG_wasm_shared_engine) return;
  global_wasm_engine.Pointer()->reset(new WasmEngine());
}

// static
void WasmEngine::GlobalTearDown() {
  if (!FLAG_wasm_shared_engine) return;
  global_wasm_engine.Pointer()->reset();
}

// static
std::shared_ptr<WasmEngine> WasmEngine::GetWasmEngine() {
  if (FLAG_wasm_shared_engine) return global_wasm_engine.Get();
  return std::shared_ptr<WasmEngine>(new WasmEngine());
}

}  // namespace wasm
}  // namespace internal
}  // namespace v8
