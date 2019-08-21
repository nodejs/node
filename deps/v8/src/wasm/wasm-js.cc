// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/wasm/wasm-js.h"

#include <cinttypes>
#include <cstring>

#include "src/api/api-inl.h"
#include "src/api/api-natives.h"
#include "src/ast/ast.h"
#include "src/base/overflowing-math.h"
#include "src/common/assert-scope.h"
#include "src/execution/execution.h"
#include "src/execution/isolate.h"
#include "src/handles/handles.h"
#include "src/heap/factory.h"
#include "src/init/v8.h"
#include "src/objects/js-promise-inl.h"
#include "src/objects/objects-inl.h"
#include "src/objects/templates.h"
#include "src/parsing/parse-info.h"
#include "src/tasks/task-utils.h"
#include "src/trap-handler/trap-handler.h"
#include "src/wasm/streaming-decoder.h"
#include "src/wasm/wasm-engine.h"
#include "src/wasm/wasm-limits.h"
#include "src/wasm/wasm-memory.h"
#include "src/wasm/wasm-objects-inl.h"
#include "src/wasm/wasm-serialization.h"

using v8::internal::wasm::ErrorThrower;

namespace v8 {

class WasmStreaming::WasmStreamingImpl {
 public:
  WasmStreamingImpl(
      Isolate* isolate, const char* api_method_name,
      std::shared_ptr<internal::wasm::CompilationResultResolver> resolver)
      : isolate_(isolate), resolver_(std::move(resolver)) {
    i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate_);
    auto enabled_features = i::wasm::WasmFeaturesFromIsolate(i_isolate);
    streaming_decoder_ = i_isolate->wasm_engine()->StartStreamingCompilation(
        i_isolate, enabled_features, handle(i_isolate->context(), i_isolate),
        api_method_name, resolver_);
  }

  void OnBytesReceived(const uint8_t* bytes, size_t size) {
    streaming_decoder_->OnBytesReceived(i::VectorOf(bytes, size));
  }
  void Finish() { streaming_decoder_->Finish(); }

  void Abort(MaybeLocal<Value> exception) {
    i::HandleScope scope(reinterpret_cast<i::Isolate*>(isolate_));
    streaming_decoder_->Abort();

    // If no exception value is provided, we do not reject the promise. This can
    // happen when streaming compilation gets aborted when no script execution
    // is allowed anymore, e.g. when a browser tab gets refreshed.
    if (exception.IsEmpty()) return;

    resolver_->OnCompilationFailed(
        Utils::OpenHandle(*exception.ToLocalChecked()));
  }

  bool SetCompiledModuleBytes(const uint8_t* bytes, size_t size) {
    if (!i::wasm::IsSupportedVersion({bytes, size})) return false;
    return streaming_decoder_->SetCompiledModuleBytes({bytes, size});
  }

  void SetClient(std::shared_ptr<Client> client) {
    // There are no other event notifications so just pass client to decoder.
    // Wrap the client with a callback to trigger the callback in a new
    // foreground task.
    i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate_);
    v8::Platform* platform = i::V8::GetCurrentPlatform();
    std::shared_ptr<TaskRunner> foreground_task_runner =
        platform->GetForegroundTaskRunner(isolate_);
    streaming_decoder_->SetModuleCompiledCallback(
        [client, i_isolate, foreground_task_runner](
            const std::shared_ptr<i::wasm::NativeModule>& native_module) {
          foreground_task_runner->PostTask(
              i::MakeCancelableTask(i_isolate, [client, native_module] {
                client->OnModuleCompiled(Utils::Convert(native_module));
              }));
        });
  }

 private:
  Isolate* const isolate_;
  std::shared_ptr<internal::wasm::StreamingDecoder> streaming_decoder_;
  std::shared_ptr<internal::wasm::CompilationResultResolver> resolver_;
};

WasmStreaming::WasmStreaming(std::unique_ptr<WasmStreamingImpl> impl)
    : impl_(std::move(impl)) {}

// The destructor is defined here because we have a unique_ptr with forward
// declaration.
WasmStreaming::~WasmStreaming() = default;

void WasmStreaming::OnBytesReceived(const uint8_t* bytes, size_t size) {
  impl_->OnBytesReceived(bytes, size);
}

void WasmStreaming::Finish() { impl_->Finish(); }

void WasmStreaming::Abort(MaybeLocal<Value> exception) {
  impl_->Abort(exception);
}

bool WasmStreaming::SetCompiledModuleBytes(const uint8_t* bytes, size_t size) {
  return impl_->SetCompiledModuleBytes(bytes, size);
}

void WasmStreaming::SetClient(std::shared_ptr<Client> client) {
  impl_->SetClient(client);
}

// static
std::shared_ptr<WasmStreaming> WasmStreaming::Unpack(Isolate* isolate,
                                                     Local<Value> value) {
  i::HandleScope scope(reinterpret_cast<i::Isolate*>(isolate));
  auto managed =
      i::Handle<i::Managed<WasmStreaming>>::cast(Utils::OpenHandle(*value));
  return managed->get();
}

namespace {

#define ASSIGN(type, var, expr)                      \
  Local<type> var;                                   \
  do {                                               \
    if (!expr.ToLocal(&var)) {                       \
      DCHECK(i_isolate->has_scheduled_exception());  \
      return;                                        \
    } else {                                         \
      DCHECK(!i_isolate->has_scheduled_exception()); \
    }                                                \
  } while (false)

// Like an ErrorThrower, but turns all pending exceptions into scheduled
// exceptions when going out of scope. Use this in API methods.
// Note that pending exceptions are not necessarily created by the ErrorThrower,
// but e.g. by the wasm start function. There might also be a scheduled
// exception, created by another API call (e.g. v8::Object::Get). But there
// should never be both pending and scheduled exceptions.
class ScheduledErrorThrower : public ErrorThrower {
 public:
  ScheduledErrorThrower(i::Isolate* isolate, const char* context)
      : ErrorThrower(isolate, context) {}

  ~ScheduledErrorThrower();
};

ScheduledErrorThrower::~ScheduledErrorThrower() {
  // There should never be both a pending and a scheduled exception.
  DCHECK(!isolate()->has_scheduled_exception() ||
         !isolate()->has_pending_exception());
  // Don't throw another error if there is already a scheduled error.
  if (isolate()->has_scheduled_exception()) {
    Reset();
  } else if (isolate()->has_pending_exception()) {
    Reset();
    isolate()->OptionalRescheduleException(false);
  } else if (error()) {
    isolate()->ScheduleThrow(*Reify());
  }
}

i::Handle<i::String> v8_str(i::Isolate* isolate, const char* str) {
  return isolate->factory()->NewStringFromAsciiChecked(str);
}
Local<String> v8_str(Isolate* isolate, const char* str) {
  return Utils::ToLocal(v8_str(reinterpret_cast<i::Isolate*>(isolate), str));
}

#define GET_FIRST_ARGUMENT_AS(Type)                                  \
  i::MaybeHandle<i::Wasm##Type##Object> GetFirstArgumentAs##Type(    \
      const v8::FunctionCallbackInfo<v8::Value>& args,               \
      ErrorThrower* thrower) {                                       \
    i::Handle<i::Object> arg0 = Utils::OpenHandle(*args[0]);         \
    if (!arg0->IsWasm##Type##Object()) {                             \
      thrower->TypeError("Argument 0 must be a WebAssembly." #Type); \
      return {};                                                     \
    }                                                                \
    Local<Object> obj = Local<Object>::Cast(args[0]);                \
    return i::Handle<i::Wasm##Type##Object>::cast(                   \
        v8::Utils::OpenHandle(*obj));                                \
  }

GET_FIRST_ARGUMENT_AS(Module)
GET_FIRST_ARGUMENT_AS(Memory)
GET_FIRST_ARGUMENT_AS(Table)
GET_FIRST_ARGUMENT_AS(Global)

#undef GET_FIRST_ARGUMENT_AS

i::wasm::ModuleWireBytes GetFirstArgumentAsBytes(
    const v8::FunctionCallbackInfo<v8::Value>& args, ErrorThrower* thrower,
    bool* is_shared) {
  const uint8_t* start = nullptr;
  size_t length = 0;
  v8::Local<v8::Value> source = args[0];
  if (source->IsArrayBuffer()) {
    // A raw array buffer was passed.
    Local<ArrayBuffer> buffer = Local<ArrayBuffer>::Cast(source);
    ArrayBuffer::Contents contents = buffer->GetContents();

    start = reinterpret_cast<const uint8_t*>(contents.Data());
    length = contents.ByteLength();
    *is_shared = buffer->IsSharedArrayBuffer();
  } else if (source->IsTypedArray()) {
    // A TypedArray was passed.
    Local<TypedArray> array = Local<TypedArray>::Cast(source);
    Local<ArrayBuffer> buffer = array->Buffer();

    ArrayBuffer::Contents contents = buffer->GetContents();

    start =
        reinterpret_cast<const uint8_t*>(contents.Data()) + array->ByteOffset();
    length = array->ByteLength();
    *is_shared = buffer->IsSharedArrayBuffer();
  } else {
    thrower->TypeError("Argument 0 must be a buffer source");
  }
  DCHECK_IMPLIES(length, start != nullptr);
  if (length == 0) {
    thrower->CompileError("BufferSource argument is empty");
  }
  if (length > i::wasm::kV8MaxWasmModuleSize) {
    thrower->RangeError("buffer source exceeds maximum size of %zu (is %zu)",
                        i::wasm::kV8MaxWasmModuleSize, length);
  }
  if (thrower->error()) return i::wasm::ModuleWireBytes(nullptr, nullptr);
  return i::wasm::ModuleWireBytes(start, start + length);
}

i::MaybeHandle<i::JSReceiver> GetValueAsImports(Local<Value> arg,
                                                ErrorThrower* thrower) {
  if (arg->IsUndefined()) return {};

  if (!arg->IsObject()) {
    thrower->TypeError("Argument 1 must be an object");
    return {};
  }
  Local<Object> obj = Local<Object>::Cast(arg);
  return i::Handle<i::JSReceiver>::cast(v8::Utils::OpenHandle(*obj));
}

namespace {
// This class resolves the result of WebAssembly.compile. It just places the
// compilation result in the supplied {promise}.
class AsyncCompilationResolver : public i::wasm::CompilationResultResolver {
 public:
  AsyncCompilationResolver(i::Isolate* isolate, i::Handle<i::JSPromise> promise)
      : promise_(isolate->global_handles()->Create(*promise)) {
    i::GlobalHandles::AnnotateStrongRetainer(promise_.location(),
                                             kGlobalPromiseHandle);
  }

  ~AsyncCompilationResolver() override {
    i::GlobalHandles::Destroy(promise_.location());
  }

  void OnCompilationSucceeded(i::Handle<i::WasmModuleObject> result) override {
    if (finished_) return;
    finished_ = true;
    i::MaybeHandle<i::Object> promise_result =
        i::JSPromise::Resolve(promise_, result);
    CHECK_EQ(promise_result.is_null(),
             promise_->GetIsolate()->has_pending_exception());
  }

  void OnCompilationFailed(i::Handle<i::Object> error_reason) override {
    if (finished_) return;
    finished_ = true;
    i::MaybeHandle<i::Object> promise_result =
        i::JSPromise::Reject(promise_, error_reason);
    CHECK_EQ(promise_result.is_null(),
             promise_->GetIsolate()->has_pending_exception());
  }

