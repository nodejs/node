// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/wasm/wasm-engine.h"

#include "src/objects-inl.h"
#include "src/wasm/module-compiler.h"
#include "src/wasm/module-decoder.h"
#include "src/wasm/streaming-decoder.h"
#include "src/wasm/wasm-objects.h"

namespace v8 {
namespace internal {
namespace wasm {

bool WasmEngine::SyncValidate(Isolate* isolate, const ModuleWireBytes& bytes) {
  // TODO(titzer): remove dependency on the isolate.
  if (bytes.start() == nullptr || bytes.length() == 0) return false;
  ModuleResult result = SyncDecodeWasmModule(isolate, bytes.start(),
                                             bytes.end(), true, kWasmOrigin);
  return result.ok();
}

MaybeHandle<WasmModuleObject> WasmEngine::SyncCompileTranslatedAsmJs(
    Isolate* isolate, ErrorThrower* thrower, const ModuleWireBytes& bytes,
    Handle<Script> asm_js_script,
    Vector<const byte> asm_js_offset_table_bytes) {
  ModuleResult result = SyncDecodeWasmModule(isolate, bytes.start(),
                                             bytes.end(), false, kAsmJsOrigin);
  CHECK(!result.failed());

  // Transfer ownership of the WasmModule to the {Managed<WasmModule>} generated
  // in {CompileToModuleObject}.
  return CompileToModuleObject(isolate, thrower, std::move(result.val), bytes,
                               asm_js_script, asm_js_offset_table_bytes);
}

MaybeHandle<WasmModuleObject> WasmEngine::SyncCompile(
    Isolate* isolate, ErrorThrower* thrower, const ModuleWireBytes& bytes) {
  ModuleResult result = SyncDecodeWasmModule(isolate, bytes.start(),
                                             bytes.end(), false, kWasmOrigin);
  if (result.failed()) {
    thrower->CompileFailed("Wasm decoding failed", result);
    return {};
  }

  // Transfer ownership of the WasmModule to the {Managed<WasmModule>} generated
  // in {CompileToModuleObject}.
  return CompileToModuleObject(isolate, thrower, std::move(result.val), bytes,
                               Handle<Script>(), Vector<const byte>());
}

MaybeHandle<WasmInstanceObject> WasmEngine::SyncInstantiate(
    Isolate* isolate, ErrorThrower* thrower,
    Handle<WasmModuleObject> module_object, MaybeHandle<JSReceiver> imports,
    MaybeHandle<JSArrayBuffer> memory) {
  return InstantiateToInstanceObject(isolate, thrower, module_object, imports,
                                     memory);
}

void WasmEngine::AsyncInstantiate(Isolate* isolate, Handle<JSPromise> promise,
                                  Handle<WasmModuleObject> module_object,
                                  MaybeHandle<JSReceiver> imports) {
  ErrorThrower thrower(isolate, nullptr);
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
    Handle<WasmInstanceObject> instance = instance_object.ToHandleChecked();
    MaybeHandle<Object> result = JSPromise::Resolve(promise, instance);
    CHECK_EQ(result.is_null(), isolate->has_pending_exception());
    return;
  }