 private:
  static constexpr char kGlobalPromiseHandle[] =
      "AsyncCompilationResolver::promise_";
  bool finished_ = false;
  i::Handle<i::JSPromise> promise_;
};

constexpr char AsyncCompilationResolver::kGlobalPromiseHandle[];

// This class resolves the result of WebAssembly.instantiate(module, imports).
// It just places the instantiation result in the supplied {promise}.
class InstantiateModuleResultResolver
    : public i::wasm::InstantiationResultResolver {
 public:
  InstantiateModuleResultResolver(i::Isolate* isolate,
                                  i::Handle<i::JSPromise> promise)
      : promise_(isolate->global_handles()->Create(*promise)) {
    i::GlobalHandles::AnnotateStrongRetainer(promise_.location(),
                                             kGlobalPromiseHandle);
  }

  ~InstantiateModuleResultResolver() override {
    i::GlobalHandles::Destroy(promise_.location());
  }

  void OnInstantiationSucceeded(
      i::Handle<i::WasmInstanceObject> instance) override {
    i::MaybeHandle<i::Object> promise_result =
        i::JSPromise::Resolve(promise_, instance);
    CHECK_EQ(promise_result.is_null(),
             promise_->GetIsolate()->has_pending_exception());
  }

  void OnInstantiationFailed(i::Handle<i::Object> error_reason) override {
    i::MaybeHandle<i::Object> promise_result =
        i::JSPromise::Reject(promise_, error_reason);
    CHECK_EQ(promise_result.is_null(),
             promise_->GetIsolate()->has_pending_exception());
  }

 private:
  static constexpr char kGlobalPromiseHandle[] =
      "InstantiateModuleResultResolver::promise_";
  i::Handle<i::JSPromise> promise_;
};

constexpr char InstantiateModuleResultResolver::kGlobalPromiseHandle[];

// This class resolves the result of WebAssembly.instantiate(bytes, imports).
// For that it creates a new {JSObject} which contains both the provided
// {WasmModuleObject} and the resulting {WebAssemblyInstanceObject} itself.
class InstantiateBytesResultResolver
    : public i::wasm::InstantiationResultResolver {
 public:
  InstantiateBytesResultResolver(i::Isolate* isolate,
                                 i::Handle<i::JSPromise> promise,
                                 i::Handle<i::WasmModuleObject> module)
      : isolate_(isolate),
        promise_(isolate_->global_handles()->Create(*promise)),
        module_(isolate_->global_handles()->Create(*module)) {
    i::GlobalHandles::AnnotateStrongRetainer(promise_.location(),
                                             kGlobalPromiseHandle);
    i::GlobalHandles::AnnotateStrongRetainer(module_.location(),
                                             kGlobalModuleHandle);
  }

  ~InstantiateBytesResultResolver() override {
    i::GlobalHandles::Destroy(promise_.location());
    i::GlobalHandles::Destroy(module_.location());
  }

  void OnInstantiationSucceeded(
      i::Handle<i::WasmInstanceObject> instance) override {
    // The result is a JSObject with 2 fields which contain the
    // WasmInstanceObject and the WasmModuleObject.
    i::Handle<i::JSObject> result =
        isolate_->factory()->NewJSObject(isolate_->object_function());

    i::Handle<i::String> instance_name =
        isolate_->factory()
            ->NewStringFromOneByte(i::StaticCharVector("instance"))
            .ToHandleChecked();

    i::Handle<i::String> module_name =
        isolate_->factory()
            ->NewStringFromOneByte(i::StaticCharVector("module"))
            .ToHandleChecked();

    i::JSObject::AddProperty(isolate_, result, instance_name, instance,
                             i::NONE);
    i::JSObject::AddProperty(isolate_, result, module_name, module_, i::NONE);

    i::MaybeHandle<i::Object> promise_result =
        i::JSPromise::Resolve(promise_, result);
    CHECK_EQ(promise_result.is_null(), isolate_->has_pending_exception());
  }

  void OnInstantiationFailed(i::Handle<i::Object> error_reason) override {
    i::MaybeHandle<i::Object> promise_result =
        i::JSPromise::Reject(promise_, error_reason);
    CHECK_EQ(promise_result.is_null(), isolate_->has_pending_exception());
  }

 private:
  static constexpr char kGlobalPromiseHandle[] =
      "InstantiateBytesResultResolver::promise_";
  static constexpr char kGlobalModuleHandle[] =
      "InstantiateBytesResultResolver::module_";
  i::Isolate* isolate_;
  i::Handle<i::JSPromise> promise_;
  i::Handle<i::WasmModuleObject> module_;
};

constexpr char InstantiateBytesResultResolver::kGlobalPromiseHandle[];
constexpr char InstantiateBytesResultResolver::kGlobalModuleHandle[];

// This class is the {CompilationResultResolver} for
// WebAssembly.instantiate(bytes, imports). When compilation finishes,
// {AsyncInstantiate} is started on the compilation result.
class AsyncInstantiateCompileResultResolver
    : public i::wasm::CompilationResultResolver {
 public:
  AsyncInstantiateCompileResultResolver(
      i::Isolate* isolate, i::Handle<i::JSPromise> promise,
      i::MaybeHandle<i::JSReceiver> maybe_imports)
      : isolate_(isolate),
        promise_(isolate_->global_handles()->Create(*promise)),
        maybe_imports_(maybe_imports.is_null()
                           ? maybe_imports
                           : isolate_->global_handles()->Create(
                                 *maybe_imports.ToHandleChecked())) {
    i::GlobalHandles::AnnotateStrongRetainer(promise_.location(),
                                             kGlobalPromiseHandle);
    if (!maybe_imports_.is_null()) {
      i::GlobalHandles::AnnotateStrongRetainer(
          maybe_imports_.ToHandleChecked().location(), kGlobalImportsHandle);
    }
  }

  ~AsyncInstantiateCompileResultResolver() override {
    i::GlobalHandles::Destroy(promise_.location());
    if (!maybe_imports_.is_null()) {
      i::GlobalHandles::Destroy(maybe_imports_.ToHandleChecked().location());
    }
  }

  void OnCompilationSucceeded(i::Handle<i::WasmModuleObject> result) override {
    if (finished_) return;
    finished_ = true;
    isolate_->wasm_engine()->AsyncInstantiate(
        isolate_,
        base::make_unique<InstantiateBytesResultResolver>(isolate_, promise_,
                                                          result),
        result, maybe_imports_);
  }

  void OnCompilationFailed(i::Handle<i::Object> error_reason) override {
    if (finished_) return;
    finished_ = true;
    i::MaybeHandle<i::Object> promise_result =
        i::JSPromise::Reject(promise_, error_reason);
    CHECK_EQ(promise_result.is_null(), isolate_->has_pending_exception());
  }

 private:
  static constexpr char kGlobalPromiseHandle[] =
      "AsyncInstantiateCompileResultResolver::promise_";
  static constexpr char kGlobalImportsHandle[] =
      "AsyncInstantiateCompileResultResolver::module_";
  bool finished_ = false;
  i::Isolate* isolate_;
  i::Handle<i::JSPromise> promise_;
  i::MaybeHandle<i::JSReceiver> maybe_imports_;
};

constexpr char AsyncInstantiateCompileResultResolver::kGlobalPromiseHandle[];
constexpr char AsyncInstantiateCompileResultResolver::kGlobalImportsHandle[];

std::string ToString(const char* name) { return std::string(name); }

std::string ToString(const i::Handle<i::String> name) {
  return std::string("Property '") + name->ToCString().get() + "'";
}

// Web IDL: '[EnforceRange] unsigned long'
// Previously called ToNonWrappingUint32 in the draft WebAssembly JS spec.
// https://heycam.github.io/webidl/#EnforceRange
template <typename T>
bool EnforceUint32(T argument_name, Local<v8::Value> v, Local<Context> context,
                   ErrorThrower* thrower, uint32_t* res) {
  double double_number;

  if (!v->NumberValue(context).To(&double_number)) {
    thrower->TypeError("%s must be convertible to a number",
                       ToString(argument_name).c_str());
    return false;
  }
  if (!std::isfinite(double_number)) {
    thrower->TypeError("%s must be convertible to a valid number",
                       ToString(argument_name).c_str());
    return false;
  }
  if (double_number < 0) {
    thrower->TypeError("%s must be non-negative",
                       ToString(argument_name).c_str());
    return false;
  }
  if (double_number > std::numeric_limits<uint32_t>::max()) {
    thrower->TypeError("%s must be in the unsigned long range",
                       ToString(argument_name).c_str());
    return false;
  }

  *res = static_cast<uint32_t>(double_number);
  return true;
}
}  // namespace

// WebAssembly.compile(bytes) -> Promise
void WebAssemblyCompile(const v8::FunctionCallbackInfo<v8::Value>& args) {
  constexpr const char* kAPIMethodName = "WebAssembly.compile()";
  v8::Isolate* isolate = args.GetIsolate();
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);

  HandleScope scope(isolate);
  ScheduledErrorThrower thrower(i_isolate, kAPIMethodName);

  if (!i::wasm::IsWasmCodegenAllowed(i_isolate, i_isolate->native_context())) {
    thrower.CompileError("Wasm code generation disallowed by embedder");
  }

  Local<Context> context = isolate->GetCurrentContext();
  ASSIGN(Promise::Resolver, promise_resolver, Promise::Resolver::New(context));
  Local<Promise> promise = promise_resolver->GetPromise();
  v8::ReturnValue<v8::Value> return_value = args.GetReturnValue();
  return_value.Set(promise);

  std::shared_ptr<i::wasm::CompilationResultResolver> resolver(
      new AsyncCompilationResolver(i_isolate, Utils::OpenHandle(*promise)));

  bool is_shared = false;
  auto bytes = GetFirstArgumentAsBytes(args, &thrower, &is_shared);
  if (thrower.error()) {
    resolver->OnCompilationFailed(thrower.Reify());
    return;
  }
  // Asynchronous compilation handles copying wire bytes if necessary.
  auto enabled_features = i::wasm::WasmFeaturesFromIsolate(i_isolate);
  i_isolate->wasm_engine()->AsyncCompile(i_isolate, enabled_features,
                                         std::move(resolver), bytes, is_shared,
                                         kAPIMethodName);
}

void WasmStreamingCallbackForTesting(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);

  HandleScope scope(isolate);
  ScheduledErrorThrower thrower(i_isolate, "WebAssembly.compile()");

  std::shared_ptr<v8::WasmStreaming> streaming =
      v8::WasmStreaming::Unpack(args.GetIsolate(), args.Data());

  bool is_shared = false;
  i::wasm::ModuleWireBytes bytes =
      GetFirstArgumentAsBytes(args, &thrower, &is_shared);
  if (thrower.error()) {
    streaming->Abort(Utils::ToLocal(thrower.Reify()));
    return;
  }
  streaming->OnBytesReceived(bytes.start(), bytes.length());
  streaming->Finish();
  CHECK(!thrower.error());
}

void WasmStreamingPromiseFailedCallback(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  std::shared_ptr<v8::WasmStreaming> streaming =
      v8::WasmStreaming::Unpack(args.GetIsolate(), args.Data());
  streaming->Abort(args[0]);
}

// WebAssembly.compileStreaming(Promise<Response>) -> Promise
void WebAssemblyCompileStreaming(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  HandleScope scope(isolate);
  const char* const kAPIMethodName = "WebAssembly.compileStreaming()";
  ScheduledErrorThrower thrower(i_isolate, kAPIMethodName);
  Local<Context> context = isolate->GetCurrentContext();

  // Create and assign the return value of this function.
  ASSIGN(Promise::Resolver, result_resolver, Promise::Resolver::New(context));
  Local<Promise> promise = result_resolver->GetPromise();
  v8::ReturnValue<v8::Value> return_value = args.GetReturnValue();
  return_value.Set(promise);

  // Prepare the CompilationResultResolver for the compilation.
  auto resolver = std::make_shared<AsyncCompilationResolver>(
      i_isolate, Utils::OpenHandle(*promise));

  if (!i::wasm::IsWasmCodegenAllowed(i_isolate, i_isolate->native_context())) {
    thrower.CompileError("Wasm code generation disallowed by embedder");
    resolver->OnCompilationFailed(thrower.Reify());
    return;
  }

  // Allocate the streaming decoder in a Managed so we can pass it to the
  // embedder.
  i::Handle<i::Managed<WasmStreaming>> data =
      i::Managed<WasmStreaming>::Allocate(
          i_isolate, 0,
          base::make_unique<WasmStreaming::WasmStreamingImpl>(
              isolate, kAPIMethodName, resolver));

  DCHECK_NOT_NULL(i_isolate->wasm_streaming_callback());
  ASSIGN(
      v8::Function, compile_callback,
      v8::Function::New(context, i_isolate->wasm_streaming_callback(),
                        Utils::ToLocal(i::Handle<i::Object>::cast(data)), 1));
  ASSIGN(
      v8::Function, reject_callback,
      v8::Function::New(context, WasmStreamingPromiseFailedCallback,
                        Utils::ToLocal(i::Handle<i::Object>::cast(data)), 1));

  // The parameter may be of type {Response} or of type {Promise<Response>}.
  // Treat either case of parameter as Promise.resolve(parameter)
  // as per https://www.w3.org/2001/tag/doc/promises-guide#resolve-arguments

  // Ending with:
  //    return Promise.resolve(parameter).then(compile_callback);
  ASSIGN(Promise::Resolver, input_resolver, Promise::Resolver::New(context));
  if (!input_resolver->Resolve(context, args[0]).IsJust()) return;

  // We do not have any use of the result here. The {compile_callback} will
  // start streaming compilation, which will eventually resolve the promise we
  // set as result value.
  USE(input_resolver->GetPromise()->Then(context, compile_callback,
                                         reject_callback));
}

// WebAssembly.validate(bytes) -> bool
void WebAssemblyValidate(const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  HandleScope scope(isolate);
  ScheduledErrorThrower thrower(i_isolate, "WebAssembly.validate()");

  bool is_shared = false;
  auto bytes = GetFirstArgumentAsBytes(args, &thrower, &is_shared);

  v8::ReturnValue<v8::Value> return_value = args.GetReturnValue();

  if (thrower.error()) {
    if (thrower.wasm_error()) thrower.Reset();  // Clear error.
    return_value.Set(v8::False(isolate));
    return;
  }

  auto enabled_features = i::wasm::WasmFeaturesFromIsolate(i_isolate);
  bool validated = false;
  if (is_shared) {
    // Make a copy of the wire bytes to avoid concurrent modification.
    std::unique_ptr<uint8_t[]> copy(new uint8_t[bytes.length()]);
    memcpy(copy.get(), bytes.start(), bytes.length());
    i::wasm::ModuleWireBytes bytes_copy(copy.get(),
                                        copy.get() + bytes.length());
    validated = i_isolate->wasm_engine()->SyncValidate(
        i_isolate, enabled_features, bytes_copy);
  } else {
    // The wire bytes are not shared, OK to use them directly.
    validated = i_isolate->wasm_engine()->SyncValidate(i_isolate,
                                                       enabled_features, bytes);
  }

  return_value.Set(Boolean::New(isolate, validated));
}

// new WebAssembly.Module(bytes) -> WebAssembly.Module
void WebAssemblyModule(const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  if (i_isolate->wasm_module_callback()(args)) return;

  HandleScope scope(isolate);
  ScheduledErrorThrower thrower(i_isolate, "WebAssembly.Module()");

  if (!args.IsConstructCall()) {
    thrower.TypeError("WebAssembly.Module must be invoked with 'new'");
    return;
  }
  if (!i::wasm::IsWasmCodegenAllowed(i_isolate, i_isolate->native_context())) {
    thrower.CompileError("Wasm code generation disallowed by embedder");
    return;
  }

  bool is_shared = false;
  auto bytes = GetFirstArgumentAsBytes(args, &thrower, &is_shared);

  if (thrower.error()) {
    return;
  }
  auto enabled_features = i::wasm::WasmFeaturesFromIsolate(i_isolate);
  i::MaybeHandle<i::Object> module_obj;
  if (is_shared) {
    // Make a copy of the wire bytes to avoid concurrent modification.
    std::unique_ptr<uint8_t[]> copy(new uint8_t[bytes.length()]);
    memcpy(copy.get(), bytes.start(), bytes.length());
    i::wasm::ModuleWireBytes bytes_copy(copy.get(),
                                        copy.get() + bytes.length());
    module_obj = i_isolate->wasm_engine()->SyncCompile(
        i_isolate, enabled_features, &thrower, bytes_copy);
  } else {
    // The wire bytes are not shared, OK to use them directly.
    module_obj = i_isolate->wasm_engine()->SyncCompile(
        i_isolate, enabled_features, &thrower, bytes);
  }

  if (module_obj.is_null()) return;

  v8::ReturnValue<v8::Value> return_value = args.GetReturnValue();
  return_value.Set(Utils::ToLocal(module_obj.ToHandleChecked()));
}

// WebAssembly.Module.imports(module) -> Array<Import>
void WebAssemblyModuleImports(const v8::FunctionCallbackInfo<v8::Value>& args) {
  HandleScope scope(args.GetIsolate());
  v8::Isolate* isolate = args.GetIsolate();
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  ScheduledErrorThrower thrower(i_isolate, "WebAssembly.Module.imports()");

  auto maybe_module = GetFirstArgumentAsModule(args, &thrower);
  if (thrower.error()) return;
  auto imports = i::wasm::GetImports(i_isolate, maybe_module.ToHandleChecked());
  args.GetReturnValue().Set(Utils::ToLocal(imports));
}

// WebAssembly.Module.exports(module) -> Array<Export>
void WebAssemblyModuleExports(const v8::FunctionCallbackInfo<v8::Value>& args) {
  HandleScope scope(args.GetIsolate());
  v8::Isolate* isolate = args.GetIsolate();
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  ScheduledErrorThrower thrower(i_isolate, "WebAssembly.Module.exports()");

  auto maybe_module = GetFirstArgumentAsModule(args, &thrower);
  if (thrower.error()) return;
  auto exports = i::wasm::GetExports(i_isolate, maybe_module.ToHandleChecked());
  args.GetReturnValue().Set(Utils::ToLocal(exports));
}

// WebAssembly.Module.customSections(module, name) -> Array<Section>
void WebAssemblyModuleCustomSections(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  HandleScope scope(args.GetIsolate());
  v8::Isolate* isolate = args.GetIsolate();
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  ScheduledErrorThrower thrower(i_isolate,
                                "WebAssembly.Module.customSections()");

  auto maybe_module = GetFirstArgumentAsModule(args, &thrower);
  if (thrower.error()) return;

  if (args[1]->IsUndefined()) {
    thrower.TypeError("Argument 1 is required");
    return;
  }

  i::MaybeHandle<i::Object> maybe_name =
      i::Object::ToString(i_isolate, Utils::OpenHandle(*args[1]));
  i::Handle<i::Object> name;
  if (!maybe_name.ToHandle(&name)) return;
  auto custom_sections =
      i::wasm::GetCustomSections(i_isolate, maybe_module.ToHandleChecked(),
                                 i::Handle<i::String>::cast(name), &thrower);
  if (thrower.error()) return;
  args.GetReturnValue().Set(Utils::ToLocal(custom_sections));
}

MaybeLocal<Value> WebAssemblyInstantiateImpl(Isolate* isolate,
                                             Local<Value> module,
                                             Local<Value> ffi) {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);

  i::MaybeHandle<i::Object> instance_object;
  {
    ScheduledErrorThrower thrower(i_isolate, "WebAssembly.Instance()");

    // TODO(ahaas): These checks on the module should not be necessary here They
    // are just a workaround for https://crbug.com/837417.
    i::Handle<i::Object> module_obj = Utils::OpenHandle(*module);
    if (!module_obj->IsWasmModuleObject()) {
      thrower.TypeError("Argument 0 must be a WebAssembly.Module object");
      return {};
    }

    i::MaybeHandle<i::JSReceiver> maybe_imports =
        GetValueAsImports(ffi, &thrower);
    if (thrower.error()) return {};

    instance_object = i_isolate->wasm_engine()->SyncInstantiate(
        i_isolate, &thrower, i::Handle<i::WasmModuleObject>::cast(module_obj),
        maybe_imports, i::MaybeHandle<i::JSArrayBuffer>());
  }

  DCHECK_EQ(instance_object.is_null(), i_isolate->has_scheduled_exception());
  if (instance_object.is_null()) return {};
  return Utils::ToLocal(instance_object.ToHandleChecked());
}

// new WebAssembly.Instance(module, imports) -> WebAssembly.Instance
void WebAssemblyInstance(const v8::FunctionCallbackInfo<v8::Value>& args) {
  Isolate* isolate = args.GetIsolate();
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  i_isolate->CountUsage(
      v8::Isolate::UseCounterFeature::kWebAssemblyInstantiation);

  HandleScope scope(args.GetIsolate());
  if (i_isolate->wasm_instance_callback()(args)) return;

  ScheduledErrorThrower thrower(i_isolate, "WebAssembly.Instance()");
  if (!args.IsConstructCall()) {
    thrower.TypeError("WebAssembly.Instance must be invoked with 'new'");
    return;
  }

  GetFirstArgumentAsModule(args, &thrower);
  if (thrower.error()) return;

  // If args.Length < 2, this will be undefined - see FunctionCallbackInfo.
  // We'll check for that in WebAssemblyInstantiateImpl.
  Local<Value> data = args[1];

  Local<Value> instance;
  if (WebAssemblyInstantiateImpl(isolate, args[0], data).ToLocal(&instance)) {
    args.GetReturnValue().Set(instance);
  }
}