  // We either have a pending exception (if the start function threw), or an
  // exception in the ErrorThrower.
  DCHECK_EQ(1, isolate->has_pending_exception() + thrower.error());
  if (thrower.error()) {
    MaybeHandle<Object> result = JSPromise::Reject(promise, thrower.Reify());
    CHECK_EQ(result.is_null(), isolate->has_pending_exception());
    return;
  }
  // The start function has thrown an exception. We have to move the
  // exception to the promise chain.
  Handle<Object> exception(isolate->pending_exception(), isolate);
  isolate->clear_pending_exception();
  DCHECK(*isolate->external_caught_exception_address());
  *isolate->external_caught_exception_address() = false;
  MaybeHandle<Object> result = JSPromise::Reject(promise, exception);
  CHECK_EQ(result.is_null(), isolate->has_pending_exception());
}

void WasmEngine::AsyncCompile(Isolate* isolate, Handle<JSPromise> promise,
                              const ModuleWireBytes& bytes, bool is_shared) {
  if (!FLAG_wasm_async_compilation) {
    // Asynchronous compilation disabled; fall back on synchronous compilation.
    ErrorThrower thrower(isolate, "WasmCompile");
    MaybeHandle<WasmModuleObject> module_object;
    if (is_shared) {
      // Make a copy of the wire bytes to avoid concurrent modification.
      std::unique_ptr<uint8_t[]> copy(new uint8_t[bytes.length()]);
      memcpy(copy.get(), bytes.start(), bytes.length());
      i::wasm::ModuleWireBytes bytes_copy(copy.get(),
                                          copy.get() + bytes.length());
      module_object = SyncCompile(isolate, &thrower, bytes_copy);
    } else {
      // The wire bytes are not shared, OK to use them directly.
      module_object = SyncCompile(isolate, &thrower, bytes);
    }
    if (thrower.error()) {
      MaybeHandle<Object> result = JSPromise::Reject(promise, thrower.Reify());
      CHECK_EQ(result.is_null(), isolate->has_pending_exception());
      return;
    }
    Handle<WasmModuleObject> module = module_object.ToHandleChecked();
    MaybeHandle<Object> result = JSPromise::Resolve(promise, module);
    CHECK_EQ(result.is_null(), isolate->has_pending_exception());
    return;
  }

  if (FLAG_wasm_test_streaming) {
    std::shared_ptr<StreamingDecoder> streaming_decoder =
        isolate->wasm_engine()
            ->StartStreamingCompilation(isolate, handle(isolate->context()),
                                        promise);
    streaming_decoder->OnBytesReceived(bytes.module_bytes());
    streaming_decoder->Finish();
    return;
  }
  // Make a copy of the wire bytes in case the user program changes them
  // during asynchronous compilation.
  std::unique_ptr<byte[]> copy(new byte[bytes.length()]);
  memcpy(copy.get(), bytes.start(), bytes.length());

  AsyncCompileJob* job =
      CreateAsyncCompileJob(isolate, std::move(copy), bytes.length(),
                            handle(isolate->context()), promise);
  job->Start();
}

std::shared_ptr<StreamingDecoder> WasmEngine::StartStreamingCompilation(
    Isolate* isolate, Handle<Context> context, Handle<JSPromise> promise) {
  AsyncCompileJob* job = CreateAsyncCompileJob(
      isolate, std::unique_ptr<byte[]>(nullptr), 0, context, promise);
  return job->CreateStreamingDecoder();
}

void WasmEngine::Register(CancelableTaskManager* task_manager) {
  task_managers_.emplace_back(task_manager);
}

void WasmEngine::Unregister(CancelableTaskManager* task_manager) {
  task_managers_.remove(task_manager);
}

AsyncCompileJob* WasmEngine::CreateAsyncCompileJob(
    Isolate* isolate, std::unique_ptr<byte[]> bytes_copy, size_t length,
    Handle<Context> context, Handle<JSPromise> promise) {
  AsyncCompileJob* job = new AsyncCompileJob(isolate, std::move(bytes_copy),
                                             length, context, promise);
  // Pass ownership to the unique_ptr in {jobs_}.
  jobs_[job] = std::unique_ptr<AsyncCompileJob>(job);
  return job;
}

std::unique_ptr<AsyncCompileJob> WasmEngine::RemoveCompileJob(
    AsyncCompileJob* job) {
  auto item = jobs_.find(job);
  DCHECK(item != jobs_.end());
  std::unique_ptr<AsyncCompileJob> result = std::move(item->second);
  jobs_.erase(item);
  return result;
}

void WasmEngine::AbortAllCompileJobs() {
  // Iterate over a copy of {jobs_}, because {job->Abort} modifies {jobs_}.
  std::vector<AsyncCompileJob*> copy;
  copy.reserve(jobs_.size());

  for (auto& entry : jobs_) copy.push_back(entry.first);

  for (auto* job : copy) job->Abort();
}

void WasmEngine::TearDown() {
  // Cancel all registered task managers.
  for (auto task_manager : task_managers_) {
    task_manager->CancelAndWait();
  }

  // Cancel all AsyncCompileJobs.
  jobs_.clear();
}

}  // namespace wasm
}  // namespace internal
}  // namespace v8