void WebAssemblyInstantiateStreaming(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  i_isolate->CountUsage(
      v8::Isolate::UseCounterFeature::kWebAssemblyInstantiation);

  HandleScope scope(isolate);
  Local<Context> context = isolate->GetCurrentContext();
  const char* const kAPIMethodName = "WebAssembly.instantiateStreaming()";
  ScheduledErrorThrower thrower(i_isolate, kAPIMethodName);

  // Create and assign the return value of this function.
  ASSIGN(Promise::Resolver, result_resolver, Promise::Resolver::New(context));
  Local<Promise> promise = result_resolver->GetPromise();
  v8::ReturnValue<v8::Value> return_value = args.GetReturnValue();
  return_value.Set(promise);

  // Create an InstantiateResultResolver in case there is an issue with the
  // passed parameters.
  std::unique_ptr<i::wasm::InstantiationResultResolver> resolver(
      new InstantiateModuleResultResolver(i_isolate,
                                          Utils::OpenHandle(*promise)));

  if (!i::wasm::IsWasmCodegenAllowed(i_isolate, i_isolate->native_context())) {
    thrower.CompileError("Wasm code generation disallowed by embedder");
    resolver->OnInstantiationFailed(thrower.Reify());
    return;
  }

  // If args.Length < 2, this will be undefined - see FunctionCallbackInfo.
  Local<Value> ffi = args[1];
  i::MaybeHandle<i::JSReceiver> maybe_imports =
      GetValueAsImports(ffi, &thrower);

  if (thrower.error()) {
    resolver->OnInstantiationFailed(thrower.Reify());
    return;
  }

  // We start compilation now, we have no use for the
  // {InstantiationResultResolver}.
  resolver.reset();

  std::shared_ptr<i::wasm::CompilationResultResolver> compilation_resolver(
      new AsyncInstantiateCompileResultResolver(
          i_isolate, Utils::OpenHandle(*promise), maybe_imports));

  // Allocate the streaming decoder in a Managed so we can pass it to the
  // embedder.
  i::Handle<i::Managed<WasmStreaming>> data =
      i::Managed<WasmStreaming>::Allocate(
          i_isolate, 0,
          base::make_unique<WasmStreaming::WasmStreamingImpl>(
              isolate, kAPIMethodName, compilation_resolver));

  DCHECK_NOT_NULL(i_isolate->wasm_streaming_callback());
  ASSIGN(
      v8::Function, compile_callback,
      v8::Function::New(context, i_isolate->wasm_streaming_callback(),
                        Utils::ToLocal(i::Handle<i::Object>::cast(data)), 1));
  ASSIGN(
      v8::Function, reject_callback,
      v8::Function::New(context, WasmStreamingPromiseFailedCallback,
                        Utils::ToLocal(i::Handle<i::Object>::cast(data)), 1));

  // The parameter may be of type {Response} or of type {Promise<Response>}.
  // Treat either case of parameter as Promise.resolve(parameter)
  // as per https://www.w3.org/2001/tag/doc/promises-guide#resolve-arguments

  // Ending with:
  //    return Promise.resolve(parameter).then(compile_callback);
  ASSIGN(Promise::Resolver, input_resolver, Promise::Resolver::New(context));
  if (!input_resolver->Resolve(context, args[0]).IsJust()) return;

  // We do not have any use of the result here. The {compile_callback} will
  // start streaming compilation, which will eventually resolve the promise we
  // set as result value.
  USE(input_resolver->GetPromise()->Then(context, compile_callback,
                                         reject_callback));
}

// WebAssembly.instantiate(module, imports) -> WebAssembly.Instance
// WebAssembly.instantiate(bytes, imports) ->
//     {module: WebAssembly.Module, instance: WebAssembly.Instance}
void WebAssemblyInstantiate(const v8::FunctionCallbackInfo<v8::Value>& args) {
  constexpr const char* kAPIMethodName = "WebAssembly.instantiate()";
  v8::Isolate* isolate = args.GetIsolate();
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  i_isolate->CountUsage(
      v8::Isolate::UseCounterFeature::kWebAssemblyInstantiation);

  ScheduledErrorThrower thrower(i_isolate, kAPIMethodName);

  HandleScope scope(isolate);

  Local<Context> context = isolate->GetCurrentContext();

  ASSIGN(Promise::Resolver, promise_resolver, Promise::Resolver::New(context));
  Local<Promise> promise = promise_resolver->GetPromise();
  args.GetReturnValue().Set(promise);

  std::unique_ptr<i::wasm::InstantiationResultResolver> resolver(
      new InstantiateModuleResultResolver(i_isolate,
                                          Utils::OpenHandle(*promise)));

  Local<Value> first_arg_value = args[0];
  i::Handle<i::Object> first_arg = Utils::OpenHandle(*first_arg_value);
  if (!first_arg->IsJSObject()) {
    thrower.TypeError(
        "Argument 0 must be a buffer source or a WebAssembly.Module object");
    resolver->OnInstantiationFailed(thrower.Reify());
    return;
  }

  // If args.Length < 2, this will be undefined - see FunctionCallbackInfo.
  Local<Value> ffi = args[1];
  i::MaybeHandle<i::JSReceiver> maybe_imports =
      GetValueAsImports(ffi, &thrower);

  if (thrower.error()) {
    resolver->OnInstantiationFailed(thrower.Reify());
    return;
  }

  if (first_arg->IsWasmModuleObject()) {
    i::Handle<i::WasmModuleObject> module_obj =
        i::Handle<i::WasmModuleObject>::cast(first_arg);

    i_isolate->wasm_engine()->AsyncInstantiate(i_isolate, std::move(resolver),
                                               module_obj, maybe_imports);
    return;
  }

  bool is_shared = false;
  auto bytes = GetFirstArgumentAsBytes(args, &thrower, &is_shared);
  if (thrower.error()) {
    resolver->OnInstantiationFailed(thrower.Reify());
    return;
  }

  // We start compilation now, we have no use for the
  // {InstantiationResultResolver}.
  resolver.reset();

  std::shared_ptr<i::wasm::CompilationResultResolver> compilation_resolver(
      new AsyncInstantiateCompileResultResolver(
          i_isolate, Utils::OpenHandle(*promise), maybe_imports));

  // The first parameter is a buffer source, we have to check if we are allowed
  // to compile it.
  if (!i::wasm::IsWasmCodegenAllowed(i_isolate, i_isolate->native_context())) {
    thrower.CompileError("Wasm code generation disallowed by embedder");
    compilation_resolver->OnCompilationFailed(thrower.Reify());
    return;
  }

  // Asynchronous compilation handles copying wire bytes if necessary.
  auto enabled_features = i::wasm::WasmFeaturesFromIsolate(i_isolate);
  i_isolate->wasm_engine()->AsyncCompile(i_isolate, enabled_features,
                                         std::move(compilation_resolver), bytes,
                                         is_shared, kAPIMethodName);
}

bool GetIntegerProperty(v8::Isolate* isolate, ErrorThrower* thrower,
                        Local<Context> context, v8::Local<v8::Value> value,
                        i::Handle<i::String> property_name, int64_t* result,
                        int64_t lower_bound, uint64_t upper_bound) {
  uint32_t number;
  if (!EnforceUint32(property_name, value, context, thrower, &number)) {
    return false;
  }
  if (number < lower_bound) {
    thrower->RangeError("Property '%s': value %" PRIu32
                        " is below the lower bound %" PRIx64,
                        property_name->ToCString().get(), number, lower_bound);
    return false;
  }
  if (number > upper_bound) {
    thrower->RangeError("Property '%s': value %" PRIu32
                        " is above the upper bound %" PRIu64,
                        property_name->ToCString().get(), number, upper_bound);
    return false;
  }

  *result = static_cast<int64_t>(number);
  return true;
}

bool GetOptionalIntegerProperty(v8::Isolate* isolate, ErrorThrower* thrower,
                                Local<Context> context,
                                Local<v8::Object> object,
                                Local<String> property, bool* has_property,
                                int64_t* result, int64_t lower_bound,
                                uint64_t upper_bound) {
  v8::Local<v8::Value> value;
  if (!object->Get(context, property).ToLocal(&value)) {
    return false;
  }

  // Web IDL: dictionary presence
  // https://heycam.github.io/webidl/#dfn-present
  if (value->IsUndefined()) {
    if (has_property != nullptr) *has_property = false;
    return true;
  }

  if (has_property != nullptr) *has_property = true;
  i::Handle<i::String> property_name = v8::Utils::OpenHandle(*property);

  return GetIntegerProperty(isolate, thrower, context, value, property_name,
                            result, lower_bound, upper_bound);
}

// Fetch 'initial' or 'minimum' property from object. If both are provided,
// 'initial' is used.
// TODO(aseemgarg): change behavior when the following bug is resolved:
// https://github.com/WebAssembly/js-types/issues/6
bool GetInitialOrMinimumProperty(v8::Isolate* isolate, ErrorThrower* thrower,
                                 Local<Context> context,
                                 Local<v8::Object> object, int64_t* result,
                                 int64_t lower_bound, uint64_t upper_bound) {
  bool has_initial = false;
  if (!GetOptionalIntegerProperty(isolate, thrower, context, object,
                                  v8_str(isolate, "initial"), &has_initial,
                                  result, lower_bound, upper_bound)) {
    return false;
  }
  auto enabled_features = i::wasm::WasmFeaturesFromFlags();
  if (!has_initial && enabled_features.type_reflection) {
    if (!GetOptionalIntegerProperty(isolate, thrower, context, object,
                                    v8_str(isolate, "minimum"), &has_initial,
                                    result, lower_bound, upper_bound)) {
      return false;
    }
  }
  if (!has_initial) {
    // TODO(aseemgarg): update error message when the spec issue is resolved.
    thrower->TypeError("Property 'initial' is required");
    return false;
  }
  return true;
}

// new WebAssembly.Table(args) -> WebAssembly.Table
void WebAssemblyTable(const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  HandleScope scope(isolate);
  ScheduledErrorThrower thrower(i_isolate, "WebAssembly.Module()");
  if (!args.IsConstructCall()) {
    thrower.TypeError("WebAssembly.Table must be invoked with 'new'");
    return;
  }
  if (!args[0]->IsObject()) {
    thrower.TypeError("Argument 0 must be a table descriptor");
    return;
  }
  Local<Context> context = isolate->GetCurrentContext();
  Local<v8::Object> descriptor = Local<Object>::Cast(args[0]);
  i::wasm::ValueType type;
  // The descriptor's 'element'.
  {
    v8::MaybeLocal<v8::Value> maybe =
        descriptor->Get(context, v8_str(isolate, "element"));
    v8::Local<v8::Value> value;
    if (!maybe.ToLocal(&value)) return;
    v8::Local<v8::String> string;
    if (!value->ToString(context).ToLocal(&string)) return;
    auto enabled_features = i::wasm::WasmFeaturesFromFlags();
    if (string->StringEquals(v8_str(isolate, "anyfunc"))) {
      type = i::wasm::kWasmFuncRef;
    } else if (enabled_features.anyref &&
               string->StringEquals(v8_str(isolate, "anyref"))) {
      type = i::wasm::kWasmAnyRef;
    } else {
      thrower.TypeError("Descriptor property 'element' must be 'anyfunc'");
      return;
    }
  }

  int64_t initial = 0;
  if (!GetInitialOrMinimumProperty(isolate, &thrower, context, descriptor,
                                   &initial, 0,
                                   i::wasm::max_table_init_entries())) {
    return;
  }
  // The descriptor's 'maximum'.
  int64_t maximum = -1;
  bool has_maximum = true;
  if (!GetOptionalIntegerProperty(
          isolate, &thrower, context, descriptor, v8_str(isolate, "maximum"),
          &has_maximum, &maximum, initial, i::wasm::max_table_init_entries())) {
    return;
  }

  i::Handle<i::FixedArray> fixed_array;
  i::Handle<i::JSObject> table_obj = i::WasmTableObject::New(
      i_isolate, type, static_cast<uint32_t>(initial), has_maximum,
      static_cast<uint32_t>(maximum), &fixed_array);
  v8::ReturnValue<v8::Value> return_value = args.GetReturnValue();
  return_value.Set(Utils::ToLocal(table_obj));
}

void WebAssemblyMemory(const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  HandleScope scope(isolate);
  ScheduledErrorThrower thrower(i_isolate, "WebAssembly.Memory()");
  if (!args.IsConstructCall()) {
    thrower.TypeError("WebAssembly.Memory must be invoked with 'new'");
    return;
  }
  if (!args[0]->IsObject()) {
    thrower.TypeError("Argument 0 must be a memory descriptor");
    return;
  }
  Local<Context> context = isolate->GetCurrentContext();
  Local<v8::Object> descriptor = Local<Object>::Cast(args[0]);

  int64_t initial = 0;
  if (!GetInitialOrMinimumProperty(isolate, &thrower, context, descriptor,
                                   &initial, 0, i::wasm::max_mem_pages())) {
    return;
  }
  // The descriptor's 'maximum'.
  int64_t maximum = -1;
  if (!GetOptionalIntegerProperty(isolate, &thrower, context, descriptor,
                                  v8_str(isolate, "maximum"), nullptr, &maximum,
                                  initial, i::wasm::kSpecMaxWasmMemoryPages)) {
    return;
  }

  bool is_shared_memory = false;
  auto enabled_features = i::wasm::WasmFeaturesFromIsolate(i_isolate);
  if (enabled_features.threads) {
    // Shared property of descriptor
    Local<String> shared_key = v8_str(isolate, "shared");
    v8::MaybeLocal<v8::Value> maybe_value =
        descriptor->Get(context, shared_key);
    v8::Local<v8::Value> value;
    if (maybe_value.ToLocal(&value)) {
      is_shared_memory = value->BooleanValue(isolate);
    }
    // Throw TypeError if shared is true, and the descriptor has no "maximum"
    if (is_shared_memory && maximum == -1) {
      thrower.TypeError(
          "If shared is true, maximum property should be defined.");
      return;
    }
  }

  i::Handle<i::JSObject> memory_obj;
  if (!i::WasmMemoryObject::New(i_isolate, static_cast<uint32_t>(initial),
                                static_cast<uint32_t>(maximum),
                                is_shared_memory)
           .ToHandle(&memory_obj)) {
    thrower.RangeError("could not allocate memory");
    return;
  }
  if (is_shared_memory) {
    i::Handle<i::JSArrayBuffer> buffer(
        i::Handle<i::WasmMemoryObject>::cast(memory_obj)->array_buffer(),
        i_isolate);
    Maybe<bool> result =
        buffer->SetIntegrityLevel(buffer, i::FROZEN, i::kDontThrow);
    if (!result.FromJust()) {
      thrower.TypeError(
          "Status of setting SetIntegrityLevel of buffer is false.");
      return;
    }
  }
  args.GetReturnValue().Set(Utils::ToLocal(memory_obj));
}

// Determines the type encoded in a value type property (e.g. type reflection).
// Returns false if there was an exception, true upon success. On success the
// outgoing {type} is set accordingly, or set to {wasm::kWasmStmt} in case the
// type could not be properly recognized.
bool GetValueType(Isolate* isolate, MaybeLocal<Value> maybe,
                  Local<Context> context, i::wasm::ValueType* type,
                  i::wasm::WasmFeatures enabled_features) {
  v8::Local<v8::Value> value;
  if (!maybe.ToLocal(&value)) return false;
  v8::Local<v8::String> string;
  if (!value->ToString(context).ToLocal(&string)) return false;
  if (string->StringEquals(v8_str(isolate, "i32"))) {
    *type = i::wasm::kWasmI32;
  } else if (string->StringEquals(v8_str(isolate, "f32"))) {
    *type = i::wasm::kWasmF32;
  } else if (string->StringEquals(v8_str(isolate, "i64"))) {
    *type = i::wasm::kWasmI64;
  } else if (string->StringEquals(v8_str(isolate, "f64"))) {
    *type = i::wasm::kWasmF64;
  } else if (enabled_features.anyref &&
             string->StringEquals(v8_str(isolate, "anyref"))) {
    *type = i::wasm::kWasmAnyRef;
  } else if (enabled_features.anyref &&
             string->StringEquals(v8_str(isolate, "anyfunc"))) {
    *type = i::wasm::kWasmFuncRef;
  } else {
    // Unrecognized type.
    *type = i::wasm::kWasmStmt;
  }
  return true;
}

// WebAssembly.Global
void WebAssemblyGlobal(const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  HandleScope scope(isolate);
  ScheduledErrorThrower thrower(i_isolate, "WebAssembly.Global()");
  if (!args.IsConstructCall()) {
    thrower.TypeError("WebAssembly.Global must be invoked with 'new'");
    return;
  }
  if (!args[0]->IsObject()) {
    thrower.TypeError("Argument 0 must be a global descriptor");
    return;
  }
  Local<Context> context = isolate->GetCurrentContext();
  Local<v8::Object> descriptor = Local<Object>::Cast(args[0]);
  auto enabled_features = i::wasm::WasmFeaturesFromIsolate(i_isolate);

  // The descriptor's 'mutable'.
  bool is_mutable = false;
  {
    Local<String> mutable_key = v8_str(isolate, "mutable");
    v8::MaybeLocal<v8::Value> maybe = descriptor->Get(context, mutable_key);
    v8::Local<v8::Value> value;
    if (maybe.ToLocal(&value)) {
      is_mutable = value->BooleanValue(isolate);
    }
  }

  // The descriptor's type, called 'value'. It is called 'value' because this
  // descriptor is planned to be re-used as the global's type for reflection,
  // so calling it 'type' is redundant.
  i::wasm::ValueType type;
  {
    v8::MaybeLocal<v8::Value> maybe =
        descriptor->Get(context, v8_str(isolate, "value"));
    if (!GetValueType(isolate, maybe, context, &type, enabled_features)) return;
    if (type == i::wasm::kWasmStmt) {
      thrower.TypeError(
          "Descriptor property 'value' must be 'i32', 'i64', 'f32', or "
          "'f64'");
      return;
    }
  }

  const uint32_t offset = 0;
  i::MaybeHandle<i::WasmGlobalObject> maybe_global_obj =
      i::WasmGlobalObject::New(i_isolate, i::MaybeHandle<i::JSArrayBuffer>(),
                               i::MaybeHandle<i::FixedArray>(), type, offset,
                               is_mutable);

  i::Handle<i::WasmGlobalObject> global_obj;
  if (!maybe_global_obj.ToHandle(&global_obj)) {
    thrower.RangeError("could not allocate memory");
    return;
  }

  // Convert value to a WebAssembly value, the default value is 0.
  Local<v8::Value> value = Local<Value>::Cast(args[1]);
  switch (type) {
    case i::wasm::kWasmI32: {
      int32_t i32_value = 0;
      if (!value->IsUndefined()) {
        v8::Local<v8::Int32> int32_value;
        if (!value->ToInt32(context).ToLocal(&int32_value)) return;
        if (!int32_value->Int32Value(context).To(&i32_value)) return;
      }
      global_obj->SetI32(i32_value);
      break;
    }
    case i::wasm::kWasmI64: {
      int64_t i64_value = 0;
      if (!value->IsUndefined()) {
        if (!enabled_features.bigint) {
          thrower.TypeError("Can't set the value of i64 WebAssembly.Global");
          return;
        }

        v8::Local<v8::BigInt> bigint_value;
        if (!value->ToBigInt(context).ToLocal(&bigint_value)) return;
        i64_value = bigint_value->Int64Value();
      }
      global_obj->SetI64(i64_value);
      break;
    }
    case i::wasm::kWasmF32: {
      float f32_value = 0;
      if (!value->IsUndefined()) {
        double f64_value = 0;
        v8::Local<v8::Number> number_value;
        if (!value->ToNumber(context).ToLocal(&number_value)) return;
        if (!number_value->NumberValue(context).To(&f64_value)) return;
        f32_value = i::DoubleToFloat32(f64_value);
      }
      global_obj->SetF32(f32_value);
      break;
    }
    case i::wasm::kWasmF64: {
      double f64_value = 0;
      if (!value->IsUndefined()) {
        v8::Local<v8::Number> number_value;
        if (!value->ToNumber(context).ToLocal(&number_value)) return;
        if (!number_value->NumberValue(context).To(&f64_value)) return;
      }
      global_obj->SetF64(f64_value);
      break;
    }
    case i::wasm::kWasmAnyRef: {
      if (args.Length() < 2) {
        // When no inital value is provided, we have to use the WebAssembly
        // default value 'null', and not the JS default value 'undefined'.
        global_obj->SetAnyRef(i_isolate->factory()->null_value());
        break;
      }
      global_obj->SetAnyRef(Utils::OpenHandle(*value));
      break;
    }
    case i::wasm::kWasmFuncRef: {
      if (args.Length() < 2) {
        // When no inital value is provided, we have to use the WebAssembly
        // default value 'null', and not the JS default value 'undefined'.
        global_obj->SetFuncRef(i_isolate, i_isolate->factory()->null_value());
        break;
      }

      if (!global_obj->SetFuncRef(i_isolate, Utils::OpenHandle(*value))) {
        thrower.TypeError(
            "The value of anyfunc globals must be null or an "
            "exported function");
      }
      break;
    }
    default:
      UNREACHABLE();
  }

  i::Handle<i::JSObject> global_js_object(global_obj);
  args.GetReturnValue().Set(Utils::ToLocal(global_js_object));
}

// WebAssembly.Exception
void WebAssemblyException(const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  HandleScope scope(isolate);
  ScheduledErrorThrower thrower(i_isolate, "WebAssembly.Exception()");
  thrower.TypeError("WebAssembly.Exception cannot be called");
}

// WebAssembly.Function
void WebAssemblyFunction(const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  HandleScope scope(isolate);
  ScheduledErrorThrower thrower(i_isolate, "WebAssembly.Function()");
  if (!args.IsConstructCall()) {
    thrower.TypeError("WebAssembly.Function must be invoked with 'new'");
    return;
  }
  if (!args[0]->IsObject()) {
    thrower.TypeError("Argument 0 must be a function type");
    return;
  }
  Local<Object> function_type = Local<Object>::Cast(args[0]);
  Local<Context> context = isolate->GetCurrentContext();
  auto enabled_features = i::wasm::WasmFeaturesFromIsolate(i_isolate);

  // Load the 'parameters' property of the function type.
  Local<String> parameters_key = v8_str(isolate, "parameters");
  v8::MaybeLocal<v8::Value> parameters_maybe =
      function_type->Get(context, parameters_key);
  v8::Local<v8::Value> parameters_value;
  if (!parameters_maybe.ToLocal(&parameters_value)) return;
  // TODO(7742): Allow any iterable, not just {Array} here.
  if (!parameters_value->IsArray()) {
    thrower.TypeError("Argument 0 must be a function type with 'parameters'");
    return;
  }
  Local<Array> parameters = parameters_value.As<Array>();
  uint32_t parameters_len = parameters->Length();
  if (parameters_len > i::wasm::kV8MaxWasmFunctionParams) {
    thrower.TypeError("Argument 0 contains too many parameters");
    return;
  }

  // Load the 'results' property of the function type.
  Local<String> results_key = v8_str(isolate, "results");
  v8::MaybeLocal<v8::Value> results_maybe =
      function_type->Get(context, results_key);
  v8::Local<v8::Value> results_value;
  if (!results_maybe.ToLocal(&results_value)) return;
  // TODO(7742): Allow any iterable, not just {Array} here.
  if (!results_value->IsArray()) {
    thrower.TypeError("Argument 0 must be a function type with 'results'");
    return;
  }
  Local<Array> results = results_value.As<Array>();
  uint32_t results_len = results->Length();
  if (results_len > (enabled_features.mv
                         ? i::wasm::kV8MaxWasmFunctionMultiReturns
                         : i::wasm::kV8MaxWasmFunctionReturns)) {
    thrower.TypeError("Argument 0 contains too many results");
    return;
  }

  // Decode the function type and construct a signature.
  i::Zone zone(i_isolate->allocator(), ZONE_NAME);
  i::wasm::FunctionSig::Builder builder(&zone, results_len, parameters_len);
  for (uint32_t i = 0; i < parameters_len; ++i) {
    i::wasm::ValueType type;
    MaybeLocal<Value> maybe = parameters->Get(context, i);
    if (!GetValueType(isolate, maybe, context, &type, enabled_features)) return;
    if (type == i::wasm::kWasmStmt) {
      thrower.TypeError(
          "Argument 0 parameter type at index #%u must be a value type", i);
      return;
    }
    builder.AddParam(type);
  }
  for (uint32_t i = 0; i < results_len; ++i) {
    i::wasm::ValueType type;
    MaybeLocal<Value> maybe = results->Get(context, i);
    if (!GetValueType(isolate, maybe, context, &type, enabled_features)) return;
    if (type == i::wasm::kWasmStmt) {
      thrower.TypeError(
          "Argument 0 result type at index #%u must be a value type", i);
      return;
    }
    builder.AddReturn(type);
  }

  if (!args[1]->IsFunction()) {
    thrower.TypeError("Argument 1 must be a function");
    return;
  }

  i::wasm::FunctionSig* sig = builder.Build();
  i::Handle<i::JSReceiver> callable =
      Utils::OpenHandle(*args[1].As<Function>());
  i::Handle<i::JSFunction> result =
      i::WasmJSFunction::New(i_isolate, sig, callable);
  args.GetReturnValue().Set(Utils::ToLocal(result));
}

// Converts the given {type} into a string representation that can be used in
// reflective functions. Should be kept in sync with the {GetValueType} helper.
Local<String> ToValueTypeString(Isolate* isolate, i::wasm::ValueType type) {
  Local<String> string;
  switch (type) {
    case i::wasm::kWasmI32: {
      string = v8_str(isolate, "i32");
      break;
    }
    case i::wasm::kWasmI64: {
      string = v8_str(isolate, "i64");
      break;
    }
    case i::wasm::kWasmF32: {
      string = v8_str(isolate, "f32");
      break;
    }
    case i::wasm::kWasmF64: {
      string = v8_str(isolate, "f64");
      break;
    }
    case i::wasm::kWasmAnyRef: {
      string = v8_str(isolate, "anyref");
      break;
    }
    default:
      UNREACHABLE();
  }
  return string;
}

// WebAssembly.Function.type(WebAssembly.Function) -> FunctionType
void WebAssemblyFunctionType(const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  HandleScope scope(isolate);
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  ScheduledErrorThrower thrower(i_isolate, "WebAssembly.Function.type()");

  i::wasm::FunctionSig* sig;
  i::Zone zone(i_isolate->allocator(), ZONE_NAME);
  i::Handle<i::Object> arg0 = Utils::OpenHandle(*args[0]);
  if (i::WasmExportedFunction::IsWasmExportedFunction(*arg0)) {
    sig = i::Handle<i::WasmExportedFunction>::cast(arg0)->sig();
  } else if (i::WasmJSFunction::IsWasmJSFunction(*arg0)) {
    sig = i::Handle<i::WasmJSFunction>::cast(arg0)->GetSignature(&zone);
  } else {
    thrower.TypeError("Argument 0 must be a WebAssembly.Function");
    return;
  }

  // Extract values for the {ValueType[]} arrays.
  size_t param_index = 0;
  i::ScopedVector<Local<Value>> param_values(sig->parameter_count());
  for (i::wasm::ValueType type : sig->parameters()) {
    param_values[param_index++] = ToValueTypeString(isolate, type);
  }
  size_t result_index = 0;
  i::ScopedVector<Local<Value>> result_values(sig->return_count());
  for (i::wasm::ValueType type : sig->returns()) {
    result_values[result_index++] = ToValueTypeString(isolate, type);
  }

  // Create the resulting {FunctionType} object.
  Local<Object> ret = v8::Object::New(isolate);
  Local<Context> context = isolate->GetCurrentContext();
  Local<Array> params =
      v8::Array::New(isolate, param_values.begin(), param_values.size());
  if (!ret->CreateDataProperty(context, v8_str(isolate, "parameters"), params)
           .IsJust()) {
    return;
  }
  Local<Array> results =
      v8::Array::New(isolate, result_values.begin(), result_values.size());
  if (!ret->CreateDataProperty(context, v8_str(isolate, "results"), results)
           .IsJust()) {
    return;
  }

  v8::ReturnValue<v8::Value> return_value = args.GetReturnValue();
  return_value.Set(ret);
}

constexpr const char* kName_WasmGlobalObject = "WebAssembly.Global";
constexpr const char* kName_WasmMemoryObject = "WebAssembly.Memory";
constexpr const char* kName_WasmInstanceObject = "WebAssembly.Instance";
constexpr const char* kName_WasmTableObject = "WebAssembly.Table";

#define EXTRACT_THIS(var, WasmType)                                  \
  i::Handle<i::WasmType> var;                                        \
  {                                                                  \
    i::Handle<i::Object> this_arg = Utils::OpenHandle(*args.This()); \
    if (!this_arg->Is##WasmType()) {                                 \
      thrower.TypeError("Receiver is not a %s", kName_##WasmType);   \
      return;                                                        \
    }                                                                \
    var = i::Handle<i::WasmType>::cast(this_arg);                    \
  }

void WebAssemblyInstanceGetExports(
  const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  HandleScope scope(isolate);
  ScheduledErrorThrower thrower(i_isolate, "WebAssembly.Instance.exports()");
  EXTRACT_THIS(receiver, WasmInstanceObject);
  i::Handle<i::JSObject> exports_object(receiver->exports_object(), i_isolate);
  args.GetReturnValue().Set(Utils::ToLocal(exports_object));
}

void WebAssemblyTableGetLength(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  HandleScope scope(isolate);
  ScheduledErrorThrower thrower(i_isolate, "WebAssembly.Table.length()");
  EXTRACT_THIS(receiver, WasmTableObject);
  args.GetReturnValue().Set(
      v8::Number::New(isolate, receiver->current_length()));
}

// WebAssembly.Table.grow(num) -> num
void WebAssemblyTableGrow(const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  HandleScope scope(isolate);
  ScheduledErrorThrower thrower(i_isolate, "WebAssembly.Table.grow()");
  Local<Context> context = isolate->GetCurrentContext();
  EXTRACT_THIS(receiver, WasmTableObject);

  uint32_t grow_by;
  if (!EnforceUint32("Argument 0", args[0], context, &thrower, &grow_by)) {
    return;
  }

  int old_size = i::WasmTableObject::Grow(i_isolate, receiver, grow_by,
                                          i_isolate->factory()->null_value());

  if (old_size < 0) {
    thrower.RangeError("failed to grow table by %u", grow_by);
    return;
  }
  v8::ReturnValue<v8::Value> return_value = args.GetReturnValue();
  return_value.Set(old_size);
}

// WebAssembly.Table.get(num) -> JSFunction
void WebAssemblyTableGet(const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  HandleScope scope(isolate);
  ScheduledErrorThrower thrower(i_isolate, "WebAssembly.Table.get()");
  Local<Context> context = isolate->GetCurrentContext();
  EXTRACT_THIS(receiver, WasmTableObject);

  uint32_t index;
  if (!EnforceUint32("Argument 0", args[0], context, &thrower, &index)) {
    return;
  }
  if (!i::WasmTableObject::IsInBounds(i_isolate, receiver, index)) {
    thrower.RangeError("invalid index %u into function table", index);
    return;
  }

  i::Handle<i::Object> result =
      i::WasmTableObject::Get(i_isolate, receiver, index);

  v8::ReturnValue<v8::Value> return_value = args.GetReturnValue();
  return_value.Set(Utils::ToLocal(result));
}

// WebAssembly.Table.set(num, JSFunction)
void WebAssemblyTableSet(const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  HandleScope scope(isolate);
  ScheduledErrorThrower thrower(i_isolate, "WebAssembly.Table.set()");
  Local<Context> context = isolate->GetCurrentContext();
  EXTRACT_THIS(table_object, WasmTableObject);

  // Parameter 0.
  uint32_t index;
  if (!EnforceUint32("Argument 0", args[0], context, &thrower, &index)) {
    return;
  }
  if (!i::WasmTableObject::IsInBounds(i_isolate, table_object, index)) {
    thrower.RangeError("invalid index %u into function table", index);
    return;
  }

  i::Handle<i::Object> element = Utils::OpenHandle(*args[1]);
  if (!i::WasmTableObject::IsValidElement(i_isolate, table_object, element)) {
    thrower.TypeError("Argument 1 must be null or a WebAssembly function");
    return;
  }
  i::WasmTableObject::Set(i_isolate, table_object, index, element);
}

// WebAssembly.Table.type(WebAssembly.Table) -> TableType
void WebAssemblyTableType(const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  HandleScope scope(isolate);
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  ScheduledErrorThrower thrower(i_isolate, "WebAssembly.Table.type()");

  auto maybe_table = GetFirstArgumentAsTable(args, &thrower);
  if (thrower.error()) return;
  i::Handle<i::WasmTableObject> table = maybe_table.ToHandleChecked();
  v8::Local<v8::Object> ret = v8::Object::New(isolate);

  Local<String> element;
  auto enabled_features = i::wasm::WasmFeaturesFromFlags();
  if (table->type() == i::wasm::ValueType::kWasmFuncRef) {
    element = v8_str(isolate, "anyfunc");
  } else if (enabled_features.anyref &&
             table->type() == i::wasm::ValueType::kWasmAnyRef) {
    element = v8_str(isolate, "anyref");
  } else {
    UNREACHABLE();
  }
  if (!ret->CreateDataProperty(isolate->GetCurrentContext(),
                               v8_str(isolate, "element"), element)
           .IsJust()) {
    return;
  }

  uint32_t curr_size = table->current_length();
  DCHECK_LE(curr_size, std::numeric_limits<uint32_t>::max());
  if (!ret->CreateDataProperty(isolate->GetCurrentContext(),
                               v8_str(isolate, "minimum"),
                               v8::Integer::NewFromUnsigned(
                                   isolate, static_cast<uint32_t>(curr_size)))
           .IsJust()) {
    return;
  }

  if (!table->maximum_length().IsUndefined()) {
    uint64_t max_size = table->maximum_length().Number();
    DCHECK_LE(max_size, std::numeric_limits<uint32_t>::max());
    if (!ret->CreateDataProperty(isolate->GetCurrentContext(),
                                 v8_str(isolate, "maximum"),
                                 v8::Integer::NewFromUnsigned(
                                     isolate, static_cast<uint32_t>(max_size)))
             .IsJust()) {
      return;
    }
  }

  v8::ReturnValue<v8::Value> return_value = args.GetReturnValue();
  return_value.Set(ret);
}

// WebAssembly.Memory.grow(num) -> num
void WebAssemblyMemoryGrow(const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  HandleScope scope(isolate);
  ScheduledErrorThrower thrower(i_isolate, "WebAssembly.Memory.grow()");
  Local<Context> context = isolate->GetCurrentContext();
  EXTRACT_THIS(receiver, WasmMemoryObject);

  uint32_t delta_size;
  if (!EnforceUint32("Argument 0", args[0], context, &thrower, &delta_size)) {
    return;
  }

  uint64_t max_size64 = receiver->maximum_pages();
  if (max_size64 > uint64_t{i::wasm::max_mem_pages()}) {
    max_size64 = i::wasm::max_mem_pages();
  }
  i::Handle<i::JSArrayBuffer> old_buffer(receiver->array_buffer(), i_isolate);

  DCHECK_LE(max_size64, std::numeric_limits<uint32_t>::max());

  uint64_t old_size64 = old_buffer->byte_length() / i::wasm::kWasmPageSize;
  uint64_t new_size64 = old_size64 + static_cast<uint64_t>(delta_size);

  if (new_size64 > max_size64) {
    thrower.RangeError("Maximum memory size exceeded");
    return;
  }

  int32_t ret = i::WasmMemoryObject::Grow(i_isolate, receiver, delta_size);
  if (ret == -1) {
    thrower.RangeError("Unable to grow instance memory.");
    return;
  }
  v8::ReturnValue<v8::Value> return_value = args.GetReturnValue();
  return_value.Set(ret);
}

// WebAssembly.Memory.buffer -> ArrayBuffer
void WebAssemblyMemoryGetBuffer(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  HandleScope scope(isolate);
  ScheduledErrorThrower thrower(i_isolate, "WebAssembly.Memory.buffer");
  EXTRACT_THIS(receiver, WasmMemoryObject);

  i::Handle<i::Object> buffer_obj(receiver->array_buffer(), i_isolate);
  DCHECK(buffer_obj->IsJSArrayBuffer());
  i::Handle<i::JSArrayBuffer> buffer(i::JSArrayBuffer::cast(*buffer_obj),
                                     i_isolate);
  if (buffer->is_shared()) {
    // TODO(gdeepti): More needed here for when cached buffer, and current
    // buffer are out of sync, handle that here when bounds checks, and Grow
    // are handled correctly.
    Maybe<bool> result =
        buffer->SetIntegrityLevel(buffer, i::FROZEN, i::kDontThrow);
    if (!result.FromJust()) {
      thrower.TypeError(
          "Status of setting SetIntegrityLevel of buffer is false.");
    }
  }
  v8::ReturnValue<v8::Value> return_value = args.GetReturnValue();
  return_value.Set(Utils::ToLocal(buffer));
}

// WebAssembly.Memory.type(WebAssembly.Memory) -> MemoryType
void WebAssemblyMemoryType(const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  HandleScope scope(isolate);
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  ScheduledErrorThrower thrower(i_isolate, "WebAssembly.Memory.type()");

  auto maybe_memory = GetFirstArgumentAsMemory(args, &thrower);
  if (thrower.error()) return;
  i::Handle<i::WasmMemoryObject> memory = maybe_memory.ToHandleChecked();
  v8::Local<v8::Object> ret = v8::Object::New(isolate);
  i::Handle<i::JSArrayBuffer> buffer(memory->array_buffer(), i_isolate);

  size_t curr_size = buffer->byte_length() / i::wasm::kWasmPageSize;
  DCHECK_LE(curr_size, std::numeric_limits<uint32_t>::max());
  if (!ret->CreateDataProperty(isolate->GetCurrentContext(),
                               v8_str(isolate, "minimum"),
                               v8::Integer::NewFromUnsigned(
                                   isolate, static_cast<uint32_t>(curr_size)))
           .IsJust()) {
    return;
  }

  if (memory->has_maximum_pages()) {
    uint64_t max_size = memory->maximum_pages();
    DCHECK_LE(max_size, std::numeric_limits<uint32_t>::max());
    if (!ret->CreateDataProperty(isolate->GetCurrentContext(),
                                 v8_str(isolate, "maximum"),
                                 v8::Integer::NewFromUnsigned(
                                     isolate, static_cast<uint32_t>(max_size)))
             .IsJust()) {
      return;
    }
  }

  v8::ReturnValue<v8::Value> return_value = args.GetReturnValue();
  return_value.Set(ret);
}

void WebAssemblyGlobalGetValueCommon(
    const v8::FunctionCallbackInfo<v8::Value>& args, const char* name) {
  v8::Isolate* isolate = args.GetIsolate();
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  HandleScope scope(isolate);
  ScheduledErrorThrower thrower(i_isolate, name);
  EXTRACT_THIS(receiver, WasmGlobalObject);

  v8::ReturnValue<v8::Value> return_value = args.GetReturnValue();

  switch (receiver->type()) {
    case i::wasm::kWasmI32:
      return_value.Set(receiver->GetI32());
      break;
    case i::wasm::kWasmI64: {
      auto enabled_features = i::wasm::WasmFeaturesFromIsolate(i_isolate);
      if (enabled_features.bigint) {
        Local<BigInt> value = BigInt::New(isolate, receiver->GetI64());

        return_value.Set(value);
      } else {
        thrower.TypeError("Can't get the value of i64 WebAssembly.Global");
      }
      break;
    }
    case i::wasm::kWasmF32:
      return_value.Set(receiver->GetF32());
      break;
    case i::wasm::kWasmF64:
      return_value.Set(receiver->GetF64());
      break;
    case i::wasm::kWasmAnyRef:
    case i::wasm::kWasmFuncRef:
    case i::wasm::kWasmExnRef:
      return_value.Set(Utils::ToLocal(receiver->GetRef()));
      break;
    default:
      UNREACHABLE();
  }
}

// WebAssembly.Global.valueOf() -> num
void WebAssemblyGlobalValueOf(const v8::FunctionCallbackInfo<v8::Value>& args) {
  return WebAssemblyGlobalGetValueCommon(args, "WebAssembly.Global.valueOf()");
}

// get WebAssembly.Global.value -> num
void WebAssemblyGlobalGetValue(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  return WebAssemblyGlobalGetValueCommon(args, "get WebAssembly.Global.value");
}

// set WebAssembly.Global.value(num)
void WebAssemblyGlobalSetValue(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  HandleScope scope(isolate);
  Local<Context> context = isolate->GetCurrentContext();
  ScheduledErrorThrower thrower(i_isolate, "set WebAssembly.Global.value");
  EXTRACT_THIS(receiver, WasmGlobalObject);

  if (!receiver->is_mutable()) {
    thrower.TypeError("Can't set the value of an immutable global.");
    return;
  }
  if (args[0]->IsUndefined()) {
    thrower.TypeError("Argument 0 is required");
    return;
  }

  switch (receiver->type()) {
    case i::wasm::kWasmI32: {
      int32_t i32_value = 0;
      if (!args[0]->Int32Value(context).To(&i32_value)) return;
      receiver->SetI32(i32_value);
      break;
    }
    case i::wasm::kWasmI64: {
      auto enabled_features = i::wasm::WasmFeaturesFromIsolate(i_isolate);
      if (enabled_features.bigint) {
        v8::Local<v8::BigInt> bigint_value;
        if (!args[0]->ToBigInt(context).ToLocal(&bigint_value)) return;
        receiver->SetI64(bigint_value->Int64Value());
      } else {
        thrower.TypeError("Can't set the value of i64 WebAssembly.Global");
      }
      break;
    }
    case i::wasm::kWasmF32: {
      double f64_value = 0;
      if (!args[0]->NumberValue(context).To(&f64_value)) return;
      receiver->SetF32(i::DoubleToFloat32(f64_value));
      break;
    }
    case i::wasm::kWasmF64: {
      double f64_value = 0;
      if (!args[0]->NumberValue(context).To(&f64_value)) return;
      receiver->SetF64(f64_value);
      break;
    }
    case i::wasm::kWasmAnyRef:
    case i::wasm::kWasmExnRef: {
      receiver->SetAnyRef(Utils::OpenHandle(*args[0]));
      break;
    }
    case i::wasm::kWasmFuncRef: {
      if (!receiver->SetFuncRef(i_isolate, Utils::OpenHandle(*args[0]))) {
        thrower.TypeError(
            "value of an anyfunc reference must be either null or an "
            "exported function");
      }
      break;
    }
    default:
      UNREACHABLE();
  }
}

// WebAssembly.Global.type(WebAssembly.Global) -> GlobalType
void WebAssemblyGlobalType(const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  HandleScope scope(isolate);
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  ScheduledErrorThrower thrower(i_isolate, "WebAssembly.Global.type()");

  auto maybe_global = GetFirstArgumentAsGlobal(args, &thrower);
  if (thrower.error()) return;
  i::Handle<i::WasmGlobalObject> global = maybe_global.ToHandleChecked();
  v8::Local<v8::Object> ret = v8::Object::New(isolate);

  if (!ret->CreateDataProperty(isolate->GetCurrentContext(),
                               v8_str(isolate, "mutable"),
                               v8::Boolean::New(isolate, global->is_mutable()))
           .IsJust()) {
    return;
  }

  Local<String> type = ToValueTypeString(isolate, global->type());
  if (!ret->CreateDataProperty(isolate->GetCurrentContext(),
                               v8_str(isolate, "value"), type)
           .IsJust()) {
    return;
  }

  v8::ReturnValue<v8::Value> return_value = args.GetReturnValue();
  return_value.Set(ret);
}

}  // namespace

// TODO(titzer): we use the API to create the function template because the
// internal guts are too ugly to replicate here.
static i::Handle<i::FunctionTemplateInfo> NewFunctionTemplate(
    i::Isolate* i_isolate, FunctionCallback func) {
  Isolate* isolate = reinterpret_cast<Isolate*>(i_isolate);
  Local<FunctionTemplate> templ = FunctionTemplate::New(isolate, func);
  templ->ReadOnlyPrototype();
  return v8::Utils::OpenHandle(*templ);
}

static i::Handle<i::ObjectTemplateInfo> NewObjectTemplate(
    i::Isolate* i_isolate) {
  Isolate* isolate = reinterpret_cast<Isolate*>(i_isolate);
  Local<ObjectTemplate> templ = ObjectTemplate::New(isolate);
  return v8::Utils::OpenHandle(*templ);
}

namespace internal {

Handle<JSFunction> CreateFunc(Isolate* isolate, Handle<String> name,
                              FunctionCallback func) {
  Handle<FunctionTemplateInfo> temp = NewFunctionTemplate(isolate, func);
  Handle<JSFunction> function =
      ApiNatives::InstantiateFunction(temp, name).ToHandleChecked();
  DCHECK(function->shared().HasSharedName());
  return function;
}

Handle<JSFunction> InstallFunc(Isolate* isolate, Handle<JSObject> object,
                               const char* str, FunctionCallback func,
                               int length = 0,
                               PropertyAttributes attributes = NONE) {
  Handle<String> name = v8_str(isolate, str);
  Handle<JSFunction> function = CreateFunc(isolate, name, func);
  function->shared().set_length(length);
  JSObject::AddProperty(isolate, object, name, function, attributes);
  return function;
}

Handle<JSFunction> InstallConstructorFunc(Isolate* isolate,
                                         Handle<JSObject> object,
                                         const char* str,
                                         FunctionCallback func) {
  return InstallFunc(isolate, object, str, func, 1, DONT_ENUM);
}

Handle<String> GetterName(Isolate* isolate, Handle<String> name) {
  return Name::ToFunctionName(isolate, name, isolate->factory()->get_string())
      .ToHandleChecked();
}

void InstallGetter(Isolate* isolate, Handle<JSObject> object, const char* str,
                   FunctionCallback func) {
  Handle<String> name = v8_str(isolate, str);
  Handle<JSFunction> function =
      CreateFunc(isolate, GetterName(isolate, name), func);

  Utils::ToLocal(object)->SetAccessorProperty(Utils::ToLocal(name),
                                              Utils::ToLocal(function),
                                              Local<Function>(), v8::None);
}

Handle<String> SetterName(Isolate* isolate, Handle<String> name) {
  return Name::ToFunctionName(isolate, name, isolate->factory()->set_string())
      .ToHandleChecked();
}

void InstallGetterSetter(Isolate* isolate, Handle<JSObject> object,
                         const char* str, FunctionCallback getter,
                         FunctionCallback setter) {
  Handle<String> name = v8_str(isolate, str);
  Handle<JSFunction> getter_func =
      CreateFunc(isolate, GetterName(isolate, name), getter);
  Handle<JSFunction> setter_func =
      CreateFunc(isolate, SetterName(isolate, name), setter);
  setter_func->shared().set_length(1);

  v8::PropertyAttribute attributes = v8::None;

  Utils::ToLocal(object)->SetAccessorProperty(
      Utils::ToLocal(name), Utils::ToLocal(getter_func),
      Utils::ToLocal(setter_func), attributes);
}

// Assigns a dummy instance template to the given constructor function. Used to
// make sure the implicit receivers for the constructors in this file have an
// instance type different from the internal one, they allocate the resulting
// object explicitly and ignore implicit receiver.
void SetDummyInstanceTemplate(Isolate* isolate, Handle<JSFunction> fun) {
  Handle<ObjectTemplateInfo> instance_template = NewObjectTemplate(isolate);
  FunctionTemplateInfo::SetInstanceTemplate(
      isolate, handle(fun->shared().get_api_func_data(), isolate),
      instance_template);
}

// static
void WasmJs::Install(Isolate* isolate, bool exposed_on_global_object) {
  Handle<JSGlobalObject> global = isolate->global_object();
  Handle<Context> context(global->native_context(), isolate);
  // Install the JS API once only.
  Object prev = context->get(Context::WASM_MODULE_CONSTRUCTOR_INDEX);
  if (!prev.IsUndefined(isolate)) {
    DCHECK(prev.IsJSFunction());
    return;
  }

  Factory* factory = isolate->factory();

  // Setup WebAssembly
  Handle<String> name = v8_str(isolate, "WebAssembly");
  NewFunctionArgs args = NewFunctionArgs::ForFunctionWithoutCode(
      name, isolate->strict_function_map(), LanguageMode::kStrict);
  Handle<JSFunction> cons = factory->NewFunction(args);
  JSFunction::SetPrototype(cons, isolate->initial_object_prototype());
  Handle<JSObject> webassembly =
      factory->NewJSObject(cons, AllocationType::kOld);

  PropertyAttributes ro_attributes =
      static_cast<PropertyAttributes>(DONT_ENUM | READ_ONLY);
  JSObject::AddProperty(isolate, webassembly, factory->to_string_tag_symbol(),
                        name, ro_attributes);
  InstallFunc(isolate, webassembly, "compile", WebAssemblyCompile, 1);
  InstallFunc(isolate, webassembly, "validate", WebAssemblyValidate, 1);
  InstallFunc(isolate, webassembly, "instantiate", WebAssemblyInstantiate, 1);

  if (FLAG_wasm_test_streaming) {
    isolate->set_wasm_streaming_callback(WasmStreamingCallbackForTesting);
  }

  if (isolate->wasm_streaming_callback() != nullptr) {
    InstallFunc(isolate, webassembly, "compileStreaming",
                WebAssemblyCompileStreaming, 1);
    InstallFunc(isolate, webassembly, "instantiateStreaming",
                WebAssemblyInstantiateStreaming, 1);
  }

  // Expose the API on the global object if configured to do so.
  if (exposed_on_global_object) {
    JSObject::AddProperty(isolate, global, name, webassembly, DONT_ENUM);
  }

  // Setup Module
  Handle<JSFunction> module_constructor =
      InstallConstructorFunc(isolate, webassembly, "Module", WebAssemblyModule);
  context->set_wasm_module_constructor(*module_constructor);
  SetDummyInstanceTemplate(isolate, module_constructor);
  JSFunction::EnsureHasInitialMap(module_constructor);
  Handle<JSObject> module_proto(
      JSObject::cast(module_constructor->instance_prototype()), isolate);
  Handle<Map> module_map =
      isolate->factory()->NewMap(i::WASM_MODULE_TYPE, WasmModuleObject::kSize);
  JSFunction::SetInitialMap(module_constructor, module_map, module_proto);
  InstallFunc(isolate, module_constructor, "imports", WebAssemblyModuleImports,
              1);
  InstallFunc(isolate, module_constructor, "exports", WebAssemblyModuleExports,
              1);
  InstallFunc(isolate, module_constructor, "customSections",
              WebAssemblyModuleCustomSections, 2);
  JSObject::AddProperty(isolate, module_proto, factory->to_string_tag_symbol(),
                        v8_str(isolate, "WebAssembly.Module"), ro_attributes);

  // Setup Instance
  Handle<JSFunction> instance_constructor = InstallConstructorFunc(
      isolate, webassembly, "Instance", WebAssemblyInstance);
  context->set_wasm_instance_constructor(*instance_constructor);
  SetDummyInstanceTemplate(isolate, instance_constructor);
  JSFunction::EnsureHasInitialMap(instance_constructor);
  Handle<JSObject> instance_proto(
      JSObject::cast(instance_constructor->instance_prototype()), isolate);
  Handle<Map> instance_map = isolate->factory()->NewMap(
      i::WASM_INSTANCE_TYPE, WasmInstanceObject::kSize);
  JSFunction::SetInitialMap(instance_constructor, instance_map, instance_proto);
  InstallGetter(isolate, instance_proto, "exports",
                WebAssemblyInstanceGetExports);
  JSObject::AddProperty(isolate, instance_proto,
                        factory->to_string_tag_symbol(),
                        v8_str(isolate, "WebAssembly.Instance"), ro_attributes);

  // The context is not set up completely yet. That's why we cannot use
  // {WasmFeaturesFromIsolate} and have to use {WasmFeaturesFromFlags} instead.
  auto enabled_features = i::wasm::WasmFeaturesFromFlags();

  // Setup Table
  Handle<JSFunction> table_constructor =
      InstallConstructorFunc(isolate, webassembly, "Table", WebAssemblyTable);
  context->set_wasm_table_constructor(*table_constructor);
  SetDummyInstanceTemplate(isolate, table_constructor);
  JSFunction::EnsureHasInitialMap(table_constructor);
  Handle<JSObject> table_proto(
      JSObject::cast(table_constructor->instance_prototype()), isolate);
  Handle<Map> table_map =
      isolate->factory()->NewMap(i::WASM_TABLE_TYPE, WasmTableObject::kSize);
  JSFunction::SetInitialMap(table_constructor, table_map, table_proto);
  InstallGetter(isolate, table_proto, "length", WebAssemblyTableGetLength);
  InstallFunc(isolate, table_proto, "grow", WebAssemblyTableGrow, 1);
  InstallFunc(isolate, table_proto, "get", WebAssemblyTableGet, 1);
  InstallFunc(isolate, table_proto, "set", WebAssemblyTableSet, 2);
  if (enabled_features.type_reflection) {
    InstallFunc(isolate, table_constructor, "type", WebAssemblyTableType, 1);
  }
  JSObject::AddProperty(isolate, table_proto, factory->to_string_tag_symbol(),
                        v8_str(isolate, "WebAssembly.Table"), ro_attributes);

  // Setup Memory
  Handle<JSFunction> memory_constructor =
      InstallConstructorFunc(isolate, webassembly, "Memory", WebAssemblyMemory);
  context->set_wasm_memory_constructor(*memory_constructor);
  SetDummyInstanceTemplate(isolate, memory_constructor);
  JSFunction::EnsureHasInitialMap(memory_constructor);
  Handle<JSObject> memory_proto(
      JSObject::cast(memory_constructor->instance_prototype()), isolate);
  Handle<Map> memory_map =
      isolate->factory()->NewMap(i::WASM_MEMORY_TYPE, WasmMemoryObject::kSize);
  JSFunction::SetInitialMap(memory_constructor, memory_map, memory_proto);
  InstallFunc(isolate, memory_proto, "grow", WebAssemblyMemoryGrow, 1);
  InstallGetter(isolate, memory_proto, "buffer", WebAssemblyMemoryGetBuffer);
  if (enabled_features.type_reflection) {
    InstallFunc(isolate, memory_constructor, "type", WebAssemblyMemoryType, 1);
  }
  JSObject::AddProperty(isolate, memory_proto, factory->to_string_tag_symbol(),
                        v8_str(isolate, "WebAssembly.Memory"), ro_attributes);

  // Setup Global
  Handle<JSFunction> global_constructor =
      InstallConstructorFunc(isolate, webassembly, "Global", WebAssemblyGlobal);
  context->set_wasm_global_constructor(*global_constructor);
  SetDummyInstanceTemplate(isolate, global_constructor);
  JSFunction::EnsureHasInitialMap(global_constructor);
  Handle<JSObject> global_proto(
      JSObject::cast(global_constructor->instance_prototype()), isolate);
  Handle<Map> global_map =
      isolate->factory()->NewMap(i::WASM_GLOBAL_TYPE, WasmGlobalObject::kSize);
  JSFunction::SetInitialMap(global_constructor, global_map, global_proto);
  InstallFunc(isolate, global_proto, "valueOf", WebAssemblyGlobalValueOf, 0);
  InstallGetterSetter(isolate, global_proto, "value", WebAssemblyGlobalGetValue,
                      WebAssemblyGlobalSetValue);
  if (enabled_features.type_reflection) {
    InstallFunc(isolate, global_constructor, "type", WebAssemblyGlobalType, 1);
  }
  JSObject::AddProperty(isolate, global_proto, factory->to_string_tag_symbol(),
                        v8_str(isolate, "WebAssembly.Global"), ro_attributes);

  // Setup Exception
  if (enabled_features.eh) {
    Handle<JSFunction> exception_constructor = InstallConstructorFunc(
        isolate, webassembly, "Exception", WebAssemblyException);
    context->set_wasm_exception_constructor(*exception_constructor);
    SetDummyInstanceTemplate(isolate, exception_constructor);
    JSFunction::EnsureHasInitialMap(exception_constructor);
    Handle<JSObject> exception_proto(
        JSObject::cast(exception_constructor->instance_prototype()), isolate);
    Handle<Map> exception_map = isolate->factory()->NewMap(
        i::WASM_EXCEPTION_TYPE, WasmExceptionObject::kSize);
    JSFunction::SetInitialMap(exception_constructor, exception_map,
                              exception_proto);
  }

  // Setup Function
  if (enabled_features.type_reflection) {
    Handle<JSFunction> function_constructor = InstallConstructorFunc(
        isolate, webassembly, "Function", WebAssemblyFunction);
    SetDummyInstanceTemplate(isolate, function_constructor);
    JSFunction::EnsureHasInitialMap(function_constructor);
    Handle<JSObject> function_proto(
        JSObject::cast(function_constructor->instance_prototype()), isolate);
    Handle<Map> function_map = isolate->factory()->CreateSloppyFunctionMap(
        FUNCTION_WITHOUT_PROTOTYPE, MaybeHandle<JSFunction>());
    CHECK(JSObject::SetPrototype(
              function_proto,
              handle(context->function_function().prototype(), isolate), false,
              kDontThrow)
              .FromJust());
    JSFunction::SetInitialMap(function_constructor, function_map,
                              function_proto);
    InstallFunc(isolate, function_constructor, "type", WebAssemblyFunctionType,
                1);
    // Make all exported functions an instance of {WebAssembly.Function}.
    context->set_wasm_exported_function_map(*function_map);
  } else {
    // Make all exported functions an instance of {Function}.
    Handle<Map> function_map = isolate->sloppy_function_without_prototype_map();
    context->set_wasm_exported_function_map(*function_map);
  }

  // Setup errors
  Handle<JSFunction> compile_error(
      isolate->native_context()->wasm_compile_error_function(), isolate);
  JSObject::AddProperty(isolate, webassembly,
                        isolate->factory()->CompileError_string(),
                        compile_error, DONT_ENUM);
  Handle<JSFunction> link_error(
      isolate->native_context()->wasm_link_error_function(), isolate);
  JSObject::AddProperty(isolate, webassembly,
                        isolate->factory()->LinkError_string(), link_error,
                        DONT_ENUM);
  Handle<JSFunction> runtime_error(
      isolate->native_context()->wasm_runtime_error_function(), isolate);
  JSObject::AddProperty(isolate, webassembly,
                        isolate->factory()->RuntimeError_string(),
                        runtime_error, DONT_ENUM);
}

#undef ASSIGN
#undef EXTRACT_THIS

}  // namespace internal
}  // namespace v8
