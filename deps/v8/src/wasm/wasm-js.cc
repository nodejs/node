// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/wasm/wasm-js.h"

#include <cinttypes>
#include <cstring>
#include <optional>

#include "include/v8-function.h"
#include "include/v8-persistent-handle.h"
#include "include/v8-promise.h"
#include "include/v8-wasm.h"
#include "src/api/api-arguments.h"
#include "src/api/api-inl.h"
#include "src/api/api-natives.h"
#include "src/base/logging.h"
#include "src/base/unique-array.h"
#include "src/builtins/builtins-utils-inl.h"
#include "src/execution/execution.h"
#include "src/execution/isolate.h"
#include "src/execution/messages.h"
#include "src/flags/flags.h"
#include "src/handles/handles.h"
#include "src/heap/factory.h"
#include "src/objects/fixed-array.h"
#include "src/objects/instance-type.h"
#include "src/objects/js-array-buffer-inl.h"
#include "src/objects/js-function.h"
#include "src/objects/managed-inl.h"
#include "src/objects/object-conversions-inl.h"
#include "src/objects/objects-inl.h"
#include "src/objects/shared-function-info.h"
#include "src/objects/templates.h"
#include "src/wasm/function-compiler.h"
#include "src/wasm/streaming-decoder.h"
#include "src/wasm/value-type.h"
#include "src/wasm/wasm-debug.h"
#include "src/wasm/wasm-engine.h"
#include "src/wasm/wasm-limits.h"
#include "src/wasm/wasm-memory-map-descriptor.h"
#include "src/wasm/wasm-objects-inl.h"
#include "src/wasm/wasm-serialization.h"
#include "src/wasm/wasm-value.h"

namespace v8 {

using i::wasm::AddressType;
using i::wasm::CompileTimeImport;
using i::wasm::CompileTimeImports;
using i::wasm::ErrorThrower;
using i::wasm::WasmEnabledFeatures;

// This is the implementation of the public `WasmStreaming` API, see
// `include/v8-wasm.h`.
class WasmStreaming::WasmStreamingImpl {
 public:
  WasmStreamingImpl(
      i::Isolate* isolate, const char* api_method_name,
      CompileTimeImports compile_imports,
      std::shared_ptr<internal::wasm::CompilationResultResolver> resolver)
      : i_isolate_(isolate),
        streaming_decoder_(i::wasm::GetWasmEngine()->StartStreamingCompilation(
            WasmEnabledFeatures::FromIsolate(i_isolate_),
            std::move(compile_imports), api_method_name, resolver)),
        resolver_(std::move(resolver)) {
    streaming_decoder_->InitializeIsolateSpecificInfo(isolate);
  }

  void OnBytesReceived(const uint8_t* bytes, size_t size) {
    streaming_decoder_->OnBytesReceived(base::VectorOf(bytes, size));
  }
  void Finish(const WasmStreaming::ModuleCachingCallback& caching_callback) {
    // Finish can only be called on the main thread of the isolate.
    CHECK_EQ(i_isolate_->thread_id(), base::ThreadId::Current());
    streaming_decoder_->Finish(caching_callback);
  }

  void Abort(i::MaybeHandle<i::JSAny> exception) {
    i::HandleScope scope(i_isolate_);
    streaming_decoder_->Abort();

    // If no exception value is provided, we do not reject the promise. This can
    // happen when streaming compilation gets aborted when no script execution
    // is allowed anymore, e.g. when a browser tab gets refreshed.
    if (exception.is_null()) return;

    resolver_->OnCompilationFailed(exception.ToHandleChecked());
  }

  void SetMoreFunctionsCanBeSerializedCallback(
      std::function<void(CompiledWasmModule)> callback) {
    streaming_decoder_->SetMoreFunctionsCanBeSerializedCallback(
        [callback = std::move(callback),
         url = streaming_decoder_->shared_url()](
            const std::shared_ptr<i::wasm::NativeModule>& native_module) {
          callback(CompiledWasmModule{native_module, *url});
        });
  }

  void SetHasCompiledModuleBytes() {
    streaming_decoder_->SetHasCompiledModuleBytes();
  }

  void SetUrl(std::string_view url) { streaming_decoder_->SetUrl(url); }

 private:
  i::Isolate* const i_isolate_;
  const std::shared_ptr<i::wasm::StreamingDecoder> streaming_decoder_;
  const std::shared_ptr<i::wasm::CompilationResultResolver> resolver_;
};

WasmStreaming::WasmStreaming(std::unique_ptr<WasmStreamingImpl> impl)
    : impl_(std::move(impl)) {
  TRACE_EVENT("v8.wasm", "wasm.InitializeStreaming");
}

// The destructor is defined here because we have a unique_ptr with forward
// declaration.
WasmStreaming::~WasmStreaming() = default;

void WasmStreaming::OnBytesReceived(const uint8_t* bytes, size_t size) {
  TRACE_EVENT("v8.wasm", "wasm.OnBytesReceived", "bytes", size);
  impl_->OnBytesReceived(bytes, size);
}

void WasmStreaming::Finish(
    const WasmStreaming::ModuleCachingCallback& caching_callback) {
  TRACE_EVENT("v8.wasm", "wasm.FinishStreaming");
  impl_->Finish(caching_callback);
}

void WasmStreaming::Abort(MaybeLocal<Value> exception) {
  TRACE_EVENT("v8.wasm", "wasm.AbortStreaming");
  i::MaybeHandle<i::JSAny> maybe_exception;
  if (!exception.IsEmpty()) {
    maybe_exception =
        Cast<i::JSAny>(Utils::OpenHandle(*exception.ToLocalChecked()));
  }
  impl_->Abort(maybe_exception);
}

void WasmStreaming::SetHasCompiledModuleBytes() {
  impl_->SetHasCompiledModuleBytes();
}

void WasmStreaming::SetMoreFunctionsCanBeSerializedCallback(
    std::function<void(CompiledWasmModule)> callback) {
  impl_->SetMoreFunctionsCanBeSerializedCallback(std::move(callback));
}

void WasmStreaming::SetUrl(const char* url, size_t length) {
  DCHECK_EQ('\0', url[length]);  // {url} is null-terminated.
  TRACE_EVENT("v8.wasm", "wasm.SetUrl", "url", url);
  impl_->SetUrl(std::string_view(url, length));
}

// static
std::shared_ptr<WasmStreaming> WasmStreaming::Unpack(Isolate* isolate,
                                                     Local<Data> data) {
  TRACE_EVENT("v8.wasm", "wasm.WasmStreaming.Unpack");
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  i::HandleScope scope(i_isolate);
  auto managed =
      i::Cast<i::CppGCManaged<WasmStreaming>>(Utils::OpenDirectHandle(*data));
  std::shared_ptr<WasmStreaming> streaming = managed->get();
  managed->SetManagedObject({}, i_isolate, 0);
  return streaming;
}

class WasmModuleCompilation::Impl {
 public:
  Impl(WasmEnabledFeatures enabled_features, CompileTimeImports compile_imports)
      : streaming_decoder_(i::wasm::GetWasmEngine()->StartStreamingCompilation(
            enabled_features, std::move(compile_imports),
            "WasmModuleCompilation", resolver_)) {}

  void OnBytesReceived(const uint8_t* bytes, size_t size) {
    streaming_decoder_->OnBytesReceived(base::VectorOf(bytes, size));
  }

  void Finish(
      Isolate* isolate, const ModuleCachingCallback& caching_callback,
      const std::function<void(
          std::variant<Local<WasmModuleObject>, Local<Value>> module_or_error)>&
          resolution_callback) {
    DCHECK_NULL(resolver_->callback_);
    resolver_->callback_ = resolution_callback;

    streaming_decoder_->InitializeIsolateSpecificInfo(
        reinterpret_cast<i::Isolate*>(isolate));
    streaming_decoder_->Finish(caching_callback);
  }

  void Abort() { streaming_decoder_->Abort(); }

  void SetMoreFunctionsCanBeSerializedCallback(
      std::function<void(CompiledWasmModule)> callback) {
    streaming_decoder_->SetMoreFunctionsCanBeSerializedCallback(
        [callback = std::move(callback),
         url = streaming_decoder_->shared_url()](
            const std::shared_ptr<i::wasm::NativeModule>& native_module) {
          callback(CompiledWasmModule{native_module, *url});
        });
  }

  void SetHasCompiledModuleBytes() {
    streaming_decoder_->SetHasCompiledModuleBytes();
  }

  void SetUrl(std::string_view url) { streaming_decoder_->SetUrl(url); }

 private:
  class Resolver final : public i::wasm::CompilationResultResolver {
   public:
    void OnCompilationSucceeded(
        i::DirectHandle<i::WasmModuleObject> result) override {
      DCHECK_NOT_NULL(callback_);
      callback_(Utils::ToLocal(result));
    }
    void OnCompilationFailed(i::DirectHandle<i::JSAny> error_reason) override {
      DCHECK_NOT_NULL(callback_);
      callback_(Utils::ToLocal(error_reason));
    }

   private:
    friend class WasmModuleCompilation::Impl;
    std::function<void(
        std::variant<Local<WasmModuleObject>, Local<Value>> module_or_error)>
        callback_;
  };

  const std::shared_ptr<Resolver> resolver_ = std::make_shared<Resolver>();
  const std::shared_ptr<i::wasm::StreamingDecoder> streaming_decoder_;
};

// TODO(clemensb): Pass enabled features.
WasmModuleCompilation::WasmModuleCompilation(const CompileOptions& options)
    : impl_(std::make_unique<Impl>(
          WasmEnabledFeatures::FromFlags(),
          i::wasm::CompileTimeImportsFromOptions(options))) {
  TRACE_EVENT("v8.wasm", "wasm.ModuleCompilation");
  if (!options.source_url.empty()) {
    impl_->SetUrl(options.source_url);
  }
}

WasmModuleCompilation::~WasmModuleCompilation() = default;

void WasmModuleCompilation::OnBytesReceived(const uint8_t* bytes, size_t size) {
  TRACE_EVENT("v8.wasm", "wasm.OnBytesReceived", "bytes", size);
  impl_->OnBytesReceived(bytes, size);
}

void WasmModuleCompilation::Finish(
    Isolate* isolate, const ModuleCachingCallback& caching_callback,
    const std::function<void(std::variant<Local<WasmModuleObject>, Local<Value>>
                                 module_or_error)>& resolution_callback) {
  TRACE_EVENT("v8.wasm", "wasm.FinishModuleCompilation");
  impl_->Finish(isolate, caching_callback, resolution_callback);
}

void WasmModuleCompilation::Abort() {
  TRACE_EVENT("v8.wasm", "wasm.AbortModuleCompilation");
  impl_->Abort();
}

void WasmModuleCompilation::SetHasCompiledModuleBytes() {
  impl_->SetHasCompiledModuleBytes();
}

void WasmModuleCompilation::SetMoreFunctionsCanBeSerializedCallback(
    std::function<void(CompiledWasmModule)> callback) {
  impl_->SetMoreFunctionsCanBeSerializedCallback(std::move(callback));
}

void WasmModuleCompilation::SetUrl(const char* url, size_t length) {
  DCHECK_EQ('\0', url[length]);  // {url} is null-terminated.
  TRACE_EVENT("v8.wasm", "wasm.SetUrl", "url", url);
  impl_->SetUrl(std::string_view(url, length));
}

namespace internal {

// Note: The implementation of this function is in runtime-wasm.cc, in order
// to be able to use helpers that aren't visible outside that file.
void ToUtf8Lossy(Isolate* isolate, DirectHandle<String> string,
                 std::string& out);

namespace {

DirectHandle<String> v8_str(Isolate* isolate, const char* str) {
  return isolate->factory()->NewStringFromAsciiChecked(str);
}

#define GET_FIRST_ARGUMENT_AS(Type)                                         \
  MaybeDirectHandle<Wasm##Type##Object> GetArgumentAs##Type(                \
      DirectHandle<Object> arg0, ErrorThrower* thrower) {                   \
    if (!IsWasm##Type##Object(*arg0)) {                                     \
      thrower->TypeError("Argument 0 must be a WebAssembly." #Type);        \
      return {};                                                            \
    }                                                                       \
    return Cast<Wasm##Type##Object>(arg0);                                  \
  }                                                                         \
  MaybeDirectHandle<Wasm##Type##Object> GetFirstArgumentAs##Type(           \
      const v8::FunctionCallbackInfo<v8::Value>& info,                      \
      ErrorThrower* thrower) {                                              \
    return GetArgumentAs##Type(Utils::OpenDirectHandle(*info[0]), thrower); \
  }

GET_FIRST_ARGUMENT_AS(Module)
GET_FIRST_ARGUMENT_AS(Tag)

#undef GET_FIRST_ARGUMENT_AS

base::UniqueArray<const uint8_t> GetAndCopyArgumentAsBytes(
    DirectHandle<Object> source, size_t max_length, ErrorThrower* thrower) {
  DisallowGarbageCollection no_gc;
  const uint8_t* start = nullptr;
  size_t length = 0;
  if (Tagged<JSArrayBuffer> buffer;
      TryCast(*source, &buffer) && !buffer->is_shared()) {
    // A raw array buffer was passed.
    start = reinterpret_cast<const uint8_t*>(buffer->backing_store());
    length = buffer->GetByteLength();
  } else if (Tagged<JSTypedArray> array; TryCast(*source, &array)) {
    // A TypedArray was passed.
    start = reinterpret_cast<const uint8_t*>(array->DataPtr());
    length = array->GetByteLength();
  } else {
    thrower->TypeError("Argument 0 must be a buffer source");
    return {};
  }
  DCHECK_IMPLIES(length, start != nullptr);
  if (length == 0) {
    thrower->CompileError("BufferSource argument is empty");
    return {};
  }
  if (length > max_length) {
    // The spec requires a CompileError for implementation-defined limits, see
    // https://webassembly.github.io/spec/js-api/index.html#limits.
    thrower->CompileError("buffer source exceeds maximum size of %zu (is %zu)",
                          max_length, length);
    return {};
  }

  // Use relaxed reads (and writes, which is unnecessary here) to avoid TSan
  // reports in case the buffer is shared and is being modified concurrently.
  auto result = base::UniqueArray<uint8_t>::NewForOverwrite(length);
  base::Relaxed_Memcpy(reinterpret_cast<base::Atomic8*>(result.begin()),
                       reinterpret_cast<const base::Atomic8*>(start), length);
  return result;
}

base::UniqueArray<const uint8_t> GetAndCopyFirstArgumentAsBytes(
    const v8::FunctionCallbackInfo<v8::Value>& info, size_t max_length,
    ErrorThrower* thrower) {
  return GetAndCopyArgumentAsBytes(Utils::OpenDirectHandle(*info[0]),
                                   max_length, thrower);
}

MaybeDirectHandle<JSReceiver> ImportsAsMaybeReceiver(DirectHandle<Object> ffi) {
  if (IsUndefined(*ffi)) return {};
  return Cast<JSReceiver>(ffi);
}

// This class resolves the result of WebAssembly.compile. It just places the
// compilation result in the supplied {promise}.
class AsyncCompilationResolver : public wasm::CompilationResultResolver {
 public:
  AsyncCompilationResolver(v8::Isolate* isolate, Local<v8::Context> context,
                           Local<Promise::Resolver> promise_resolver)
      : isolate_(isolate),
        context_(isolate, context),
        promise_resolver_(isolate, promise_resolver) {
    context_.SetWeak();
    promise_resolver_.AnnotateStrongRetainer(kGlobalPromiseHandle);
  }

  void OnCompilationSucceeded(DirectHandle<WasmModuleObject> result) override {
    if (finished_) return;
    finished_ = true;
    if (context_.IsEmpty()) return;
    auto callback = reinterpret_cast<Isolate*>(isolate_)
                        ->wasm_async_resolve_promise_callback();
    CHECK(callback);
    callback(isolate_, context_.Get(isolate_), promise_resolver_.Get(isolate_),
             Utils::ToLocal(Cast<Object>(result)), WasmAsyncSuccess::kSuccess);
  }

  void OnCompilationFailed(DirectHandle<JSAny> error_reason) override {
    if (finished_) return;
    finished_ = true;
    if (context_.IsEmpty()) return;
    auto callback = reinterpret_cast<Isolate*>(isolate_)
                        ->wasm_async_resolve_promise_callback();
    CHECK(callback);
    callback(isolate_, context_.Get(isolate_), promise_resolver_.Get(isolate_),
             Utils::ToLocal(error_reason), WasmAsyncSuccess::kFail);
  }

 private:
  static constexpr char kGlobalPromiseHandle[] =
      "AsyncCompilationResolver::promise_";
  bool finished_ = false;
  v8::Isolate* isolate_;
  Global<v8::Context> context_;
  Global<Promise::Resolver> promise_resolver_;
};

constexpr char AsyncCompilationResolver::kGlobalPromiseHandle[];

// This class resolves the result of WebAssembly.instantiate(module, imports).
// It just places the instantiation result in the supplied {promise}.
class InstantiateModuleResultResolver
    : public wasm::InstantiationResultResolver {
 public:
  InstantiateModuleResultResolver(v8::Isolate* isolate,
                                  Local<v8::Context> context,
                                  Local<Promise::Resolver> promise_resolver)
      : isolate_(isolate),
        context_(isolate, context),
        promise_resolver_(isolate, promise_resolver) {
    context_.SetWeak();
    promise_resolver_.AnnotateStrongRetainer(kGlobalPromiseHandle);
  }

  void OnInstantiationSucceeded(
      DirectHandle<WasmInstanceObject> instance) override {
    if (context_.IsEmpty()) return;
    auto callback = reinterpret_cast<Isolate*>(isolate_)
                        ->wasm_async_resolve_promise_callback();
    CHECK_NOT_NULL(callback);
    callback(isolate_, context_.Get(isolate_), promise_resolver_.Get(isolate_),
             Utils::ToLocal(Cast<Object>(instance)),
             WasmAsyncSuccess::kSuccess);
  }

  void OnInstantiationFailed(DirectHandle<JSAny> error_reason) override {
    if (context_.IsEmpty()) return;
    FailInstantiation(isolate_, context_.Get(isolate_),
                      promise_resolver_.Get(isolate_),
                      Utils::ToLocal(error_reason));
  }

  static void FailInstantiation(v8::Isolate* isolate,
                                Local<v8::Context> context,
                                Local<Promise::Resolver> promise_resolver,
                                Local<v8::Value> error) {
    auto callback = reinterpret_cast<Isolate*>(isolate)
                        ->wasm_async_resolve_promise_callback();
    CHECK_NOT_NULL(callback);
    callback(isolate, context, promise_resolver, error,
             WasmAsyncSuccess::kFail);
  }

 private:
  static constexpr char kGlobalPromiseHandle[] =
      "InstantiateModuleResultResolver::promise_";
  v8::Isolate* isolate_;
  Global<v8::Context> context_;
  Global<Promise::Resolver> promise_resolver_;
};

constexpr char InstantiateModuleResultResolver::kGlobalPromiseHandle[];

// This class resolves the result of WebAssembly.instantiate(bytes, imports).
// For that it creates a new {JSObject} which contains both the provided
// {WasmModuleObject} and the resulting {WebAssemblyInstanceObject} itself.
class InstantiateBytesResultResolver
    : public wasm::InstantiationResultResolver {
 public:
  InstantiateBytesResultResolver(v8::Isolate* isolate,
                                 Local<v8::Context> context,
                                 Local<Promise::Resolver> promise_resolver,
                                 Local<v8::Value> module)
      : isolate_(isolate),
        context_(isolate, context),
        promise_resolver_(isolate, promise_resolver),
        module_(isolate, module) {
    context_.SetWeak();
    promise_resolver_.AnnotateStrongRetainer(kGlobalPromiseHandle);
    module_.AnnotateStrongRetainer(kGlobalModuleHandle);
  }

  void OnInstantiationSucceeded(
      DirectHandle<WasmInstanceObject> instance) override {
    if (context_.IsEmpty()) return;
    Isolate* i_isolate = reinterpret_cast<Isolate*>(isolate_);

    // The result is a JSObject with 2 fields which contain the
    // WasmInstanceObject and the WasmModuleObject.
    DirectHandle<JSObject> result =
        i_isolate->factory()->NewJSObject(i_isolate->object_function());
    JSObject::AddProperty(i_isolate, result, "module",
                          Utils::OpenDirectHandle(*module_.Get(isolate_)),
                          NONE);
    JSObject::AddProperty(i_isolate, result, "instance", instance, NONE);

    auto callback = i_isolate->wasm_async_resolve_promise_callback();
    CHECK(callback);
    callback(isolate_, context_.Get(isolate_), promise_resolver_.Get(isolate_),
             Utils::ToLocal(result), WasmAsyncSuccess::kSuccess);
  }

  void OnInstantiationFailed(DirectHandle<JSAny> error_reason) override {
    if (context_.IsEmpty()) return;
    auto callback = reinterpret_cast<Isolate*>(isolate_)
                        ->wasm_async_resolve_promise_callback();
    CHECK(callback);
    callback(isolate_, context_.Get(isolate_), promise_resolver_.Get(isolate_),
             Utils::ToLocal(error_reason), WasmAsyncSuccess::kFail);
  }

 private:
  static constexpr char kGlobalPromiseHandle[] =
      "InstantiateBytesResultResolver::promise_";
  static constexpr char kGlobalModuleHandle[] =
      "InstantiateBytesResultResolver::module_";
  v8::Isolate* isolate_;
  Global<v8::Context> context_;
  Global<Promise::Resolver> promise_resolver_;
  Global<v8::Value> module_;
};

constexpr char InstantiateBytesResultResolver::kGlobalPromiseHandle[];
constexpr char InstantiateBytesResultResolver::kGlobalModuleHandle[];

// This class is the {CompilationResultResolver} for
// WebAssembly.instantiate(bytes, imports). When compilation finishes,
// {AsyncInstantiate} is started on the compilation result.
class AsyncInstantiateCompileResultResolver
    : public wasm::CompilationResultResolver {
 public:
  AsyncInstantiateCompileResultResolver(
      v8::Isolate* isolate, Local<v8::Context> context,
      Local<Promise::Resolver> promise_resolver, Local<v8::Value> imports)
      : isolate_(isolate),
        context_(isolate, context),
        promise_resolver_(isolate, promise_resolver),
        imports_(isolate, imports) {
    context_.SetWeak();
    promise_resolver_.AnnotateStrongRetainer(kGlobalPromiseHandle);
    imports_.AnnotateStrongRetainer(kGlobalImportsHandle);
  }

  void OnCompilationSucceeded(DirectHandle<WasmModuleObject> result) override {
    if (finished_) return;
    finished_ = true;
    wasm::GetWasmEngine()->AsyncInstantiate(
        reinterpret_cast<Isolate*>(isolate_),
        std::make_unique<InstantiateBytesResultResolver>(
            isolate_, context_.Get(isolate_), promise_resolver_.Get(isolate_),
            Utils::ToLocal(Cast<Object>(result))),
        result,
        ImportsAsMaybeReceiver(
            Utils::OpenDirectHandle(*imports_.Get(isolate_))));
  }

  void OnCompilationFailed(DirectHandle<JSAny> error_reason) override {
    if (finished_) return;
    finished_ = true;
    if (context_.IsEmpty()) return;
    auto callback = reinterpret_cast<Isolate*>(isolate_)
                        ->wasm_async_resolve_promise_callback();
    CHECK(callback);
    callback(isolate_, context_.Get(isolate_), promise_resolver_.Get(isolate_),
             Utils::ToLocal(error_reason), WasmAsyncSuccess::kFail);
  }

 private:
  static constexpr char kGlobalPromiseHandle[] =
      "AsyncInstantiateCompileResultResolver::promise_";
  static constexpr char kGlobalImportsHandle[] =
      "AsyncInstantiateCompileResultResolver::module_";
  bool finished_ = false;
  v8::Isolate* isolate_;
  Global<v8::Context> context_;
  Global<Promise::Resolver> promise_resolver_;
  Global<v8::Value> imports_;
};

constexpr char AsyncInstantiateCompileResultResolver::kGlobalPromiseHandle[];
constexpr char AsyncInstantiateCompileResultResolver::kGlobalImportsHandle[];

// Web IDL: '[EnforceRange] unsigned long'
// https://heycam.github.io/webidl/#EnforceRange
std::optional<uint32_t> EnforceUint32(const char* argument_name,
                                      DirectHandle<Object> v, Isolate* isolate,
                                      ErrorThrower* thrower) {
  DirectHandle<Number> num;
  if (!Object::ToNumber(isolate, v).ToHandle(&num)) {
    return std::nullopt;
  }
  double double_number = Object::NumberValue(*num);
  if (!std::isfinite(double_number)) {
    thrower->TypeError("%s must be convertible to a valid number",
                       argument_name);
    return std::nullopt;
  }
  if (double_number < 0) {
    thrower->TypeError("%s must be non-negative", argument_name);
    return std::nullopt;
  }
  if (double_number > std::numeric_limits<uint32_t>::max()) {
    thrower->TypeError("%s must be in the unsigned long range", argument_name);
    return std::nullopt;
  }

  return static_cast<uint32_t>(double_number);
}

// First step of AddressValueToU64, for addrtype == "i64".
std::optional<uint64_t> EnforceBigIntUint64(const char* argument_name,
                                            DirectHandle<Object> v,
                                            Isolate* isolate,
                                            ErrorThrower* thrower) {
  DirectHandle<BigInt> bigint;
  if (!BigInt::FromObject(isolate, v).ToHandle(&bigint)) {
    return std::nullopt;
  }

  bool lossless;
  uint64_t result = bigint->AsUint64(&lossless);
  if (!lossless) {
    thrower->TypeError("%s must be in u64 range", argument_name);
    return std::nullopt;
  }

  return result;
}

// The enum values need to match "WasmCompilationMethod" in
// tools/metrics/histograms/enums.xml.
enum CompilationMethod {
  kSyncCompilation = 0,
  kAsyncCompilation = 1,
  kStreamingCompilation = 2,
  kAsyncInstantiation = 3,
  kStreamingInstantiation = 4,
};

void RecordCompilationMethod(Isolate* isolate, CompilationMethod method) {
  isolate->counters()->wasm_compilation_method()->AddSample(method);
}

// A scope object with accessors and destructor DCHECKs to be used in
// implementations of Wasm JS-API methods.
class WasmJSApiScope {
 public:
  explicit WasmJSApiScope(
      const v8::FunctionCallbackInfo<v8::Value>& callback_info,
      const char* api_name)
      : callback_info_(callback_info),
        isolate_{reinterpret_cast<Isolate*>(callback_info.GetIsolate())},
        handle_scope_{isolate_},
        thrower_{isolate_, api_name} {
    DCHECK(ValidateCallbackInfo(callback_info));
  }

  WasmJSApiScope(const WasmJSApiScope&) = delete;
  WasmJSApiScope& operator=(const WasmJSApiScope&) = delete;

  void AssertException() const {
    DCHECK(isolate_->has_exception() || thrower_.error());
  }

  const v8::FunctionCallbackInfo<v8::Value>& callback_info() {
    return callback_info_;
  }

  const char* api_name() const { return thrower_.context_name(); }

  ErrorThrower& thrower() { return thrower_; }

  std::pair<Isolate*, ErrorThrower&> isolate_and_thrower() {
    return {isolate_, thrower_};
  }

  // Accessor for all essential fields. To be decomposed into individual aliases
  // via structured binding.
  std::tuple<v8::Isolate*, Isolate*, ErrorThrower&> isolates_and_thrower() {
    return {v8_isolate(), isolate_, thrower_};
  }

 private:
  v8::Isolate* v8_isolate() const {
    return reinterpret_cast<v8::Isolate*>(isolate_);
  }

  const v8::FunctionCallbackInfo<v8::Value>& callback_info_;
  Isolate* const isolate_;
  HandleScope handle_scope_;
  ErrorThrower thrower_;
};

// Equivalent to WasmJSApiScope for Wasm constructors, which are implemented as
// CPP builtins rather than v8::FunctionCallbacks.
class WasmJSConstructorScope {
 public:
  WasmJSConstructorScope(Isolate* isolate, const char* api_name)
      : isolate_(isolate),
        handle_scope_(isolate),
        thrower_(isolate, api_name) {}

  WasmJSConstructorScope(const WasmJSConstructorScope&) = delete;
  WasmJSConstructorScope& operator=(const WasmJSConstructorScope&) = delete;

  Tagged<Object> AssertException() const {
    DCHECK(isolate_->has_exception() || thrower_.error());
    return ReadOnlyRoots(isolate_).exception();
  }

  ErrorThrower& thrower() { return thrower_; }

 private:
  Isolate* const isolate_;
  HandleScope handle_scope_;
  ErrorThrower thrower_;
};

// Check that the constructor was invoked with `new`. This is separate from
// GetDerivedMap below because argument validation (which can observably call
// user JS) must happen before reading `new.target.prototype` in GetDerivedMap.
bool CheckConstructCall(BuiltinArguments& args, ErrorThrower* thrower,
                        const char* name) {
  if (IsUndefined(*args.new_target())) {
    thrower->TypeError("%s must be invoked with 'new'", name);
    return false;
  }
  return true;
}

// Compute the initial or derived map from `target` and `new.target`.
// Requires CheckConstructCall to have succeeded first.
MaybeDirectHandle<Map> GetDerivedMap(Isolate* isolate, BuiltinArguments& args) {
  return JSFunction::GetDerivedMap(isolate, args.target(),
                                   Cast<JSReceiver>(args.new_target()));
}

// Adapter to invoke the embedder-supplied Wasm module/instance callback (or
// the deprecated v8::ExtensionCallback, which expects a
// v8::FunctionCallbackInfo) from a CPP constructor builtin.
// Returns true if the callback handled the call (by throwing an exception) and
// the caller should abort, or false if normal execution should proceed.
// TODO(crbug.com/572830550): Remove deprecated_callback once embedders have
// migrated to WasmModuleCallback / WasmInstanceCallback.
bool InvokeWasmEmbedderCallback(Isolate* isolate, WasmModuleCallback callback,
                                ExtensionCallback deprecated_callback,
                                BuiltinArguments& args) {
  DirectHandle<Object> arg0 = args.atOrUndefined(isolate, 1);
  if (callback != nullptr &&
      callback(reinterpret_cast<v8::Isolate*>(isolate), Utils::ToLocal(arg0))) {
    return true;
  }
  if (V8_LIKELY(deprecated_callback == nullptr)) {
    return false;
  }
  // Synthesize a minimal FunctionCallbackInfo with argc, isolate, and arg0.
  using FCA = FunctionCallbackArguments;
  Address values[FCA::kFirstJSArgumentIndex + 1] = {};
  values[FCA::kArgcIndex] = 1;
  values[FCA::kIsolateIndex] = reinterpret_cast<Address>(isolate);
  values[FCA::kFirstJSArgumentIndex] = (*arg0).ptr();
  return deprecated_callback(
      *reinterpret_cast<const v8::FunctionCallbackInfo<v8::Value>*>(values));
}

}  // namespace

// WebAssembly.compile(bytes, options) -> Promise
void WebAssemblyCompileImpl(const v8::FunctionCallbackInfo<v8::Value>& info) {
  WasmJSApiScope js_api_scope{info, "WebAssembly.compile()"};
  auto [isolate, i_isolate, thrower] = js_api_scope.isolates_and_thrower();
  RecordCompilationMethod(i_isolate, kAsyncCompilation);

  Local<v8::Context> context = isolate->GetCurrentContext();
  Local<Promise::Resolver> promise_resolver;
  if (!Promise::Resolver::New(context).ToLocal(&promise_resolver)) {
    return js_api_scope.AssertException();
  }
  Local<Promise> promise = promise_resolver->GetPromise();
  info.GetReturnValue().Set(promise);

  std::shared_ptr<wasm::CompilationResultResolver> resolver(
      new AsyncCompilationResolver(isolate, context, promise_resolver));

  DirectHandle<NativeContext> native_context = i_isolate->native_context();
  if (!wasm::IsWasmCodegenAllowed(i_isolate, native_context)) {
    DirectHandle<String> error =
        wasm::ErrorStringForCodegen(i_isolate, native_context);
    thrower.CompileError("%s", error->ToCString().get());
    resolver->OnCompilationFailed(thrower.Reify());
    return;
  }

  base::UniqueArray<const uint8_t> bytes =
      GetAndCopyFirstArgumentAsBytes(info, wasm::max_module_size(), &thrower);
  if (bytes.empty()) {
    resolver->OnCompilationFailed(thrower.Reify());
    return;
  }
  auto enabled_features = WasmEnabledFeatures::FromIsolate(i_isolate);
  CompileTimeImports compile_imports = WasmJs::CompileTimeImportsFromArgument(
      Utils::OpenDirectHandle(*info[1]), i_isolate, enabled_features);
  if (i_isolate->has_exception()) {
    if (i_isolate->is_execution_terminating()) return;
    DirectHandle<JSAny> exception(Cast<JSAny>(i_isolate->exception()),
                                  i_isolate);
    i_isolate->clear_exception();
    resolver->OnCompilationFailed(exception);
    return;
  }
  wasm::GetWasmEngine()->AsyncCompile(
      i_isolate, enabled_features, std::move(compile_imports),
      std::move(resolver), std::move(bytes), js_api_scope.api_name());
}

void WasmStreamingCallbackForTesting(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  WasmJSApiScope js_api_scope{info, "WebAssembly.compile()"};
  ErrorThrower& thrower = js_api_scope.thrower();

  std::shared_ptr<v8::WasmStreaming> streaming =
      v8::WasmStreaming::Unpack(info.GetIsolate(), info.DataV2());

  // We don't check the buffer length up front, to allow d8 to test that the
  // streaming decoder implementation handles overly large inputs correctly.
  size_t unlimited = std::numeric_limits<size_t>::max();
  base::UniqueArray<const uint8_t> bytes =
      GetAndCopyFirstArgumentAsBytes(info, unlimited, &thrower);
  if (bytes.empty()) {
    streaming->Abort(Utils::ToLocal(thrower.Reify()));
    return;
  }
  streaming->OnBytesReceived(bytes.begin(), bytes.size());
  streaming->Finish(WasmStreaming::ModuleCachingCallback{});
  CHECK(!thrower.error());
}

void WasmStreamingStartCallback(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  DCHECK(ValidateCallbackInfo(info));
  Isolate* isolate = reinterpret_cast<Isolate*>(info.GetIsolate());
  auto managed = Cast<CppGCManaged<WasmStreaming>>(
      Utils::OpenDirectHandle(*info.DataV2()));
  if (!managed->get()) return;
  isolate->wasm_streaming_callback()(info);
}

void WasmStreamingPromiseFailedCallback(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  DCHECK(ValidateCallbackInfo(info));
  std::shared_ptr<v8::WasmStreaming> streaming =
      v8::WasmStreaming::Unpack(info.GetIsolate(), info.DataV2());
  if (!streaming) return;
  streaming->Abort(info[0]);
}

void StartAsyncCompilationWithResolver(
    WasmJSApiScope& js_api_scope, Local<v8::Value> response_or_promise,
    DirectHandle<Object> options_arg,
    std::shared_ptr<wasm::CompilationResultResolver> resolver) {
  auto [isolate, i_isolate, thrower] = js_api_scope.isolates_and_thrower();

  Local<v8::Context> context = isolate->GetCurrentContext();
  DirectHandle<NativeContext> native_context = i_isolate->native_context();

  if (!wasm::IsWasmCodegenAllowed(i_isolate, native_context)) {
    DirectHandle<String> error =
        wasm::ErrorStringForCodegen(i_isolate, native_context);
    thrower.CompileError("%s", error->ToCString().get());
    resolver->OnCompilationFailed(thrower.Reify());
    return;
  }

  auto enabled_features = WasmEnabledFeatures::FromIsolate(i_isolate);
  CompileTimeImports compile_imports = WasmJs::CompileTimeImportsFromArgument(
      options_arg, i_isolate, enabled_features);
  if (i_isolate->has_exception()) {
    if (i_isolate->is_execution_terminating()) return;
    DirectHandle<JSAny> exception(Cast<JSAny>(i_isolate->exception()),
                                  i_isolate);
    i_isolate->clear_exception();
    resolver->OnCompilationFailed(exception);
    return;
  }

  // Allocate the streaming decoder in a CppGCManaged so we can pass it to the
  // embedder.
  std::shared_ptr<WasmStreaming> streaming = std::make_shared<WasmStreaming>(
      std::make_unique<WasmStreaming::WasmStreamingImpl>(
          i_isolate, js_api_scope.api_name(), std::move(compile_imports),
          resolver));
  DirectHandle<CppGCManaged<WasmStreaming>> data =
      CppGCManaged<WasmStreaming>::Create(i_isolate, 0, streaming);
  Local<Data> callback_data =
      Utils::Convert<Object, v8::Data>(Cast<Object>(data));

  DCHECK_NOT_NULL(i_isolate->wasm_streaming_callback());
  Local<v8::Function> compile_callback, reject_callback;
  if (!v8::Function::New(context, WasmStreamingStartCallback, callback_data, 1)
           .ToLocal(&compile_callback) ||
      !v8::Function::New(context, WasmStreamingPromiseFailedCallback,
                         callback_data, 1)
           .ToLocal(&reject_callback)) {
    return js_api_scope.AssertException();
  }

  // The parameter may be of type {Response} or of type {Promise<Response>}.
  // Treat either case of parameter as Promise.resolve(parameter)
  // as per https://www.w3.org/2001/tag/doc/promises-guide#resolve-arguments

  // Ending with:
  //    return Promise.resolve(parameter).then(compile_callback);
  Local<Promise::Resolver> input_resolver;
  if (!Promise::Resolver::New(context).ToLocal(&input_resolver) ||
      input_resolver->Resolve(context, response_or_promise).IsNothing()) {
    return js_api_scope.AssertException();
  }

  // Calling `then` on the promise can fail if the user monkey-patched stuff,
  // see https://crbug.com/374820218 / https://crbug.com/396461004.
  // If this does not fail, then the {compile_callback} will start streaming
  // compilation, which will eventually resolve the promise we set as result
  // value.
  if (input_resolver->GetPromise()
          ->Then(context, compile_callback, reject_callback)
          .IsEmpty()) {
    streaming->Abort(MaybeLocal<v8::Value>{});
    return js_api_scope.AssertException();
  }
}

// WebAssembly.compileStreaming(Response | Promise<Response>, options)
//   -> Promise<WebAssembly.Module>
void WebAssemblyCompileStreaming(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  WasmJSApiScope js_api_scope{info, "WebAssembly.compileStreaming()"};
  auto [isolate, i_isolate, thrower] = js_api_scope.isolates_and_thrower();
  RecordCompilationMethod(i_isolate, kStreamingCompilation);
  Local<v8::Context> context = isolate->GetCurrentContext();

  // Create and assign the return value of this function.
  Local<Promise::Resolver> promise_resolver;
  if (!Promise::Resolver::New(context).ToLocal(&promise_resolver)) {
    return js_api_scope.AssertException();
  }
  Local<Promise> promise = promise_resolver->GetPromise();
  info.GetReturnValue().Set(promise);

  // Prepare the CompilationResultResolver for the compilation.
  auto resolver = std::make_shared<AsyncCompilationResolver>(isolate, context,
                                                             promise_resolver);

  StartAsyncCompilationWithResolver(
      js_api_scope, info[0], Utils::OpenDirectHandle(*info[1]), resolver);
}

// WebAssembly.validate(bytes, options) -> bool
void WebAssemblyValidateImpl(const v8::FunctionCallbackInfo<v8::Value>& info) {
  WasmJSApiScope js_api_scope{info, "WebAssembly.validate()"};
  auto [isolate, thrower] = js_api_scope.isolate_and_thrower();
  v8::ReturnValue<v8::Value> return_value = info.GetReturnValue();

  // Always copy. Even if the buffer isn't shared,
  // {WasmJs::CompileTimeImportsFromArgument} could detach it.
  base::UniqueArray<const uint8_t> bytes =
      GetAndCopyFirstArgumentAsBytes(info, wasm::max_module_size(), &thrower);
  if (bytes.empty()) {
    js_api_scope.AssertException();
    // Propagate anything except wasm exceptions.
    if (!thrower.wasm_error()) return;
    // Clear wasm exceptions; return false instead.
    thrower.Reset();
    return_value.Set(false);
    return;
  }

  auto enabled_features = WasmEnabledFeatures::FromIsolate(isolate);
  CompileTimeImports compile_imports = WasmJs::CompileTimeImportsFromArgument(
      Utils::OpenDirectHandle(*info[1]), isolate, enabled_features);
  if (isolate->has_exception()) {
    if (isolate->is_execution_terminating()) return;
    isolate->clear_exception();
    return_value.Set(false);
    return;
  }

  bool validated = wasm::GetWasmEngine()->SyncValidate(
      isolate, enabled_features, std::move(compile_imports), bytes.as_span());

  return_value.Set(validated);
}

// new WebAssembly.Module(bytes, options) -> WebAssembly.Module
BUILTIN(WebAssemblyModule) {
  WasmJSConstructorScope js_api_scope{isolate, "WebAssembly.Module()"};
  ErrorThrower& thrower = js_api_scope.thrower();
  if (InvokeWasmEmbedderCallback(isolate, isolate->wasm_module_callback(),
                                 isolate->deprecated_wasm_module_callback(),
                                 args)) {
    return js_api_scope.AssertException();
  }
  RecordCompilationMethod(isolate, kSyncCompilation);

  if (!CheckConstructCall(args, &thrower, "WebAssembly.Module")) {
    return js_api_scope.AssertException();
  }

  DirectHandle<NativeContext> native_context = isolate->native_context();
  if (!wasm::IsWasmCodegenAllowed(isolate, native_context)) {
    DirectHandle<String> error =
        wasm::ErrorStringForCodegen(isolate, native_context);
    thrower.CompileError("%s", error->ToCString().get());
    return js_api_scope.AssertException();
  }

  base::UniqueArray<const uint8_t> bytes = GetAndCopyArgumentAsBytes(
      args.atOrUndefined(isolate, 1), wasm::max_module_size(), &thrower);

  if (bytes.empty()) return js_api_scope.AssertException();

  auto enabled_features = WasmEnabledFeatures::FromIsolate(isolate);
  CompileTimeImports compile_imports = WasmJs::CompileTimeImportsFromArgument(
      args.atOrUndefined(isolate, 2), isolate, enabled_features);
  if (isolate->has_exception()) {
    // TODO(14179): Does this need different error message handling?
    return js_api_scope.AssertException();
  }

  DirectHandle<Map> map;
  if (!GetDerivedMap(isolate, args).ToHandle(&map)) {
    return js_api_scope.AssertException();
  }

  DirectHandle<WasmModuleObject> module_obj;
  if (!wasm::GetWasmEngine()
           ->SyncCompile(isolate, enabled_features, std::move(compile_imports),
                         &thrower, std::move(bytes), {}, map)
           .ToHandle(&module_obj)) {
    return js_api_scope.AssertException();
  }

  return *module_obj;
}

// WebAssembly.Module.imports(module) -> Array<Import>
void WebAssemblyModuleImportsImpl(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  WasmJSApiScope js_api_scope{info, "WebAssembly.Module.imports()"};
  auto [isolate, thrower] = js_api_scope.isolate_and_thrower();

  DirectHandle<WasmModuleObject> module_object;
  if (!GetFirstArgumentAsModule(info, &thrower).ToHandle(&module_object)) {
    return js_api_scope.AssertException();
  }
  auto imports = wasm::GetImports(isolate, module_object);
  info.GetReturnValue().Set(Utils::ToLocal(imports));
}

// WebAssembly.Module.exports(module) -> Array<Export>
void WebAssemblyModuleExportsImpl(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  WasmJSApiScope js_api_scope{info, "WebAssembly.Module.exports()"};
  auto [isolate, thrower] = js_api_scope.isolate_and_thrower();

  DirectHandle<WasmModuleObject> module_object;
  if (!GetFirstArgumentAsModule(info, &thrower).ToHandle(&module_object)) {
    return js_api_scope.AssertException();
  }
  auto exports = wasm::GetExports(isolate, module_object);
  info.GetReturnValue().Set(Utils::ToLocal(exports));
}

// WebAssembly.Module.customSections(module, name) -> Array<Section>
void WebAssemblyModuleCustomSectionsImpl(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  WasmJSApiScope js_api_scope{info, "WebAssembly.Module.customSections()"};
  auto [isolate, thrower] = js_api_scope.isolate_and_thrower();

  DirectHandle<WasmModuleObject> module_object;
  if (!GetFirstArgumentAsModule(info, &thrower).ToHandle(&module_object)) {
    return js_api_scope.AssertException();
  }

  DirectHandle<Object> custom_section_name = Utils::OpenDirectHandle(*info[1]);
  if (IsUndefined(*custom_section_name)) {
    thrower.TypeError("Argument 1 is required");
    return;
  }

  DirectHandle<String> name;
  if (!Object::ToString(isolate, custom_section_name).ToHandle(&name)) {
    return js_api_scope.AssertException();
  }
  auto custom_sections =
      wasm::GetCustomSections(isolate, module_object, name, &thrower);
  if (thrower.error()) return;
  info.GetReturnValue().Set(Utils::ToLocal(custom_sections));
}

// new WebAssembly.Instance(module, imports) -> WebAssembly.Instance
BUILTIN(WebAssemblyInstance) {
  WasmJSConstructorScope js_api_scope{isolate, "WebAssembly.Instance()"};
  ErrorThrower& thrower = js_api_scope.thrower();
  RecordCompilationMethod(isolate, kAsyncInstantiation);
  isolate->CountUsage(
      v8::Isolate::UseCounterFeature::kWebAssemblyInstantiation);

  if (InvokeWasmEmbedderCallback(isolate, isolate->wasm_instance_callback(),
                                 isolate->deprecated_wasm_instance_callback(),
                                 args)) {
    return js_api_scope.AssertException();
  }

  if (!CheckConstructCall(args, &thrower, "WebAssembly.Instance")) {
    return js_api_scope.AssertException();
  }

  DirectHandle<WasmModuleObject> module_object;
  if (!GetArgumentAsModule(args.atOrUndefined(isolate, 1), &thrower)
           .ToHandle(&module_object)) {
    return js_api_scope.AssertException();
  }

  DirectHandle<Object> ffi = args.atOrUndefined(isolate, 2);
  if (!IsUndefined(*ffi) && !IsJSReceiver(*ffi)) {
    thrower.TypeError("Argument 1 must be an object");
    return js_api_scope.AssertException();
  }

  DirectHandle<Map> map;
  if (!GetDerivedMap(isolate, args).ToHandle(&map)) {
    return js_api_scope.AssertException();
  }

  DirectHandle<WasmInstanceObject> instance_obj;
  if (!wasm::GetWasmEngine()
           ->SyncInstantiate(isolate, &thrower, module_object,
                             ImportsAsMaybeReceiver(ffi), map)
           .ToHandle(&instance_obj)) {
    return js_api_scope.AssertException();
  }

  return *instance_obj;
}

// WebAssembly.instantiateStreaming(
//     Response | Promise<Response> [, imports [, options]])
//   -> Promise<ResultObject>
// (where ResultObject has a "module" and an "instance" field)
void WebAssemblyInstantiateStreaming(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  WasmJSApiScope js_api_scope{info, "WebAssembly.instantiateStreaming()"};
  auto [isolate, i_isolate, thrower] = js_api_scope.isolates_and_thrower();
  RecordCompilationMethod(i_isolate, kStreamingInstantiation);
  i_isolate->CountUsage(
      v8::Isolate::UseCounterFeature::kWebAssemblyInstantiation);

  Local<v8::Context> context = isolate->GetCurrentContext();

  // Create and assign the return value of this function.
  Local<Promise::Resolver> result_resolver;
  if (!Promise::Resolver::New(context).ToLocal(&result_resolver)) {
    return js_api_scope.AssertException();
  }
  Local<Promise> promise = result_resolver->GetPromise();
  info.GetReturnValue().Set(promise);

  // If info.Length < 2, this will be undefined - see FunctionCallbackInfo.
  DirectHandle<Object> ffi = Utils::OpenDirectHandle(*info[1]);

  if (!IsUndefined(*ffi) && !IsJSReceiver(*ffi)) {
    thrower.TypeError("Argument 1 must be an object");
    InstantiateModuleResultResolver::FailInstantiation(
        isolate, context, result_resolver, Utils::ToLocal(thrower.Reify()));
    return;
  }

  auto compilation_resolver =
      std::make_shared<AsyncInstantiateCompileResultResolver>(
          isolate, context, result_resolver, info[1]);
  StartAsyncCompilationWithResolver(js_api_scope, info[0],
                                    Utils::OpenDirectHandle(*info[2]),
                                    std::move(compilation_resolver));
}

template <typename... Args>
Local<v8::FixedArray> MakeInternalFixedArray(Isolate* isolate, Args... args) {
  DirectHandle<FixedArray> fixed_array =
      isolate->factory()->NewFixedArray(sizeof...(args));
  uint32_t index = 0;
  auto add = [&](Local<Data> obj) {
    fixed_array->set(index++, *Utils::OpenDirectHandle(*obj));
  };
  (add(args), ...);
  DCHECK_EQ(fixed_array->ulength().value(), index);
  return Utils::FixedArrayToLocal(fixed_array);
}

// WebAssembly.instantiate(module, imports) -> WebAssembly.Instance
// WebAssembly.instantiate(bytes, imports, options) ->
//     {module: WebAssembly.Module, instance: WebAssembly.Instance}
void WebAssemblyInstantiateImpl(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  WasmJSApiScope js_api_scope{info, "WebAssembly.instantiate()"};
  auto [isolate, i_isolate, thrower] = js_api_scope.isolates_and_thrower();
  i_isolate->CountUsage(
      v8::Isolate::UseCounterFeature::kWebAssemblyInstantiation);

  Local<v8::Context> context = isolate->GetCurrentContext();

  Local<Promise::Resolver> instantiation_promise_resolver;
  if (!Promise::Resolver::New(context).ToLocal(
          &instantiation_promise_resolver)) {
    return js_api_scope.AssertException();
  }
  Local<Promise> instantiation_promise =
      instantiation_promise_resolver->GetPromise();
  info.GetReturnValue().Set(instantiation_promise);

  DirectHandle<Object> first_arg = Utils::OpenDirectHandle(*info[0]);
  if (!IsJSObject(*first_arg)) {
    thrower.TypeError(
        "Argument 0 must be a buffer source or a WebAssembly.Module object");
    InstantiateModuleResultResolver::FailInstantiation(
        isolate, context, instantiation_promise_resolver,
        Utils::ToLocal(thrower.Reify()));
    return;
  }

  // If info.Length < 2, this will be undefined - see FunctionCallbackInfo.
  DirectHandle<Object> ffi = Utils::OpenDirectHandle(*info[1]);

  if (!IsUndefined(*ffi) && !IsJSReceiver(*ffi)) {
    thrower.TypeError("Argument 1 must be an object");
    InstantiateModuleResultResolver::FailInstantiation(
        isolate, context, instantiation_promise_resolver,
        Utils::ToLocal(thrower.Reify()));
    return;
  }

  if (IsWasmModuleObject(*first_arg)) {
    DirectHandle<WasmModuleObject> module_obj =
        Cast<WasmModuleObject>(first_arg);

    std::unique_ptr<wasm::InstantiationResultResolver> resolver(
        new InstantiateModuleResultResolver(isolate, context,
                                            instantiation_promise_resolver));
    wasm::GetWasmEngine()->AsyncInstantiate(i_isolate, std::move(resolver),
                                            module_obj,
                                            ImportsAsMaybeReceiver(ffi));
    return;
  }

  base::UniqueArray<const uint8_t> bytes =
      GetAndCopyFirstArgumentAsBytes(info, wasm::max_module_size(), &thrower);
  if (bytes.empty()) {
    InstantiateModuleResultResolver::FailInstantiation(
        isolate, context, instantiation_promise_resolver,
        Utils::ToLocal(thrower.Reify()));
    return;
  }

  // The first parameter is a buffer source. We start a compilation, and then
  // chain the instantiation to that.
  Local<Promise::Resolver> compilation_promise_resolver;
  if (!Promise::Resolver::New(context).ToLocal(&compilation_promise_resolver)) {
    return js_api_scope.AssertException();
  }
  Local<Promise> compilation_promise =
      compilation_promise_resolver->GetPromise();

  auto InstantiateThenCallback =
      [](const v8::FunctionCallbackInfo<v8::Value>& info) {
        v8::Isolate* isolate2 = info.GetIsolate();
        Isolate* i_isolate2 = reinterpret_cast<Isolate*>(isolate2);
        HandleScope scope(i_isolate2);
        Local<v8::FixedArray> data = info.DataV2().As<v8::FixedArray>();
        DCHECK_EQ(3, data->Length());
        Local<v8::Context> context = data->Get(0).As<v8::Context>();
        Local<Promise::Resolver> promise_resolver =
            data->Get(1).As<v8::Value>().As<Promise::Resolver>();
        Local<v8::Value> imports = data->Get(2).As<v8::Value>();

        DirectHandle<WasmModuleObject> module_obj;
        if (!TryCast(Utils::OpenDirectHandle(*info[0]), &module_obj)) {
          auto callback = i_isolate2->wasm_async_resolve_promise_callback();
          CHECK(callback);
          DirectHandle<JSObject> error = i_isolate2->factory()->NewError(
              i_isolate2->type_error_function(),
              v8_str(i_isolate2,
                     "Illegal call to WebAssembly.instantiate callback: "
                     "Argument is not a WasmModuleObject"));
          callback(isolate2, context, promise_resolver, Utils::ToLocal(error),
                   WasmAsyncSuccess::kFail);
          return;
        }

        wasm::GetWasmEngine()->AsyncInstantiate(
            i_isolate2,
            std::make_unique<InstantiateBytesResultResolver>(
                isolate2, context, promise_resolver, info[0]),
            module_obj,
            ImportsAsMaybeReceiver(Utils::OpenDirectHandle(*imports)));
      };

  Local<v8::FixedArray> then_data = MakeInternalFixedArray(
      i_isolate, context, instantiation_promise_resolver, info[1]);
  Local<v8::Function> then_callback;
  if (!v8::Function::New(context, InstantiateThenCallback, then_data, 1)
           .ToLocal(&then_callback)) {
    return js_api_scope.AssertException();
  }

  auto RejectInstantiation =
      [](const v8::FunctionCallbackInfo<v8::Value>& info) {
        v8::HandleScope scope(info.GetIsolate());
        Local<Promise::Resolver> instantiation_promise_resolver =
            info.DataV2().As<v8::Value>().As<Promise::Resolver>();

        instantiation_promise_resolver
            ->Reject(info.GetIsolate()->GetCurrentContext(), info[0])
            .ToChecked();
      };

  Local<v8::Function> reject_callback;
  if (!v8::Function::New(context, RejectInstantiation,
                         instantiation_promise_resolver, 1)
           .ToLocal(&reject_callback)) {
    return js_api_scope.AssertException();
  }

  if (compilation_promise->Then(context, then_callback, reject_callback)
          .IsEmpty()) {
    return js_api_scope.AssertException();
  }

  std::shared_ptr<wasm::CompilationResultResolver> compilation_resolver(
      new AsyncCompilationResolver(isolate, context,
                                   compilation_promise_resolver));

  // The first parameter is a buffer source, we have to check if we are allowed
  // to compile it.
  DirectHandle<NativeContext> native_context = i_isolate->native_context();
  if (!wasm::IsWasmCodegenAllowed(i_isolate, native_context)) {
    DirectHandle<String> error =
        wasm::ErrorStringForCodegen(i_isolate, native_context);
    thrower.CompileError("%s", error->ToCString().get());
    compilation_resolver->OnCompilationFailed(thrower.Reify());
    return;
  }

  auto enabled_features = WasmEnabledFeatures::FromIsolate(i_isolate);
  CompileTimeImports compile_imports = WasmJs::CompileTimeImportsFromArgument(
      Utils::OpenDirectHandle(*info[2]), i_isolate, enabled_features);
  if (i_isolate->has_exception()) {
    if (i_isolate->is_execution_terminating()) return;
    DirectHandle<JSAny> exception(Cast<JSAny>(i_isolate->exception()),
                                  i_isolate);
    i_isolate->clear_exception();
    compilation_resolver->OnCompilationFailed(exception);
    return;
  }

  wasm::GetWasmEngine()->AsyncCompile(
      i_isolate, enabled_features, std::move(compile_imports),
      std::move(compilation_resolver), std::move(bytes),
      js_api_scope.api_name());
}

namespace {
// {AddressValueToU64} as defined in the memory64 js-api spec.
// Returns std::nullopt on error (exception or error set in the thrower), and
// the address value otherwise.
std::optional<uint64_t> AddressValueToU64(Isolate* isolate,
                                          ErrorThrower* thrower,
                                          DirectHandle<Object> value,
                                          const char* property_name,
                                          AddressType address_type) {
  switch (address_type) {
    case AddressType::kI32:
      return EnforceUint32(property_name, value, isolate, thrower);
    case AddressType::kI64:
      return EnforceBigIntUint64(property_name, value, isolate, thrower);
  }
  // The enum value is coming from inside the sandbox and while the switch is
  // exhaustive, it's not guaranteed that value is one of the declared values.
  UNREACHABLE();
}

// {AddressValueToU64} plus additional bounds checks.
std::optional<uint64_t> AddressValueToBoundedU64(
    Isolate* isolate, ErrorThrower* thrower, DirectHandle<Object> value,
    const char* property_name, AddressType address_type, uint64_t lower_bound,
    uint64_t upper_bound) {
  std::optional<uint64_t> maybe_address_value =
      AddressValueToU64(isolate, thrower, value, property_name, address_type);
  if (!maybe_address_value) return std::nullopt;
  uint64_t address_value = *maybe_address_value;

  if (address_value < lower_bound) {
    thrower->RangeError("%s: value %" PRIu64
                        " is below the lower bound %" PRIx64,
                        property_name, address_value, lower_bound);
    return std::nullopt;
  }

  if (address_value > upper_bound) {
    thrower->RangeError("%s: value %" PRIu64
                        " is above the upper bound %" PRIu64,
                        property_name, address_value, upper_bound);
    return std::nullopt;
  }

  return address_value;
}

// Returns std::nullopt on error (exception or error set in the thrower).
// The inner optional is std::nullopt if the property did not exist, and the
// address value otherwise.
std::optional<std::optional<uint64_t>> GetOptionalAddressValue(
    Isolate* isolate, ErrorThrower* thrower,
    DirectHandle<JSReceiver> descriptor, const char* property,
    const char* property_error_name, AddressType address_type,
    int64_t lower_bound, uint64_t upper_bound) {
  DirectHandle<Object> value;
  if (!JSReceiver::GetProperty(isolate, descriptor, property)
           .ToHandle(&value)) {
    return std::nullopt;
  }

  // Web IDL: dictionary presence
  // https://heycam.github.io/webidl/#dfn-present
  if (IsUndefined(*value)) {
    // No exception, but no value either.
    return std::optional<uint64_t>{};
  }

  std::optional<uint64_t> maybe_address_value =
      AddressValueToBoundedU64(isolate, thrower, value, property_error_name,
                               address_type, lower_bound, upper_bound);
  if (!maybe_address_value) return std::nullopt;
  return *maybe_address_value;
}

// Fetch 'initial' or 'minimum' property from `descriptor`. If both are
// provided, a TypeError is thrown.
// Returns std::nullopt on error (exception or error set in the thrower).
std::optional<uint64_t> GetInitialOrMinimumProperty(
    Isolate* isolate, ErrorThrower* thrower,
    DirectHandle<JSReceiver> descriptor, AddressType address_type,
    uint64_t upper_bound) {
  auto maybe_maybe_initial = GetOptionalAddressValue(
      isolate, thrower, descriptor, "initial", "Property 'initial'",
      address_type, 0, upper_bound);
  if (!maybe_maybe_initial) return std::nullopt;
  std::optional<uint64_t> maybe_initial = *maybe_maybe_initial;

  if (!maybe_initial) {
    thrower->TypeError("Property 'initial' is required");
    return std::nullopt;
  }
  return *maybe_initial;
}

DirectHandle<Object> AddressValueFromUnsigned(Isolate* isolate,
                                              wasm::AddressType type,
                                              uint32_t value) {
  return type == wasm::AddressType::kI64
             ? Cast<Object>(BigInt::FromUint64(isolate, value))
             : isolate->factory()->NewNumberFromUint(value);
}

DirectHandle<HeapObject> DefaultReferenceValue(Isolate* isolate,
                                               wasm::ValueType type) {
  DCHECK(type.is_ref());
  // Use undefined for JS type (externref) but null for wasm types as wasm does
  // not know undefined.
  if (type.is_reference_to(wasm::GenericKind::kExtern)) {
    return isolate->factory()->undefined_value();
  } else if (!type.use_wasm_null()) {
    return isolate->factory()->null_value();
  }
  return isolate->factory()->wasm_null();
}

// Read the address type from a Memory or Table descriptor.
std::optional<AddressType> GetAddressType(Isolate* isolate,
                                          DirectHandle<JSReceiver> descriptor,
                                          ErrorThrower* thrower) {
  DirectHandle<Object> address_value;
  if (!JSReceiver::GetProperty(isolate, descriptor, "address")
           .ToHandle(&address_value)) {
    return std::nullopt;
  }

  if (IsUndefined(*address_value)) return AddressType::kI32;

  DirectHandle<String> address;
  if (!Object::ToString(isolate, address_value).ToHandle(&address)) {
    return std::nullopt;
  }

  if (address->IsEqualTo("i64")) return AddressType::kI64;
  if (address->IsEqualTo("i32")) return AddressType::kI32;

  thrower->TypeError("Unknown address type '%s'; pass 'i32' or 'i64'",
                     address->ToCString().get());
  return std::nullopt;
}
}  // namespace

// new WebAssembly.Table(descriptor) -> WebAssembly.Table
BUILTIN(WebAssemblyTable) {
  WasmJSConstructorScope js_api_scope{isolate, "WebAssembly.Table()"};
  ErrorThrower& thrower = js_api_scope.thrower();

  if (!CheckConstructCall(args, &thrower, "WebAssembly.Table")) {
    return js_api_scope.AssertException();
  }
  Handle<JSReceiver> descriptor;
  if (!TryCast<JSReceiver>(args.atOrUndefined(isolate, 1), &descriptor)) {
    thrower.TypeError("Argument 0 must be a table descriptor");
    return js_api_scope.AssertException();
  }
  wasm::ValueType type;
  // Parse the 'element' property of the `descriptor`.
  {
    DirectHandle<Object> value;
    if (!JSReceiver::GetProperty(isolate, descriptor, "element")
             .ToHandle(&value)) {
      return js_api_scope.AssertException();
    }
    DirectHandle<String> string;
    if (!Object::ToString(isolate, value).ToHandle(&string)) {
      return js_api_scope.AssertException();
    }
    auto enabled_features = WasmEnabledFeatures::FromIsolate(isolate);
    // The JS api uses 'anyfunc' instead of 'funcref'.
    if (string->IsEqualTo("anyfunc")) {
      type = wasm::kWasmFuncRef;
    } else if (string->IsEqualTo("externref")) {
      type = wasm::kWasmExternRef;
    } else if (enabled_features.has_stringref() &&
               string->IsEqualTo("stringref")) {
      type = wasm::kWasmStringRef;
    } else if (string->IsEqualTo("anyref")) {
      type = wasm::kWasmAnyRef;
    } else if (string->IsEqualTo("eqref")) {
      type = wasm::kWasmEqRef;
    } else if (string->IsEqualTo("structref")) {
      type = wasm::kWasmStructRef;
    } else if (string->IsEqualTo("arrayref")) {
      type = wasm::kWasmArrayRef;
    } else if (string->IsEqualTo("i31ref")) {
      type = wasm::kWasmI31Ref;
    } else {
      thrower.TypeError(
          "Descriptor property 'element' must be a WebAssembly reference type");
      return js_api_scope.AssertException();
    }
    // TODO(14616): Support shared types.
  }

  // Parse the 'address' property of the `descriptor`.
  std::optional<AddressType> maybe_address_type =
      GetAddressType(isolate, descriptor, &thrower);
  if (!maybe_address_type) return js_api_scope.AssertException();
  AddressType address_type = *maybe_address_type;

  // Parse the 'initial' or 'minimum' property of the `descriptor`.
  std::optional<uint64_t> maybe_initial =
      GetInitialOrMinimumProperty(isolate, &thrower, descriptor, address_type,
                                  wasm::max_table_init_entries());
  if (!maybe_initial) return js_api_scope.AssertException();
  static_assert(wasm::kV8MaxWasmTableInitEntries <= kMaxUInt32);
  uint32_t initial = static_cast<uint32_t>(*maybe_initial);

  // Parse the 'maximum' property of the `descriptor`.
  uint64_t kNoMaximum = kMaxUInt64;
  auto maybe_maybe_maximum = GetOptionalAddressValue(
      isolate, &thrower, descriptor, "maximum", "Property 'maximum'",
      address_type, initial, kNoMaximum);
  if (!maybe_maybe_maximum) return js_api_scope.AssertException();
  std::optional<uint64_t> maybe_maximum = *maybe_maybe_maximum;

  DCHECK(!type.has_index());  // The JS API can't express type indices.
  wasm::CanonicalValueType canonical_type{type};
  DirectHandle<Object> initial_value;

  DirectHandle<Object> element = args.atOrUndefined(isolate, 2);
  if (initial > 0 && args.length() >= 3 && !IsUndefined(*element)) {
    const char* error_message;
    if (!wasm::JSToWasmObject(isolate, element, canonical_type, &error_message)
             .ToHandle(&initial_value)) {
      thrower.TypeError(
          "Argument 2 must be undefined or a value of type compatible "
          "with the type of the new table: %s.",
          error_message);
      return js_api_scope.AssertException();
    }
  } else {
    initial_value = DefaultReferenceValue(isolate, type);
    if (initial > 0 && type.is_abstract_ref()) {
      switch (type.generic_kind()) {
        case wasm::GenericKind::kString:
          thrower.TypeError(
              "Missing initial value when creating stringref table");
          return js_api_scope.AssertException();
        case wasm::GenericKind::kStringViewWtf8:
        case wasm::GenericKind::kStringViewWtf16:
        case wasm::GenericKind::kStringViewIter:
          thrower.TypeError("%s has no JS representation", type.name().c_str());
          return js_api_scope.AssertException();
        default:
          break;
      }
    }
  }

  DirectHandle<Map> map;
  if (!GetDerivedMap(isolate, args).ToHandle(&map)) {
    return js_api_scope.AssertException();
  }

  DirectHandle<WasmDispatchTable> dispatch_table;
  DirectHandle<WasmTableObject> table_obj = WasmTableObject::New(
      isolate, DirectHandle<WasmTrustedInstanceData>(), type, canonical_type,
      initial, maybe_maximum.has_value(),
      maybe_maximum.value_or(0) /* note: unused if previous param is false */,
      initial_value, address_type, &dispatch_table, map);

  return *table_obj;
}

// new WebAssembly.Memory(descriptor) -> WebAssembly.Memory
BUILTIN(WebAssemblyMemory) {
  WasmJSConstructorScope js_api_scope{isolate, "WebAssembly.Memory()"};
  ErrorThrower& thrower = js_api_scope.thrower();

  if (!CheckConstructCall(args, &thrower, "WebAssembly.Memory")) {
    return js_api_scope.AssertException();
  }
  Handle<JSReceiver> descriptor;
  if (!TryCast<JSReceiver>(args.atOrUndefined(isolate, 1), &descriptor)) {
    thrower.TypeError("Argument 0 must be a memory descriptor");
    return js_api_scope.AssertException();
  }

  // Parse the 'address' property of the `descriptor`.
  std::optional<AddressType> maybe_address_type =
      GetAddressType(isolate, descriptor, &thrower);
  if (!maybe_address_type) return js_api_scope.AssertException();
  AddressType address_type = *maybe_address_type;
  uint64_t max_supported_pages = address_type == AddressType::kI64
                                     ? wasm::kSpecMaxMemory64Pages
                                     : wasm::kSpecMaxMemory32Pages;
  // {max_supported_pages} will actually be in integer range. That's the type
  // {WasmMemoryObject::New} uses.
  static_assert(wasm::kSpecMaxMemory32Pages < kMaxInt);
  static_assert(wasm::kSpecMaxMemory64Pages < kMaxInt);

  // Parse the 'initial' or 'minimum' property of the `descriptor`.
  std::optional<uint64_t> maybe_initial = GetInitialOrMinimumProperty(
      isolate, &thrower, descriptor, address_type, max_supported_pages);
  if (!maybe_initial) {
    return js_api_scope.AssertException();
  }
  uint64_t initial = *maybe_initial;

  // Parse the 'maximum' property of the `descriptor`.
  auto maybe_maybe_maximum = GetOptionalAddressValue(
      isolate, &thrower, descriptor, "maximum", "Property 'maximum'",
      address_type, initial, max_supported_pages);
  if (!maybe_maybe_maximum) {
    return js_api_scope.AssertException();
  }
  std::optional<uint64_t> maybe_maximum = *maybe_maybe_maximum;

  // Parse the 'shared' property of the `descriptor`.
  DirectHandle<Object> value;
  if (!JSReceiver::GetProperty(isolate, descriptor, "shared")
           .ToHandle(&value)) {
    return js_api_scope.AssertException();
  }

  SharedFlag shared = SharedFlag(Object::BooleanValue(*value, isolate));

  // Throw TypeError if shared is true, and the descriptor has no "maximum".
  if (shared && !maybe_maximum.has_value()) {
    thrower.TypeError("If shared is true, maximum property should be defined.");
    return js_api_scope.AssertException();
  }

  DirectHandle<Map> map;
  if (!GetDerivedMap(isolate, args).ToHandle(&map)) {
    return js_api_scope.AssertException();
  }

  DirectHandle<WasmMemoryObject> memory_obj;
  if (!WasmMemoryObject::New(isolate, static_cast<int>(initial),
                             maybe_maximum ? static_cast<int>(*maybe_maximum)
                                           : WasmMemoryObject::kNoMaximum,
                             shared, address_type, map)
           .ToHandle(&memory_obj)) {
    thrower.RangeError("could not allocate memory");
    return js_api_scope.AssertException();
  }

  // The JSArrayBuffer will be allocated lazily later.
  DCHECK(IsUndefined(memory_obj->array_buffer()));

  return *memory_obj;
}

// new WebAssembly.MemoryMapDescriptor(size) -> WebAssembly.MemoryMapDescriptor
void WebAssemblyMemoryMapDescriptorImpl(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  CHECK(v8_flags.wasm_memory_control);
  WasmJSApiScope js_api_scope{info, "WebAssembly.MemoryMapDescriptor()"};
  auto [isolate, i_isolate, thrower] = js_api_scope.isolates_and_thrower();
  if (!info.IsConstructCall()) {
    thrower.TypeError(
        "WebAssembly.MemoryMapDescriptor must be invoked with 'new'");
    return js_api_scope.AssertException();
  }

  std::optional<uint32_t> size = EnforceUint32(
      "size", Utils::OpenDirectHandle(*info[0]), i_isolate, &thrower);

  if (!size.has_value()) {
    return js_api_scope.AssertException();
  }

  v8::Local<v8::Object> descriptor_obj;
  if (!wasm::WasmMemoryMapDescriptor::NewFromAnonymous(isolate, size.value(),
                                                       info.This())
           .ToLocal(&descriptor_obj)) {
    thrower.RuntimeError("Failed to create a MemoryMapDescriptor");
    return js_api_scope.AssertException();
  }

  info.GetReturnValue().Set(descriptor_obj);
}

// Determines the type encoded in a value type property (e.g. type reflection).
// Returns std::nullopt if there was an exception, or the decoded type upon
// success (set to {wasm::kWasmVoid} in case the type could not be properly
// recognized).
std::optional<wasm::ValueType> GetValueType(
    Isolate* isolate, MaybeDirectHandle<Object> maybe,
    WasmEnabledFeatures enabled_features) {
  DirectHandle<Object> value;
  if (!maybe.ToHandle(&value)) return std::nullopt;
  DirectHandle<String> string;
  if (!Object::ToString(isolate, value).ToHandle(&string)) {
    return std::nullopt;
  }
  if (string->IsEqualTo("i32")) {
    return wasm::kWasmI32;
  } else if (string->IsEqualTo("f32")) {
    return wasm::kWasmF32;
  } else if (string->IsEqualTo("i64")) {
    return wasm::kWasmI64;
  } else if (string->IsEqualTo("f64")) {
    return wasm::kWasmF64;
  } else if (string->IsEqualTo("v128")) {
    return wasm::kWasmS128;
  } else if (string->IsEqualTo("externref")) {
    return wasm::kWasmExternRef;
  } else if (string->IsEqualTo("anyfunc")) {
    // The JS api spec uses 'anyfunc' instead of 'funcref'.
    return wasm::kWasmFuncRef;
  } else if (string->IsEqualTo("eqref")) {
    return wasm::kWasmEqRef;
  } else if (enabled_features.has_stringref() &&
             string->IsEqualTo("stringref")) {
    return wasm::kWasmStringRef;
  } else if (string->IsEqualTo("anyref")) {
    return wasm::kWasmAnyRef;
  } else if (string->IsEqualTo("structref")) {
    return wasm::kWasmStructRef;
  } else if (string->IsEqualTo("arrayref")) {
    return wasm::kWasmArrayRef;
  } else if (string->IsEqualTo("i31ref")) {
    return wasm::kWasmI31Ref;
  } else if (string->IsEqualTo("exnref")) {
    return wasm::kWasmExnRef;
  }
  // Unrecognized type.
  return wasm::kWasmVoid;
}

namespace {

bool ToI32(Isolate* isolate, DirectHandle<Object> value, int32_t* i32_value) {
  if (!IsUndefined(*value)) {
    DirectHandle<Object> int32_value;
    if (!Object::ToInt32(isolate, value).ToHandle(&int32_value)) return false;
    *i32_value = NumberToInt32(*int32_value);
  }
  return true;
}

bool ToI64(Isolate* isolate, DirectHandle<Object> value, int64_t* i64_value) {
  if (!IsUndefined(*value)) {
    DirectHandle<BigInt> bigint_value;
    if (!BigInt::FromObject(isolate, value).ToHandle(&bigint_value)) {
      return false;
    }
    *i64_value = bigint_value->AsInt64();
  }
  return true;
}

bool ToF32(Isolate* isolate, DirectHandle<Object> value, float* f32_value) {
  if (!IsUndefined(*value)) {
    DirectHandle<Number> number_value;
    if (!Object::ToNumber(isolate, value).ToHandle(&number_value)) return false;
    *f32_value = DoubleToFloat32(Object::NumberValue(*number_value));
  }
  return true;
}

bool ToF64(Isolate* isolate, DirectHandle<Object> value, double* f64_value) {
  if (!IsUndefined(*value)) {
    DirectHandle<Number> number_value;
    if (!Object::ToNumber(isolate, value).ToHandle(&number_value)) return false;
    *f64_value = Object::NumberValue(*number_value);
  }
  return true;
}
}  // namespace

// new WebAssembly.Global(descriptor[, value])
BUILTIN(WebAssemblyGlobal) {
  WasmJSConstructorScope js_api_scope{isolate, "WebAssembly.Global()"};
  ErrorThrower& thrower = js_api_scope.thrower();

  if (!CheckConstructCall(args, &thrower, "WebAssembly.Global")) {
    return js_api_scope.AssertException();
  }
  Handle<JSReceiver> descriptor;
  if (!TryCast<JSReceiver>(args.atOrUndefined(isolate, 1), &descriptor)) {
    thrower.TypeError("Argument 0 must be a global descriptor");
    return js_api_scope.AssertException();
  }
  auto enabled_features = WasmEnabledFeatures::FromIsolate(isolate);

  // The descriptor's 'mutable'.
  bool is_mutable;
  {
    DirectHandle<Object> value;
    if (!JSReceiver::GetProperty(isolate, descriptor, "mutable")
             .ToHandle(&value)) {
      return js_api_scope.AssertException();
    }
    is_mutable = Object::BooleanValue(*value, isolate);
    if (is_mutable) {
      isolate->CountUsage(v8::Isolate::UseCounterFeature::kWasmMutableGlobals);
    }
  }

  // The descriptor's type, called 'value'. It is called 'value' because this
  // descriptor is planned to be reused as the global's type for reflection,
  // so calling it 'type' is redundant.
  wasm::ValueType type;
  {
    MaybeDirectHandle<Object> maybe =
        JSReceiver::GetProperty(isolate, descriptor, "value");
    std::optional<wasm::ValueType> maybe_type =
        GetValueType(isolate, maybe, enabled_features);
    if (!maybe_type) return js_api_scope.AssertException();
    type = *maybe_type;
    if (type == wasm::kWasmVoid) {
      thrower.TypeError(
          "Descriptor property 'value' must be a WebAssembly type");
      return js_api_scope.AssertException();
    }
  }

  // Convert value to a WebAssembly value, the default value is 0.
  DirectHandle<Object> value_arg = args.atOrUndefined(isolate, 2);
  int32_t i32_value = 0;
  int64_t i64_value = 0;
  float f32_value = 0;
  double f64_value = 0;
  DirectHandle<Object> ref_value;
  switch (type.kind()) {
    case wasm::kI32:
      if (!ToI32(isolate, value_arg, &i32_value)) {
        return js_api_scope.AssertException();
      }
      break;
    case wasm::kI64:
      if (!ToI64(isolate, value_arg, &i64_value)) {
        return js_api_scope.AssertException();
      }
      break;
    case wasm::kF32:
      if (!ToF32(isolate, value_arg, &f32_value)) {
        return js_api_scope.AssertException();
      }
      break;
    case wasm::kF64:
      if (!ToF64(isolate, value_arg, &f64_value)) {
        return js_api_scope.AssertException();
      }
      break;
    case wasm::kRef:
      if (args.length() < 3) {
        thrower.TypeError("Non-defaultable global needs initial value");
        return js_api_scope.AssertException();
      }
      [[fallthrough]];
    case wasm::kRefNull:
      // We need the wasm default value {null} over {undefined}.
      if (args.length() < 3) {
        ref_value = DefaultReferenceValue(isolate, type);
      } else {
        const char* error_message;
        // While the JS API generally allows indexed types, it currently has
        // no way to specify such types in `new WebAssembly.Global(...)`.
        // TODO(14034): Fix this if that changes.
        DCHECK(!type.has_index());
        wasm::CanonicalValueType canonical_type{type};
        if (!wasm::JSToWasmObject(isolate, value_arg, canonical_type,
                                  &error_message)
                 .ToHandle(&ref_value)) {
          thrower.TypeError("%s", error_message);
          return js_api_scope.AssertException();
        }
      }
      break;
    case wasm::kS128:
      thrower.TypeError(
          "A global of type 'v128' cannot be created in JavaScript");
      return js_api_scope.AssertException();
    case wasm::kI8:
    case wasm::kI16:
    case wasm::kF16:
    case wasm::kVoid:
    case wasm::kTop:
    case wasm::kBottom:
      UNREACHABLE();
  }

  DirectHandle<Map> map;
  if (!GetDerivedMap(isolate, args).ToHandle(&map)) {
    return js_api_scope.AssertException();
  }

  const uint32_t offset = 0;
  MaybeDirectHandle<WasmGlobalObject> maybe_global_obj =
      WasmGlobalObject::New(isolate, DirectHandle<WasmTrustedInstanceData>(),
                            MaybeDirectHandle<WasmGlobalObject::BufferType>(),
                            type, offset, is_mutable, map);

  DirectHandle<WasmGlobalObject> global_obj;
  if (!maybe_global_obj.ToHandle(&global_obj)) {
    return js_api_scope.AssertException();
  }

  switch (type.kind()) {
    case wasm::kI32:
      global_obj->SetI32(i32_value);
      break;
    case wasm::kI64:
      global_obj->SetI64(i64_value);
      break;
    case wasm::kF32:
      global_obj->SetF32(f32_value);
      break;
    case wasm::kF64:
      global_obj->SetF64(f64_value);
      break;
    case wasm::kRef:
    case wasm::kRefNull:
      global_obj->SetRef(ref_value);
      break;
    case wasm::kS128:
    case wasm::kI8:
    case wasm::kI16:
    case wasm::kF16:
    case wasm::kVoid:
    case wasm::kTop:
    case wasm::kBottom:
      UNREACHABLE();
  }

  return *global_obj;
}

namespace {

uint32_t GetIterableLength(Isolate* isolate,
                           DirectHandle<JSReceiver> iterable) {
  DirectHandle<Object> property;
  if (!JSReceiver::GetProperty(isolate, iterable,
                               isolate->factory()->length_string())
           .ToHandle(&property)) {
    return kMaxUInt32;
  }
  uint32_t length;
  if (Object::ToArrayIndex(*property, &length)) return length;
  if (IsSmi(*property)) return kMaxUInt32;
  DirectHandle<String> length_str;
  if (!Object::ToString(isolate, property).ToHandle(&length_str)) {
    return kMaxUInt32;
  }
  if (!length_str->AsArrayIndex(&length)) return kMaxUInt32;
  DCHECK_NE(kMaxUInt32, length);
  return length;
}

}  // namespace

// WebAssembly.Tag
BUILTIN(WebAssemblyTag) {
  WasmJSConstructorScope js_api_scope{isolate, "WebAssembly.Tag()"};
  ErrorThrower& thrower = js_api_scope.thrower();

  if (!CheckConstructCall(args, &thrower, "WebAssembly.Tag")) {
    return js_api_scope.AssertException();
  }
  Handle<JSReceiver> event_type;
  if (!TryCast<JSReceiver>(args.atOrUndefined(isolate, 1), &event_type)) {
    thrower.TypeError("Argument 0 must be a tag type");
    return js_api_scope.AssertException();
  }

  auto enabled_features = WasmEnabledFeatures::FromIsolate(isolate);

  // Load the 'parameters' property of the event type.
  DirectHandle<Object> parameters_value;
  if (!JSReceiver::GetProperty(isolate, event_type, "parameters")
           .ToHandle(&parameters_value)) {
    return js_api_scope.AssertException();
  }
  DirectHandle<JSReceiver> parameters;
  if (!TryCast<JSReceiver>(parameters_value, &parameters)) {
    thrower.TypeError("Argument 0 must be a tag type with 'parameters'");
    return js_api_scope.AssertException();
  }
  uint32_t parameters_len = GetIterableLength(isolate, parameters);
  if (parameters_len == kMaxUInt32) {
    thrower.TypeError("Argument 0 contains parameters without 'length'");
    return js_api_scope.AssertException();
  }
  if (parameters_len > wasm::kV8MaxWasmFunctionParams) {
    thrower.TypeError("Argument 0 contains too many parameters");
    return js_api_scope.AssertException();
  }

  // Decode the tag type and construct a signature.
  std::vector<wasm::ValueType> param_types(parameters_len, wasm::kWasmVoid);
  for (uint32_t i = 0; i < parameters_len; ++i) {
    wasm::ValueType& type = param_types[i];
    MaybeDirectHandle<Object> maybe =
        JSReceiver::GetElement(isolate, parameters, i);
    std::optional<wasm::ValueType> maybe_type =
        GetValueType(isolate, maybe, enabled_features);
    if (!maybe_type) return js_api_scope.AssertException();
    type = *maybe_type;
    if (type == wasm::kWasmVoid) {
      thrower.TypeError(
          "Argument 0 parameter type at index #%u must be a value type", i);
      return js_api_scope.AssertException();
    }
  }
  const wasm::FunctionSig sig{0, parameters_len, param_types.data()};
  // Set the tag index to 0. It is only used for debugging purposes, and has no
  // meaningful value when declared outside of a wasm module.
  auto tag = WasmExceptionTag::New(isolate, 0);

  wasm::CanonicalTypeIndex type_index =
      wasm::GetWasmEngine()->type_canonicalizer()->AddRecursiveGroup(&sig);

  DirectHandle<Map> map;
  if (!GetDerivedMap(isolate, args).ToHandle(&map)) {
    return js_api_scope.AssertException();
  }

  return *WasmTagObject::New(isolate, &sig, type_index, tag,
                             DirectHandle<WasmTrustedInstanceData>(), map);
}

namespace {

V8_WARN_UNUSED_RESULT bool EncodeExceptionValues(
    Isolate* isolate, const wasm::CanonicalSig* signature,
    DirectHandle<WasmTagObject> tag_object, DirectHandle<Object> arg,
    ErrorThrower* thrower, DirectHandle<FixedArray> values_out) {
  uint32_t index = 0;
  DirectHandle<JSReceiver> values;
  if (!TryCast<JSReceiver>(arg, &values)) {
    thrower->TypeError("Exception values must be an iterable object");
    return false;
  }
  uint32_t length = GetIterableLength(isolate, values);
  if (length == kMaxUInt32) {
    thrower->TypeError("Exception values argument has no length");
    return false;
  }
  if (length != signature->parameter_count()) {
    thrower->TypeError(
        "Number of exception values does not match signature length");
    return false;
  }
  for (size_t param_idx = 0; param_idx < signature->parameter_count();
       ++param_idx) {
    static_assert(wasm::kV8MaxWasmFunctionParams <= kMaxUInt32);
    DirectHandle<Object> value;
    if (!JSReceiver::GetElement(isolate, values,
                                static_cast<uint32_t>(param_idx))
             .ToHandle(&value)) {
      return false;
    }
    wasm::CanonicalValueType type = signature->GetParam(param_idx);
    switch (type.kind()) {
      case wasm::kI32: {
        int32_t i32 = 0;
        if (!ToI32(isolate, value, &i32)) return false;
        EncodeI32ExceptionValue(values_out, &index, i32);
        break;
      }
      case wasm::kI64: {
        int64_t i64 = 0;
        if (!ToI64(isolate, value, &i64)) return false;
        EncodeI64ExceptionValue(values_out, &index, i64);
        break;
      }
      case wasm::kF32: {
        float f32 = 0;
        if (!ToF32(isolate, value, &f32)) return false;
        int32_t i32 = base::bit_cast<int32_t>(f32);
        EncodeI32ExceptionValue(values_out, &index, i32);
        break;
      }
      case wasm::kF64: {
        double f64 = 0;
        if (!ToF64(isolate, value, &f64)) return false;
        int64_t i64 = base::bit_cast<int64_t>(f64);
        EncodeI64ExceptionValue(values_out, &index, i64);
        break;
      }
      case wasm::kRef:
      case wasm::kRefNull: {
        const char* error_message;
        if (!wasm::JSToWasmObject(isolate, value, type, &error_message)
                 .ToHandle(&value)) {
          thrower->TypeError("%s", error_message);
          return false;
        }
        values_out->set(index++, *value);
        break;
      }
      case wasm::kS128:
        thrower->TypeError("Invalid type v128");
        return false;
      case wasm::kI8:
      case wasm::kI16:
      case wasm::kF16:
      case wasm::kVoid:
      case wasm::kTop:
      case wasm::kBottom:
        UNREACHABLE();
    }
  }
  return true;
}

}  // namespace

// WebAssembly.Exception
BUILTIN(WebAssemblyException) {
  WasmJSConstructorScope js_api_scope{isolate, "WebAssembly.Exception()"};
  ErrorThrower& thrower = js_api_scope.thrower();

  if (!CheckConstructCall(args, &thrower, "WebAssembly.Exception")) {
    return js_api_scope.AssertException();
  }
  DirectHandle<Object> arg0 = args.atOrUndefined(isolate, 1);
  if (!IsWasmTagObject(*arg0)) {
    thrower.TypeError("Argument 0 must be a WebAssembly tag");
    return js_api_scope.AssertException();
  }
  DirectHandle<WasmTagObject> tag_object = Cast<WasmTagObject>(arg0);
  DirectHandle<WasmExceptionTag> tag(Cast<WasmExceptionTag>(tag_object->tag()),
                                     isolate);
  auto js_tag = Cast<WasmTagObject>(isolate->context()->wasm_js_tag());
  if (*tag == js_tag->tag()) {
    thrower.TypeError("Argument 0 cannot be WebAssembly.JSTag");
    return js_api_scope.AssertException();
  }
  const wasm::CanonicalSig* sig =
      wasm::GetTypeCanonicalizer()->LookupFunctionSignature(
          wasm::CanonicalTypeIndex{
              static_cast<uint32_t>(tag_object->canonical_type_index())});

  if (sig->return_count() != 0) {
    thrower.TypeError(
        "Invalid WebAssembly tag (return values not permitted in Exception "
        "tag)");
    return js_api_scope.AssertException();
  }

  uint32_t size = WasmExceptionPackage::GetEncodedSize(sig);
  DirectHandle<FixedArray> values = isolate->factory()->NewFixedArray(size);
  DirectHandle<Object> arg1 = args.atOrUndefined(isolate, 2);
  if (!EncodeExceptionValues(isolate, sig, tag_object, arg1, &thrower,
                             values)) {
    return js_api_scope.AssertException();
  }

  // Third argument: optional ExceptionOption ({traceStack: <bool>}).
  DirectHandle<Object> arg2 = args.atOrUndefined(isolate, 3);
  if (!IsNullOrUndefined(*arg2) && !IsJSReceiver(*arg2)) {
    thrower.TypeError("Argument 2 is not an object");
    return js_api_scope.AssertException();
  }
  bool trace_stack = false;
  if (DirectHandle<JSReceiver> trace_stack_obj;
      TryCast<JSReceiver>(arg2, &trace_stack_obj)) {
    DirectHandle<Object> trace_stack_value;
    if (!JSReceiver::GetProperty(isolate, trace_stack_obj, "traceStack")
             .ToHandle(&trace_stack_value)) {
      return js_api_scope.AssertException();
    }
    trace_stack = Object::BooleanValue(*trace_stack_value, isolate);
  }

  DirectHandle<Map> map;
  if (!GetDerivedMap(isolate, args).ToHandle(&map)) {
    return js_api_scope.AssertException();
  }

  DirectHandle<WasmExceptionPackage> runtime_exception =
      WasmExceptionPackage::New(isolate, tag, values, map);
  if (trace_stack) {
    // Skip frames up to and including `new.target` (to exclude the
    // WebAssembly.Exception constructor and any subclass constructors), or
    // just the top builtin frame if `new.target` is not a JSFunction (e.g. a
    // Proxy).
    Handle<Object> caller = args.new_target();
    FrameSkipMode mode = IsJSFunction(*caller) ? SKIP_UNTIL_SEEN : SKIP_FIRST;

    DirectHandle<Object> capture_result;
    if (!ErrorUtils::CaptureStackTrace(isolate, runtime_exception, mode, caller)
             .ToHandle(&capture_result)) {
      return js_api_scope.AssertException();
    }
  }

  return *runtime_exception;
}

DirectHandle<JSFunction> NewPromisingWasmExportedFunction(
    Isolate* isolate, DirectHandle<WasmExportedFunctionData> data,
    ErrorThrower& thrower) {
  DirectHandle<WasmTrustedInstanceData> trusted_instance_data{
      data->instance_data(), isolate};
  int func_index = data->function_index();
  const wasm::WasmModule* module = trusted_instance_data->module();
  wasm::ModuleTypeIndex sig_index = module->functions[func_index].sig_index;
  const wasm::CanonicalSig* sig = data->internal()->sig();
  DirectHandle<Code> wrapper;
  if (!wasm::IsJSCompatibleSignature(sig)) {
    // If the signature is incompatible with JS, the original export will have
    // compiled an incompatible signature wrapper, so fetch it from the cache.
    wrapper = WasmExportedFunction::GetWrapper(isolate, sig);
  } else {
    wrapper = BUILTIN_CODE(isolate, WasmPromising);
  }

  // TODO(14034): Create funcref RTTs lazily?
  DirectHandle<Map> rtt{
      Cast<Map>(
          trusted_instance_data->managed_object_maps()->get(sig_index.index)),
      isolate};

  int num_imported_functions = module->num_imported_functions;
  DirectHandle<TrustedObject> implicit_arg;
  if (func_index >= num_imported_functions) {
    implicit_arg = trusted_instance_data;
  } else {
    implicit_arg = direct_handle(
        TrustedCast<WasmImportData>(
            trusted_instance_data->dispatch_table_for_imports()->implicit_arg(
                func_index)),
        isolate);
  }

  DirectHandle<WasmInternalFunction> internal =
      isolate->factory()->NewWasmInternalFunction(
          implicit_arg, func_index,
          trusted_instance_data->GetCallTarget(func_index), sig);
  DirectHandle<WasmFuncRef> func_ref =
      isolate->factory()->NewWasmFuncRef(internal, rtt);

  DirectHandle<JSFunction> result = WasmExportedFunction::New(
      isolate, trusted_instance_data, func_ref, internal,
      static_cast<int>(sig->parameter_count()), wrapper);
  return result;
}

// WebAssembly.promising(Function) -> Function
void WebAssemblyPromising(const v8::FunctionCallbackInfo<v8::Value>& info) {
  WasmJSApiScope js_api_scope{info, "WebAssembly.promising()"};
  auto [isolate, thrower] = js_api_scope.isolate_and_thrower();
  isolate->CountUsage(v8::Isolate::kWasmJavaScriptPromiseIntegration);

  DirectHandle<Object> arg0 = Utils::OpenDirectHandle(*info[0]);
  if (!IsCallable(*arg0)) {
    thrower.TypeError("Argument 0 must be a function");
    return;
  }
  DirectHandle<JSReceiver> callable = Cast<JSReceiver>(arg0);

  if (!WasmExportedFunction::IsWasmExportedFunction(*callable)) {
    thrower.TypeError("Argument 0 must be a WebAssembly exported function");
    return;
  }
  auto wasm_exported_function = Cast<WasmExportedFunction>(*callable);
  DirectHandle<WasmExportedFunctionData> data(
      wasm_exported_function->shared()->wasm_exported_function_data(), isolate);
  DirectHandle<JSFunction> result =
      NewPromisingWasmExportedFunction(isolate, data, thrower);
  info.GetReturnValue().Set(Utils::ToLocal(Cast<JSObject>(result)));
}

// WebAssembly.Suspending(Function) -> Suspending
BUILTIN(WebAssemblySuspending) {
  WasmJSConstructorScope js_api_scope{isolate, "WebAssembly.Suspending()"};
  ErrorThrower& thrower = js_api_scope.thrower();
  isolate->CountUsage(v8::Isolate::kWasmJavaScriptPromiseIntegration);

  if (!CheckConstructCall(args, &thrower, "WebAssembly.Suspending")) {
    return js_api_scope.AssertException();
  }
  DirectHandle<Object> arg0 = args.atOrUndefined(isolate, 1);
  if (!IsCallable(*arg0)) {
    thrower.TypeError("Argument 0 must be a function");
    return js_api_scope.AssertException();
  }

  DirectHandle<JSReceiver> callable = Cast<JSReceiver>(arg0);

  if (WasmExportedFunction::IsWasmExportedFunction(*callable)) {
    thrower.TypeError("Argument 0 must not be a WebAssembly function");
    return js_api_scope.AssertException();
  }

  DirectHandle<Map> map;
  if (!GetDerivedMap(isolate, args).ToHandle(&map)) {
    return js_api_scope.AssertException();
  }

  return *WasmSuspendingObject::New(isolate, callable, map);
}

constexpr const char* kName_WasmGlobalObject = "WebAssembly.Global";
constexpr const char* kName_WasmMemoryObject = "WebAssembly.Memory";
constexpr const char* kName_WasmInstanceObject = "WebAssembly.Instance";
constexpr const char* kName_WasmTableObject = "WebAssembly.Table";
constexpr const char* kName_WasmExceptionPackage = "WebAssembly.Exception";

#define EXTRACT_THIS(var, WasmType)                                        \
  DirectHandle<WasmType> var;                                              \
  {                                                                        \
    DirectHandle<Object> this_arg = Utils::OpenDirectHandle(*info.This()); \
    if (!Is##WasmType(*this_arg)) {                                        \
      thrower.TypeError("Receiver is not a %s", kName_##WasmType);         \
      return;                                                              \
    }                                                                      \
    var = Cast<WasmType>(this_arg);                                        \
  }

void WebAssemblyInstanceGetExportsImpl(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  WasmJSApiScope js_api_scope{info, "WebAssembly.Instance.exports()"};
  auto [isolate, thrower] = js_api_scope.isolate_and_thrower();
  EXTRACT_THIS(receiver, WasmInstanceObject);
  DirectHandle<JSObject> exports_object(
      Cast<JSObject>(receiver->exports_object()), isolate);

  info.GetReturnValue().Set(Utils::ToLocal(exports_object));
}

void WebAssemblyTableGetLengthImpl(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  WasmJSApiScope js_api_scope{info, "WebAssembly.Table.length()"};
  auto [isolate, thrower] = js_api_scope.isolate_and_thrower();
  EXTRACT_THIS(receiver, WasmTableObject);

  int length = receiver->current_length();
  DCHECK_LE(0, length);
  info.GetReturnValue().Set(Utils::ToLocal(AddressValueFromUnsigned(
      isolate, receiver->address_type(), static_cast<uint32_t>(length))));
}

// WebAssembly.Table.grow(num, init_value = null) -> num
void WebAssemblyTableGrowImpl(const v8::FunctionCallbackInfo<v8::Value>& info) {
  WasmJSApiScope js_api_scope{info, "WebAssembly.Table.grow()"};
  auto [isolate, thrower] = js_api_scope.isolate_and_thrower();
  EXTRACT_THIS(receiver, WasmTableObject);

  std::optional<uint64_t> maybe_grow_by =
      AddressValueToU64(isolate, &thrower, Utils::OpenDirectHandle(*info[0]),
                        "Argument 0", receiver->address_type());
  if (!maybe_grow_by) return js_api_scope.AssertException();
  uint64_t grow_by = *maybe_grow_by;

  DirectHandle<Object> init_value;

  if (info.Length() >= 2) {
    init_value = Utils::OpenDirectHandle(*info[1]);
    const char* error_message;
    if (!WasmTableObject::JSToWasmElement(isolate, receiver, init_value,
                                          &error_message)
             .ToHandle(&init_value)) {
      thrower.TypeError("Argument 1 is invalid: %s", error_message);
      return;
    }
  } else if (receiver->unsafe_type().is_non_nullable()) {
    thrower.TypeError(
        "Argument 1 must be specified for non-nullable element type");
    return;
  } else {
    init_value = DefaultReferenceValue(isolate, receiver->unsafe_type());
  }

  static_assert(wasm::kV8MaxWasmTableSize <= kMaxUInt32);
  DirectHandle<WasmDispatchTable> dispatch_table(
      receiver->trusted_dispatch_table(isolate), isolate);
  int old_size =
      grow_by > wasm::max_table_size()
          ? -1
          : WasmTableObject::Grow(isolate, receiver, dispatch_table,
                                  static_cast<uint32_t>(grow_by), init_value);
  if (old_size < 0) {
    thrower.RangeError("failed to grow table by %" PRIu64, grow_by);
    return;
  }
  info.GetReturnValue().Set(Utils::ToLocal(AddressValueFromUnsigned(
      isolate, receiver->address_type(), static_cast<uint32_t>(old_size))));
}

namespace {
V8_WARN_UNUSED_RESULT bool WasmObjectToJSReturnValue(
    v8::ReturnValue<v8::Value>& return_value, DirectHandle<Object> value,
    wasm::ValueTypeBase unsafe_type, Isolate* isolate, ErrorThrower* thrower) {
  if (unsafe_type.is_abstract_ref()) {
    switch (unsafe_type.generic_kind()) {
      case wasm::GenericKind::kStringViewIter:
      case wasm::GenericKind::kStringViewWtf8:
      case wasm::GenericKind::kStringViewWtf16:
      case wasm::GenericKind::kExn:
      case wasm::GenericKind::kNoExn:
      case wasm::GenericKind::kCont:
      case wasm::GenericKind::kNoCont:
      case wasm::GenericKind::kWaitqueue:
      case wasm::GenericKind::kNoWaitqueue:
        thrower->TypeError("invalid type %s", unsafe_type.name().c_str());
        return false;
      default:
        break;
    }
  } else if (unsafe_type.has_index() &&
             unsafe_type.ref_type_kind() == wasm::RefTypeKind::kCont) {
    thrower->TypeError("invalid type %s", unsafe_type.name().c_str());
    return false;
  }
  return_value.Set(Utils::ToLocal(
      wasm::WasmToJSObject(isolate, value, wasm::kUnshareStrings)));
  return true;
}
}  // namespace

// WebAssembly.Table.get(num) -> any
void WebAssemblyTableGetImpl(const v8::FunctionCallbackInfo<v8::Value>& info) {
  WasmJSApiScope js_api_scope{info, "WebAssembly.Table.get()"};
  auto [isolate, thrower] = js_api_scope.isolate_and_thrower();
  EXTRACT_THIS(receiver, WasmTableObject);

  std::optional<uint64_t> maybe_address =
      AddressValueToU64(isolate, &thrower, Utils::OpenDirectHandle(*info[0]),
                        "Argument 0", receiver->address_type());
  if (!maybe_address) return;
  uint64_t address = *maybe_address;

  if (address > kMaxUInt32 ||
      !receiver->is_in_bounds(static_cast<uint32_t>(address))) {
    thrower.RangeError("invalid address %" PRIu64 " in %s table of size %d",
                       address, receiver->unsafe_type().name().c_str(),
                       receiver->current_length());
    return;
  }

  DirectHandle<Object> result =
      WasmTableObject::Get(isolate, receiver, static_cast<uint32_t>(address));

  v8::ReturnValue<v8::Value> return_value = info.GetReturnValue();
  if (!WasmObjectToJSReturnValue(return_value, result, receiver->unsafe_type(),
                                 isolate, &thrower)) {
    return js_api_scope.AssertException();
  }
}

// WebAssembly.Table.set(num, any)
void WebAssemblyTableSetImpl(const v8::FunctionCallbackInfo<v8::Value>& info) {
  WasmJSApiScope js_api_scope{info, "WebAssembly.Table.set()"};
  auto [isolate, thrower] = js_api_scope.isolate_and_thrower();
  EXTRACT_THIS(table_object, WasmTableObject);

  std::optional<uint64_t> maybe_address =
      AddressValueToU64(isolate, &thrower, Utils::OpenDirectHandle(*info[0]),
                        "Argument 0", table_object->address_type());
  if (!maybe_address) return js_api_scope.AssertException();
  uint64_t address = *maybe_address;

  if (address > kMaxUInt32 ||
      !table_object->is_in_bounds(static_cast<uint32_t>(address))) {
    thrower.RangeError("invalid address %" PRIu64 " in %s table of size %d",
                       address, table_object->unsafe_type().name().c_str(),
                       table_object->current_length());
    return;
  }

  DirectHandle<Object> element;
  if (info.Length() >= 2) {
    element = Utils::OpenDirectHandle(*info[1]);
    const char* error_message;
    if (!WasmTableObject::JSToWasmElement(isolate, table_object, element,
                                          &error_message)
             .ToHandle(&element)) {
      thrower.TypeError("Argument 1 is invalid for table: %s", error_message);
      return;
    }
  } else if (table_object->unsafe_type().is_defaultable()) {
    element = DefaultReferenceValue(isolate, table_object->unsafe_type());
  } else {
    thrower.TypeError("Table of non-defaultable type %s needs explicit element",
                      table_object->unsafe_type().name().c_str());
    return;
  }

  DirectHandle<WasmDispatchTable> dispatch_table(
      table_object->trusted_dispatch_table(isolate), isolate);
  WasmTableObject::Set(isolate, table_object, dispatch_table,
                       static_cast<uint32_t>(address), element);
}

// WebAssembly.MemoryMapDescriptor.map()
void WebAssemblyMemoryMapDescriptorMapImpl(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  CHECK(v8_flags.wasm_memory_control);
  WasmJSApiScope js_api_scope{info, "WebAssembly.MemoryMapDescriptor.map()"};
  auto [isolate, i_isolate, thrower] = js_api_scope.isolates_and_thrower();
  auto* descriptor =
      v8::Object::Unwrap<wasm::WasmMemoryMapDescriptor::kPointerTag,
                         wasm::WasmMemoryMapDescriptor>(isolate, info.This());
  if (!descriptor) {
    thrower.TypeError(
        "Incompatible receiver, expected WebAssembly.MemoryMapDescriptor");
    return js_api_scope.AssertException();
  }

  DirectHandle<WasmMemoryObject> memory;
  {
    DirectHandle<Object> memory_param = Utils::OpenDirectHandle(*info[0]);
    if (!IsWasmMemoryObject(*memory_param)) {
      thrower.TypeError("Parameter is not a WebAssembly.Memory");
      return js_api_scope.AssertException();
    }
    memory = Cast<WasmMemoryObject>(memory_param);
  }

  std::optional<uint32_t> offset = EnforceUint32(
      "Argument 1", Utils::OpenDirectHandle(*info[1]), i_isolate, &thrower);
  if (!offset.has_value()) {
    return js_api_scope.AssertException();
  }
  size_t mapped_size = descriptor->Map(isolate, memory, offset.value());
  if (!mapped_size) {
    thrower.RuntimeError(
        "Failed to map the MemoryMapDescriptor to WebAssembly memory.");
    return js_api_scope.AssertException();
  }
  info.GetReturnValue().Set(static_cast<int64_t>(mapped_size));
}

// WebAssembly.MemoryMapDescriptor.unmap()
void WebAssemblyMemoryMapDescriptorUnmapImpl(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  CHECK(v8_flags.wasm_memory_control);
  WasmJSApiScope js_api_scope{info, "WebAssembly.MemoryMapDescriptor.unmap()"};
  auto [isolate, i_isolate, thrower] = js_api_scope.isolates_and_thrower();
  auto* descriptor =
      v8::Object::Unwrap<wasm::WasmMemoryMapDescriptor::kPointerTag,
                         wasm::WasmMemoryMapDescriptor>(isolate, info.This());
  if (!descriptor) {
    thrower.TypeError(
        "Incompatible receiver, expected WebAssembly.MemoryMapDescriptor");
    return js_api_scope.AssertException();
  }

  if (!descriptor->Unmap(isolate)) {
    thrower.RangeError("Failed to unmap the MemoryMapDescriptor.");
    return;
  }
}

// WebAssembly.Memory.grow(num) -> num
void WebAssemblyMemoryGrowImpl(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  WasmJSApiScope js_api_scope{info, "WebAssembly.Memory.grow()"};
  auto [isolate, thrower] = js_api_scope.isolate_and_thrower();
  EXTRACT_THIS(receiver, WasmMemoryObject);

  std::optional<uint64_t> maybe_delta_pages =
      AddressValueToU64(isolate, &thrower, Utils::OpenDirectHandle(*info[0]),
                        "Argument 0", receiver->address_type());
  if (!maybe_delta_pages) return js_api_scope.AssertException();
  uint64_t delta_pages = *maybe_delta_pages;

  CppGCManaged<BackingStore>::Ptr backing_store = receiver->backing_store();
#ifdef DEBUG
  if (Tagged<JSArrayBuffer> buffer;
      TryCast(receiver->array_buffer(), &buffer)) {
    DCHECK_EQ(buffer->GetBackingStore(), backing_store);
  }
#endif  // DEBUG

  uint64_t old_pages = backing_store->byte_length(std::memory_order_seq_cst) /
                       wasm::kWasmPageSize;
  uint64_t max_pages = receiver->maximum_pages();

  if (delta_pages > max_pages - old_pages) {
    thrower.RangeError("Maximum memory size exceeded");
    return;
  }

  int32_t ret;
  if (delta_pages > wasm::kV8MaxWasmMemory64Pages - old_pages) {
    ret = -1;
  } else {
    static_assert(wasm::kV8MaxWasmMemory64Pages <= kMaxUInt32);
    ret = WasmMemoryObject::Grow(isolate, receiver,
                                 static_cast<uint32_t>(delta_pages));
  }
  if (ret == -1) {
    thrower.RangeError("Unable to grow instance memory");
    return;
  }
  info.GetReturnValue().Set(Utils::ToLocal(AddressValueFromUnsigned(
      isolate, receiver->address_type(), static_cast<uint32_t>(ret))));
}

// WebAssembly.Memory.buffer -> ArrayBuffer
void WebAssemblyMemoryGetBufferImpl(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  WasmJSApiScope js_api_scope{info, "WebAssembly.Memory.buffer"};
  auto [isolate, thrower] = js_api_scope.isolate_and_thrower();
  EXTRACT_THIS(receiver, WasmMemoryObject);

  DirectHandle<JSArrayBuffer> buffer =
      WasmMemoryObject::GetArrayBuffer(isolate, receiver);
  info.GetReturnValue().Set(Utils::ToLocal(buffer));
}

// WebAssembly.Memory.toFixedLengthBuffer() -> ArrayBuffer
void WebAssemblyMemoryToFixedLengthBufferImpl(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  WasmJSApiScope js_api_scope{info, "WebAssembly.Memory.toFixedLengthBuffer()"};
  auto [isolate, thrower] = js_api_scope.isolate_and_thrower();
  EXTRACT_THIS(receiver, WasmMemoryObject);

  DirectHandle<JSArrayBuffer> buffer =
      WasmMemoryObject::ChangeArrayBufferResizability(isolate, receiver,
                                                      ResizableFlag{false});
  info.GetReturnValue().Set(Utils::ToLocal(buffer));
}

// WebAssembly.Memory.toResizableBuffer() -> ArrayBuffer
void WebAssemblyMemoryToResizableBufferImpl(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  WasmJSApiScope js_api_scope{info, "WebAssembly.Memory.toResizableBuffer()"};
  auto [isolate, thrower] = js_api_scope.isolate_and_thrower();
  EXTRACT_THIS(receiver, WasmMemoryObject);

  if (!receiver->has_maximum_pages()) {
    thrower.TypeError("Memory must have a maximum");
    return;
  }

  DirectHandle<JSArrayBuffer> buffer =
      WasmMemoryObject::ChangeArrayBufferResizability(isolate, receiver,
                                                      ResizableFlag{true});
  info.GetReturnValue().Set(Utils::ToLocal(buffer));
}

void WebAssemblyExceptionGetArgImpl(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  WasmJSApiScope js_api_scope{info, "WebAssembly.Exception.getArg()"};
  auto [isolate, thrower] = js_api_scope.isolate_and_thrower();
  EXTRACT_THIS(exception, WasmExceptionPackage);

  DirectHandle<WasmTagObject> tag_object;
  if (!GetFirstArgumentAsTag(info, &thrower).ToHandle(&tag_object)) {
    return js_api_scope.AssertException();
  }
  std::optional<uint32_t> maybe_index = EnforceUint32(
      "Index", Utils::OpenDirectHandle(*info[1]), isolate, &thrower);
  if (!maybe_index) return js_api_scope.AssertException();
  uint32_t index = *maybe_index;
  auto maybe_values =
      WasmExceptionPackage::GetExceptionValues(isolate, exception);

  auto this_tag = WasmExceptionPackage::GetExceptionTag(isolate, exception);
  DCHECK(IsWasmExceptionTag(*this_tag));
  if (tag_object->tag() != *this_tag) {
    thrower.TypeError("First argument does not match the exception tag");
    return;
  }

  const wasm::CanonicalSig* sig =
      wasm::GetTypeCanonicalizer()->LookupFunctionSignature(
          wasm::CanonicalTypeIndex{
              static_cast<uint32_t>(tag_object->canonical_type_index())});
  DCHECK(!IsUndefined(*maybe_values));
  auto values = Cast<FixedArray>(maybe_values);
  if (index >= static_cast<uint32_t>(sig->parameter_count())) {
    thrower.RangeError("Index out of range");
    return;
  }
  // First, find the index in the values array.
  uint32_t decode_index = 0;
  // Since the bounds check above passed, the cast to int is safe.
  for (int i = 0; i < static_cast<int>(index); ++i) {
    switch (sig->GetParam(i).kind()) {
      case wasm::kI32:
      case wasm::kF32:
        decode_index += 2;
        break;
      case wasm::kI64:
      case wasm::kF64:
        decode_index += 4;
        break;
      case wasm::kRef:
      case wasm::kRefNull:
        decode_index++;
        break;
      case wasm::kS128:
        decode_index += 8;
        break;
      case wasm::kI8:
      case wasm::kI16:
      case wasm::kF16:
      case wasm::kVoid:
      case wasm::kTop:
      case wasm::kBottom:
        UNREACHABLE();
    }
  }
  // Decode the value at {decode_index}.
  ReturnValue<v8::Value> return_value = info.GetReturnValue();
  switch (sig->GetParam(index).kind()) {
    case wasm::kI32: {
      uint32_t u32_bits = 0;
      DecodeI32ExceptionValue(values, &decode_index, &u32_bits);
      return_value.Set(static_cast<int32_t>(u32_bits));
      return;
    }
    case wasm::kI64: {
      uint64_t u64_bits = 0;
      DecodeI64ExceptionValue(values, &decode_index, &u64_bits);
      return_value.Set(Utils::ToLocal(BigInt::FromInt64(isolate, u64_bits)));
      return;
    }
    case wasm::kF32: {
      uint32_t f32_bits = 0;
      DecodeI32ExceptionValue(values, &decode_index, &f32_bits);
      return_value.Set(base::bit_cast<float>(f32_bits));
      return;
    }
    case wasm::kF64: {
      uint64_t f64_bits = 0;
      DecodeI64ExceptionValue(values, &decode_index, &f64_bits);
      return_value.Set(base::bit_cast<double>(f64_bits));
      return;
    }
    case wasm::kRef:
    case wasm::kRefNull: {
      DirectHandle<Object> obj(values->get(decode_index), isolate);
      if (!WasmObjectToJSReturnValue(return_value, obj, sig->GetParam(index),
                                     isolate, &thrower)) {
        return js_api_scope.AssertException();
      }
      return;
    }
    case wasm::kS128:
      thrower.TypeError("Invalid type v128");
      return;
    case wasm::kI8:
    case wasm::kI16:
    case wasm::kF16:
    case wasm::kVoid:
    case wasm::kTop:
    case wasm::kBottom:
      UNREACHABLE();
  }
}

void WebAssemblyExceptionIsImpl(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  WasmJSApiScope js_api_scope{info, "WebAssembly.Exception.is()"};
  auto [isolate, thrower] = js_api_scope.isolate_and_thrower();
  EXTRACT_THIS(exception, WasmExceptionPackage);

  auto tag = WasmExceptionPackage::GetExceptionTag(isolate, exception);
  DCHECK(IsWasmExceptionTag(*tag));

  DirectHandle<WasmTagObject> tag_object;
  if (!GetFirstArgumentAsTag(info, &thrower).ToHandle(&tag_object)) {
    return js_api_scope.AssertException();
  }
  info.GetReturnValue().Set(tag_object->tag() == *tag);
}

void WebAssemblyExceptionGetStackImpl(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  WasmJSApiScope js_api_scope{info, "WebAssembly.Exception.stack()"};
  ErrorThrower& thrower = js_api_scope.thrower();
  EXTRACT_THIS(exception, WasmExceptionPackage);

  info.GetReturnValue().SetUndefined();
}

void WebAssemblyGlobalGetValueCommon(WasmJSApiScope& js_api_scope) {
  auto [isolate, thrower] = js_api_scope.isolate_and_thrower();
  auto& info = js_api_scope.callback_info();  // Needed by EXTRACT_THIS.
  EXTRACT_THIS(receiver, WasmGlobalObject);

  v8::ReturnValue<v8::Value> return_value = info.GetReturnValue();

  wasm::ValueType receiver_type = receiver->unsafe_type();
  switch (receiver_type.kind()) {
    case wasm::kI32:
      return_value.Set(receiver->GetI32());
      break;
    case wasm::kI64:
      return_value.Set(
          Utils::ToLocal(BigInt::FromInt64(isolate, receiver->GetI64())));
      break;
    case wasm::kF32:
      return_value.Set(receiver->GetF32());
      break;
    case wasm::kF64:
      return_value.Set(receiver->GetF64());
      break;
    case wasm::kS128:
      thrower.TypeError("Can't get the value of s128 WebAssembly.Global");
      break;
    case wasm::kRef:
    case wasm::kRefNull:
      if (!WasmObjectToJSReturnValue(return_value, receiver->GetRef(),
                                     receiver_type, isolate, &thrower)) {
        return js_api_scope.AssertException();
      }
      break;
    case wasm::kI8:
    case wasm::kI16:
    case wasm::kF16:
    case wasm::kTop:
    case wasm::kBottom:
    case wasm::kVoid:
      UNREACHABLE();
  }
}

// WebAssembly.Global.valueOf() -> num
void WebAssemblyGlobalValueOfImpl(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  WasmJSApiScope js_api_scope{info, "WebAssembly.Global.valueOf()"};
  return WebAssemblyGlobalGetValueCommon(js_api_scope);
}

// get WebAssembly.Global.value -> num
void WebAssemblyGlobalGetValueImpl(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  WasmJSApiScope js_api_scope{info, "get WebAssembly.Global.value)"};
  return WebAssemblyGlobalGetValueCommon(js_api_scope);
}

// set WebAssembly.Global.value(num)
void WebAssemblyGlobalSetValueImpl(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  WasmJSApiScope js_api_scope{info, "set WebAssembly.Global.value)"};
  auto [isolate, thrower] = js_api_scope.isolate_and_thrower();
  EXTRACT_THIS(receiver, WasmGlobalObject);

  if (!receiver->is_mutable()) {
    thrower.TypeError("Can't set the value of an immutable global.");
    return;
  }

  DirectHandle<Object> value = Utils::OpenDirectHandle(*info[0]);
  wasm::ValueType unsafe_type = receiver->unsafe_type();
  switch (unsafe_type.kind()) {
    case wasm::kI32: {
      DirectHandle<Object> int32_value;
      if (!Object::ToInt32(isolate, value).ToHandle(&int32_value)) {
        return js_api_scope.AssertException();
      }
      receiver->SetI32(NumberToInt32(*int32_value));
      break;
    }
    case wasm::kI64: {
      DirectHandle<BigInt> bigint_value;
      if (!BigInt::FromObject(isolate, value).ToHandle(&bigint_value)) {
        return js_api_scope.AssertException();
      }
      receiver->SetI64(bigint_value->AsInt64());
      break;
    }
    case wasm::kF32: {
      DirectHandle<Number> number_value;
      if (!Object::ToNumber(isolate, value).ToHandle(&number_value)) {
        return js_api_scope.AssertException();
      }
      receiver->SetF32(DoubleToFloat32(Object::NumberValue(*number_value)));
      break;
    }
    case wasm::kF64: {
      DirectHandle<Number> number_value;
      if (!Object::ToNumber(isolate, value).ToHandle(&number_value)) {
        return js_api_scope.AssertException();
      }
      receiver->SetF64(Object::NumberValue(*number_value));
      break;
    }
    case wasm::kS128:
      thrower.TypeError("Can't set the value of s128 WebAssembly.Global");
      break;
    case wasm::kRef:
    case wasm::kRefNull: {
      const wasm::WasmModule* module =
          receiver->has_trusted_data()
              ? receiver->trusted_data(isolate)->module()
              : nullptr;
      const char* error_message;
      if (!wasm::JSToWasmObject(isolate, module, value, unsafe_type,
                                &error_message)
               .ToHandle(&value)) {
        thrower.TypeError("%s", error_message);
        return;
      }
      receiver->SetRef(value);
      return;
    }
    case wasm::kI8:
    case wasm::kI16:
    case wasm::kF16:
    case wasm::kTop:
    case wasm::kBottom:
    case wasm::kVoid:
      UNREACHABLE();
  }
}

namespace wasm {

// Define the callbacks in v8::internal::wasm namespace. The implementation is
// in v8::internal directly.
#define DEF_WASM_JS_EXTERNAL_REFERENCE(Name)                   \
  void Name(const v8::FunctionCallbackInfo<v8::Value>& info) { \
    Name##Impl(info);                                          \
  }
WASM_JS_EXTERNAL_REFERENCE_LIST(DEF_WASM_JS_EXTERNAL_REFERENCE)
#undef DEF_WASM_JS_EXTERNAL_REFERENCE

}  // namespace wasm

namespace {

// TODO(titzer): we use the API to create the function template because the
// internal guts are too ugly to replicate here.
DirectHandle<FunctionTemplateInfo> NewFunctionTemplate(
    Isolate* isolate, FunctionCallback func,
    SideEffectType side_effect_type = SideEffectType::kHasSideEffect) {
  v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);
  Local<FunctionTemplate> templ =
      FunctionTemplate::New(v8_isolate, func, {}, {}, 0,
                            ConstructorBehavior::kThrow, side_effect_type);
  return v8::Utils::OpenDirectHandle(*templ);
}

DirectHandle<JSFunction> CreateFunc(
    Isolate* isolate, DirectHandle<String> name, FunctionCallback func,
    SideEffectType side_effect_type = SideEffectType::kHasSideEffect) {
  DirectHandle<FunctionTemplateInfo> temp =
      NewFunctionTemplate(isolate, func, side_effect_type);

  DirectHandle<JSFunction> function =
      ApiNatives::InstantiateFunction(isolate, temp, name).ToHandleChecked();
  DCHECK(function->shared()->HasSharedName());
  return function;
}

DirectHandle<JSFunction> InstallFunc(
    Isolate* isolate, DirectHandle<JSObject> object, DirectHandle<String> name,
    FunctionCallback func, int length, PropertyAttributes attributes = NONE,
    SideEffectType side_effect_type = SideEffectType::kHasSideEffect) {
  DirectHandle<JSFunction> function =
      CreateFunc(isolate, name, func, side_effect_type);
  function->shared()->set_length(length);
  CHECK(!JSObject::HasRealNamedProperty(isolate, object, name).FromMaybe(true));
  CHECK(object->map()->is_extensible());
  JSObject::AddProperty(isolate, object, name, function, attributes);
  return function;
}

DirectHandle<JSFunction> InstallFunc(
    Isolate* isolate, DirectHandle<JSObject> object, const char* str,
    FunctionCallback func, int length, PropertyAttributes attributes = NONE,
    SideEffectType side_effect_type = SideEffectType::kHasSideEffect) {
  DirectHandle<String> name = v8_str(isolate, str);
  return InstallFunc(isolate, object, name, func, length, attributes,
                     side_effect_type);
}

DirectHandle<JSFunction> CreateConstructorFunc(Isolate* isolate,
                                               DirectHandle<String> name,
                                               Builtin builtin,
                                               int length = 1) {
  DirectHandle<SharedFunctionInfo> sfi =
      isolate->factory()->NewSharedFunctionInfoForBuiltin(name, builtin, length,
                                                          kDontAdapt);
  sfi->set_language_mode(LanguageMode::kStrict);
  sfi->set_native(true);
  // WebIDL interface constructors have a read-only `prototype` property;
  // JSFunction::GetDerivedMap also relies on this when falling back to the
  // intrinsic default prototype.
  DirectHandle<JSFunction> function =
      Factory::JSFunctionBuilder{isolate, sfi, isolate->native_context()}
          .set_map(isolate->function_with_readonly_prototype_map())
          .Build();
  return function;
}

DirectHandle<JSFunction> InstallConstructorFunc(Isolate* isolate,
                                                DirectHandle<JSObject> object,
                                                const char* str,
                                                Builtin builtin,
                                                int length = 1) {
  DirectHandle<String> name = v8_str(isolate, str);
  DirectHandle<JSFunction> function =
      CreateConstructorFunc(isolate, name, builtin, length);
  CHECK(!JSObject::HasRealNamedProperty(isolate, object, name).FromMaybe(true));
  CHECK(object->map()->is_extensible());
  JSObject::AddProperty(isolate, object, name, function, DONT_ENUM);
  return function;
}

DirectHandle<String> GetterName(Isolate* isolate, DirectHandle<String> name) {
  return Name::ToFunctionName(isolate, name, isolate->factory()->get_string())
      .ToHandleChecked();
}

void InstallGetter(Isolate* isolate, DirectHandle<JSObject> object,
                   const char* str, FunctionCallback func) {
  DirectHandle<String> name = v8_str(isolate, str);
  DirectHandle<JSFunction> function =
      CreateFunc(isolate, GetterName(isolate, name), func,
                 SideEffectType::kHasNoSideEffect);

  JSObject::DefineOwnAccessorIgnoreAttributes(
      object, name, function, isolate->factory()->null_value(), NONE)
      .Check();
}

DirectHandle<String> SetterName(Isolate* isolate, DirectHandle<String> name) {
  return Name::ToFunctionName(isolate, name, isolate->factory()->set_string())
      .ToHandleChecked();
}

void InstallGetterSetter(Isolate* isolate, DirectHandle<JSObject> object,
                         const char* str, FunctionCallback getter,
                         FunctionCallback setter) {
  DirectHandle<String> name = v8_str(isolate, str);
  DirectHandle<JSFunction> getter_func =
      CreateFunc(isolate, GetterName(isolate, name), getter,
                 SideEffectType::kHasNoSideEffect);
  DirectHandle<JSFunction> setter_func =
      CreateFunc(isolate, SetterName(isolate, name), setter);
  setter_func->shared()->set_length(1);

  JSObject::DefineOwnAccessorIgnoreAttributes(object, name, getter_func,
                                              setter_func, NONE)
      .Check();
}

DirectHandle<JSObject> SetupConstructor(Isolate* isolate,
                                        DirectHandle<JSFunction> constructor,
                                        Context::Field context_index,
                                        InstanceType instance_type,
                                        int instance_size, const char* name,
                                        int in_object_properties = 0) {
  // Needed by FastInitializeDerivedMap so subclass maps preserve in-object
  // fields (e.g. for WebAssembly.Exception).
  constructor->shared()->set_expected_nof_properties(in_object_properties);
  JSFunction::EnsureHasInitialMap(isolate, constructor);
  DirectHandle<JSObject> proto(
      Cast<JSObject>(constructor->instance_prototype()), isolate);
  DirectHandle<Map> map = isolate->factory()->NewContextfulMap(
      constructor, instance_type, instance_size, TERMINAL_FAST_ELEMENTS_KIND,
      in_object_properties);
  JSFunction::SetInitialMap(isolate, constructor, map, proto);
  constexpr PropertyAttributes ro_attributes =
      static_cast<PropertyAttributes>(DONT_ENUM | READ_ONLY);
  JSObject::AddProperty(isolate, proto,
                        isolate->factory()->to_string_tag_symbol(),
                        v8_str(isolate, name), ro_attributes);
  // Record the native context index on the constructor so
  // JSFunction::GetDerivedMap can look up the realm's intrinsic default
  // prototype when `new.target.prototype` is not a JSReceiver.
  JSObject::AddProperty(
      isolate, constructor, isolate->factory()->native_context_index_symbol(),
      direct_handle(Smi::FromInt(context_index), isolate), ro_attributes);
  isolate->native_context()->SetNoCell(context_index, *constructor);
  return proto;
}

constexpr wasm::ValueType kWasmExceptionTagParams[] = {
    wasm::kWasmExternRef,
};
constexpr wasm::FunctionSig kWasmExceptionTagSignature{
    0, arraysize(kWasmExceptionTagParams), kWasmExceptionTagParams};
}  // namespace

// static
void WasmJs::PrepareForSnapshot(Isolate* isolate) {
  DirectHandle<JSGlobalObject> global = isolate->global_object();
  DirectHandle<NativeContext> native_context(global->native_context(), isolate);

  CHECK(IsUndefined(
      native_context->GetNoCell(Context::WASM_WEBASSEMBLY_OBJECT_INDEX)));
  CHECK(IsUndefined(
      native_context->GetNoCell(Context::WASM_MODULE_CONSTRUCTOR_INDEX)));

  Factory* const f = isolate->factory();
  static constexpr PropertyAttributes ro_attributes =
      static_cast<PropertyAttributes>(DONT_ENUM | READ_ONLY);

  // Create the WebAssembly object.
  DirectHandle<JSObject> webassembly;
  {
    DirectHandle<String> WebAssembly_string = v8_str(isolate, "WebAssembly");
    // Not supposed to be called, hence using the kIllegal builtin as code.
    DirectHandle<SharedFunctionInfo> sfi = f->NewSharedFunctionInfoForBuiltin(
        WebAssembly_string, Builtin::kIllegal, 0, kDontAdapt);
    sfi->set_language_mode(LanguageMode::kStrict);

    DirectHandle<JSFunction> ctor =
        Factory::JSFunctionBuilder{isolate, sfi, native_context}.Build();
    JSFunction::SetPrototype(isolate, ctor,
                             isolate->initial_object_prototype());
    webassembly = f->NewJSObject(ctor, AllocationType::kOld);
    native_context->set_wasm_webassembly_object(*webassembly);

    JSObject::AddProperty(isolate, webassembly, f->to_string_tag_symbol(),
                          WebAssembly_string, ro_attributes);
    InstallFunc(isolate, webassembly, "compile", wasm::WebAssemblyCompile, 1);
    InstallFunc(isolate, webassembly, "validate", wasm::WebAssemblyValidate, 1);
    InstallFunc(isolate, webassembly, "instantiate",
                wasm::WebAssemblyInstantiate, 1);
  }

  // Create the Module object.
  InstallModule(isolate, webassembly);

  // Create the Instance object.
  {
    DirectHandle<JSFunction> instance_constructor = InstallConstructorFunc(
        isolate, webassembly, "Instance", Builtin::kWebAssemblyInstance);
    DirectHandle<JSObject> instance_proto = SetupConstructor(
        isolate, instance_constructor, Context::WASM_INSTANCE_CONSTRUCTOR_INDEX,
        WASM_INSTANCE_OBJECT_TYPE, WasmInstanceObject::kHeaderSize,
        "WebAssembly.Instance");
    InstallGetter(isolate, instance_proto, "exports",
                  wasm::WebAssemblyInstanceGetExports);
  }

  // Create the Table object.
  {
    DirectHandle<JSFunction> table_constructor = InstallConstructorFunc(
        isolate, webassembly, "Table", Builtin::kWebAssemblyTable);
    DirectHandle<JSObject> table_proto = SetupConstructor(
        isolate, table_constructor, Context::WASM_TABLE_CONSTRUCTOR_INDEX,
        WASM_TABLE_OBJECT_TYPE, WasmTableObject::kHeaderSize,
        "WebAssembly.Table");
    InstallGetter(isolate, table_proto, "length",
                  wasm::WebAssemblyTableGetLength);
    InstallFunc(isolate, table_proto, "grow", wasm::WebAssemblyTableGrow, 1);
    InstallFunc(isolate, table_proto, "set", wasm::WebAssemblyTableSet, 1);
    InstallFunc(isolate, table_proto, "get", wasm::WebAssemblyTableGet, 1, NONE,
                SideEffectType::kHasNoSideEffect);
  }

  // Create the Memory object.
  {
    DirectHandle<JSFunction> memory_constructor = InstallConstructorFunc(
        isolate, webassembly, "Memory", Builtin::kWebAssemblyMemory);
    DirectHandle<JSObject> memory_proto = SetupConstructor(
        isolate, memory_constructor, Context::WASM_MEMORY_CONSTRUCTOR_INDEX,
        WASM_MEMORY_OBJECT_TYPE, WasmMemoryObject::kHeaderSize,
        "WebAssembly.Memory");
    InstallFunc(isolate, memory_proto, "grow", wasm::WebAssemblyMemoryGrow, 1);
    InstallGetter(isolate, memory_proto, "buffer",
                  wasm::WebAssemblyMemoryGetBuffer);
    InstallFunc(isolate, memory_proto, "toFixedLengthBuffer",
                wasm::WebAssemblyMemoryToFixedLengthBuffer, 0);
    InstallFunc(isolate, memory_proto, "toResizableBuffer",
                wasm::WebAssemblyMemoryToResizableBuffer, 0);
  }

  // Create the Global object.
  {
    DirectHandle<JSFunction> global_constructor = InstallConstructorFunc(
        isolate, webassembly, "Global", Builtin::kWebAssemblyGlobal);
    DirectHandle<JSObject> global_proto = SetupConstructor(
        isolate, global_constructor, Context::WASM_GLOBAL_CONSTRUCTOR_INDEX,
        WASM_GLOBAL_OBJECT_TYPE, WasmGlobalObject::kHeaderSize,
        "WebAssembly.Global");
    InstallFunc(isolate, global_proto, "valueOf",
                wasm::WebAssemblyGlobalValueOf, 0, NONE,
                SideEffectType::kHasNoSideEffect);
    InstallGetterSetter(isolate, global_proto, "value",
                        wasm::WebAssemblyGlobalGetValue,
                        wasm::WebAssemblyGlobalSetValue);
  }

  // Create the Exception object.
  {
    DirectHandle<JSFunction> tag_constructor = InstallConstructorFunc(
        isolate, webassembly, "Tag", Builtin::kWebAssemblyTag);
    SetupConstructor(isolate, tag_constructor,
                     Context::WASM_TAG_CONSTRUCTOR_INDEX, WASM_TAG_OBJECT_TYPE,
                     WasmTagObject::kHeaderSize, "WebAssembly.Tag");
    auto js_tag = WasmExceptionTag::New(isolate, 0);
    // Note the canonical_type_index is reset in WasmJs::Install s.t.
    // type_canonicalizer bookkeeping remains valid.
    static constexpr wasm::CanonicalTypeIndex kInitialCanonicalTypeIndex{0};
    DirectHandle<JSObject> js_tag_object = WasmTagObject::New(
        isolate, &kWasmExceptionTagSignature, kInitialCanonicalTypeIndex,
        js_tag, DirectHandle<WasmTrustedInstanceData>());
    native_context->set_wasm_js_tag(*js_tag_object);
    JSObject::AddProperty(isolate, webassembly, "JSTag", js_tag_object,
                          ro_attributes);
  }

  // Set up the runtime exception constructor.
  {
    DirectHandle<JSFunction> exception_constructor = InstallConstructorFunc(
        isolate, webassembly, "Exception", Builtin::kWebAssemblyException, 2);
    DirectHandle<JSObject> exception_proto = SetupConstructor(
        isolate, exception_constructor,
        Context::WASM_EXCEPTION_CONSTRUCTOR_INDEX, WASM_EXCEPTION_PACKAGE_TYPE,
        WasmExceptionPackage::kSize, "WebAssembly.Exception",
        WasmExceptionPackage::kInObjectFieldCount);
    InstallFunc(isolate, exception_proto, "getArg",
                wasm::WebAssemblyExceptionGetArg, 2);
    InstallFunc(isolate, exception_proto, "is", wasm::WebAssemblyExceptionIs,
                1);
    InstallGetter(isolate, exception_proto, "stack",
                  wasm::WebAssemblyExceptionGetStack);

    DirectHandle<Map> initial_map(exception_constructor->initial_map(),
                                  isolate);
    Map::EnsureDescriptorSlack(isolate, initial_map, 2);
    {
      Descriptor d =
          Descriptor::DataField(isolate, f->wasm_exception_tag_symbol(),
                                initial_map->GetInObjectPropertyOffset(
                                    WasmExceptionPackage::kTagIndex),
                                DONT_ENUM, Representation::Tagged(), true);
      initial_map->AppendDescriptor(isolate, &d);
    }
    {
      Descriptor d =
          Descriptor::DataField(isolate, f->wasm_exception_values_symbol(),
                                initial_map->GetInObjectPropertyOffset(
                                    WasmExceptionPackage::kValuesIndex),
                                DONT_ENUM, Representation::Tagged(), true);
      initial_map->AppendDescriptor(isolate, &d);
    }
  }

  // By default, make all exported functions an instance of {Function}.
  {
    DirectHandle<Map> function_map = isolate->function_without_prototype_map();
    native_context->set_wasm_exported_function_map(*function_map);
  }

  // Setup errors.
  {
    InstallError(isolate, webassembly, f->CompileError_string(),
                 Context::WASM_COMPILE_ERROR_FUNCTION_INDEX);
    InstallError(isolate, webassembly, f->LinkError_string(),
                 Context::WASM_LINK_ERROR_FUNCTION_INDEX);
    InstallError(isolate, webassembly, f->RuntimeError_string(),
                 Context::WASM_RUNTIME_ERROR_FUNCTION_INDEX);
  }
}

void WasmJs::InstallModule(Isolate* isolate,
                           DirectHandle<JSObject> webassembly) {
  DirectHandle<JSGlobalObject> global = isolate->global_object();
  DirectHandle<NativeContext> native_context(global->native_context(), isolate);

  DirectHandle<JSFunction> module_constructor;
  if (v8_flags.js_source_phase_imports) {
    // Check that this is a reinstallation of the Module object.
    DirectHandle<String> name = v8_str(isolate, "Module");
    CHECK(
        JSObject::HasRealNamedProperty(isolate, webassembly, name).ToChecked());
    // Reinstall the Module object with AbstractModuleSource as prototype.
    module_constructor =
        CreateConstructorFunc(isolate, name, Builtin::kWebAssemblyModule);
    // WebAssembly.Module is a subclass of %AbstractModuleSource%, hence
    // Object.GetPrototypeOf(WebAssembly.Module) === %AbstractModuleSource%.
    JSObject::ForceSetPrototype(
        isolate, module_constructor,
        direct_handle(native_context->abstract_module_source_function(),
                      isolate));
    JSObject::SetOwnPropertyIgnoreAttributes(webassembly, name,
                                             module_constructor, DONT_ENUM)
        .Assert();
  } else {
    module_constructor = InstallConstructorFunc(isolate, webassembly, "Module",
                                                Builtin::kWebAssemblyModule);
  }
  DirectHandle<JSObject> module_proto = SetupConstructor(
      isolate, module_constructor, Context::WASM_MODULE_CONSTRUCTOR_INDEX,
      WASM_MODULE_OBJECT_TYPE, WasmModuleObject::kHeaderSize,
      "WebAssembly.Module");
  if (v8_flags.js_source_phase_imports) {
    // Object.getPrototypeOf(WebAssembly.Module.prototype) ===
    // %AbstractModuleSource.prototype%.
    JSObject::ForceSetPrototype(
        isolate, module_proto,
        direct_handle(native_context->abstract_module_source_prototype(),
                      isolate));
  }

  InstallFunc(isolate, module_constructor, "imports",
              wasm::WebAssemblyModuleImports, 1, NONE,
              SideEffectType::kHasNoSideEffect);
  InstallFunc(isolate, module_constructor, "exports",
              wasm::WebAssemblyModuleExports, 1, NONE,
              SideEffectType::kHasNoSideEffect);
  InstallFunc(isolate, module_constructor, "customSections",
              wasm::WebAssemblyModuleCustomSections, 2, NONE,
              SideEffectType::kHasNoSideEffect);
}

// static
void WasmJs::Install(Isolate* isolate) {
  DirectHandle<JSGlobalObject> global = isolate->global_object();
  DirectHandle<NativeContext> native_context(global->native_context(), isolate);

  if (native_context->is_wasm_js_installed() != Smi::zero()) return;
  native_context->set_is_wasm_js_installed(Smi::FromInt(1));

  // We always use the WebAssembly object from the native context; as this code
  // is executed before any user code, this is expected to be the same as the
  // global "WebAssembly" property. But even later during execution we always
  // want to use this preallocated object instead of whatever user code
  // installed as "WebAssembly" property.
  DirectHandle<JSObject> webassembly(native_context->wasm_webassembly_object(),
                                     isolate);
  if (v8_flags.js_source_phase_imports) {
    // Reinstall the Module object with the experimental interface.
    InstallModule(isolate, webassembly);
  }

  // Expose the API on the global object if not in jitless mode (with more
  // subtleties).
  //
  // Even in interpreter-only mode, wasm currently still creates executable
  // memory at runtime. Unexpose wasm until this changes.
  // The correctness fuzzers are a special case: many of their test cases are
  // built by fetching a random property from the global object, and thus
  // the global object layout must not change between configs. That is why we
  // continue exposing wasm on correctness fuzzers even in jitless mode.
  // TODO(jgruber): Remove this once / if wasm can run without executable
  // memory.
  bool expose_wasm = !v8_flags.jitless ||
                     v8_flags.correctness_fuzzer_suppressions ||
                     v8_flags.wasm_jitless;
  if (expose_wasm) {
    DirectHandle<String> WebAssembly_string = v8_str(isolate, "WebAssembly");
    JSObject::AddProperty(isolate, global, WebAssembly_string, webassembly,
                          DONT_ENUM);
  }

  {
    // Reset the JSTag's canonical_type_index based on this Isolate's
    // type_canonicalizer.
    DirectHandle<WasmTagObject> js_tag_object(
        Cast<WasmTagObject>(native_context->wasm_js_tag()), isolate);
    js_tag_object->set_canonical_type_index(
        wasm::GetWasmEngine()
            ->type_canonicalizer()
            ->AddRecursiveGroup(&kWasmExceptionTagSignature)
            .index);
  }

  if (v8_flags.wasm_test_streaming) {
    isolate->set_wasm_streaming_callback(WasmStreamingCallbackForTesting);
  }

  if (isolate->wasm_streaming_callback() != nullptr) {
    InstallFunc(isolate, webassembly, "compileStreaming",
                WebAssemblyCompileStreaming, 1);
    InstallFunc(isolate, webassembly, "instantiateStreaming",
                WebAssemblyInstantiateStreaming, 1);
  }

  // The native_context is not set up completely yet. That's why we cannot use
  // {WasmEnabledFeatures::FromIsolate} and have to use
  // {WasmEnabledFeatures::FromFlags} instead.
  const auto enabled_features = wasm::WasmEnabledFeatures::FromFlags();

  if (enabled_features.has_memory_control()) {
    InstallMemoryControl(isolate, native_context, webassembly);
  }

  // Initialize and install JSPI feature.
  // JSPI is not supported by Wasm interpreter.
  if (!v8_flags.wasm_jitless) {
    CHECK(native_context->is_wasm_jspi_installed() == Smi::zero());
    isolate->WasmInitJSPIFeature();
    InstallJSPromiseIntegration(isolate, webassembly);
    native_context->set_is_wasm_jspi_installed(Smi::FromInt(1));
  }
}

// static
void WasmJs::InstallConditionalFeatures(Isolate* isolate,
                                        DirectHandle<NativeContext> context) {
  DirectHandle<JSObject> webassembly{context->wasm_webassembly_object(),
                                     isolate};
  if (!webassembly->map()->is_extensible()) return;
  if (webassembly->map()->is_access_check_needed()) return;

  // If you need to install some optional features, follow the pattern:
  //
  // if (isolate->IsMyWasmFeatureEnabled(context)) {
  //   DirectHandle<String> feature = isolate->factory()->...;
  //   if (!JSObject::HasRealNamedProperty(isolate, webassembly, feature)
  //            .FromMaybe(true)) {
  //     InstallFeature(isolate, webassembly);
  //   }
  // }

  // Install JSPI-related features.
  if (!v8_flags.wasm_jitless) {
    if (context->is_wasm_jspi_installed() == Smi::zero()) {
      isolate->WasmInitJSPIFeature();
      if (InstallJSPromiseIntegration(isolate, webassembly)) {
        context->set_is_wasm_jspi_installed(Smi::FromInt(1));
      }
    }
  }
}

// static
// Return true if this call results in JSPI being installed.
bool WasmJs::InstallJSPromiseIntegration(Isolate* isolate,
                                         DirectHandle<JSObject> webassembly) {
  DirectHandle<String> suspender_string = v8_str(isolate, "Suspender");
  if (JSObject::HasRealNamedProperty(isolate, webassembly, suspender_string)
          .FromMaybe(true)) {
    return false;
  }
  DirectHandle<String> suspending_string = v8_str(isolate, "Suspending");
  if (JSObject::HasRealNamedProperty(isolate, webassembly, suspending_string)
          .FromMaybe(true)) {
    return false;
  }
  DirectHandle<String> promising_string = v8_str(isolate, "promising");
  if (JSObject::HasRealNamedProperty(isolate, webassembly, promising_string)
          .FromMaybe(true)) {
    return false;
  }
  DirectHandle<String> suspend_error_string = v8_str(isolate, "SuspendError");
  if (JSObject::HasRealNamedProperty(isolate, webassembly, suspend_error_string)
          .FromMaybe(true)) {
    return false;
  }
  DirectHandle<JSFunction> suspending_constructor = InstallConstructorFunc(
      isolate, webassembly, "Suspending", Builtin::kWebAssemblySuspending);
  SetupConstructor(isolate, suspending_constructor,
                   Context::WASM_SUSPENDING_CONSTRUCTOR_INDEX,
                   WASM_SUSPENDING_OBJECT_TYPE,
                   WasmSuspendingObject::kHeaderSize, "WebAssembly.Suspending");
  InstallFunc(isolate, webassembly, "promising", WebAssemblyPromising, 1);
  InstallError(isolate, webassembly, isolate->factory()->SuspendError_string(),
               Context::WASM_SUSPEND_ERROR_FUNCTION_INDEX);
  return true;
}

void WasmJs::InstallMemoryControl(Isolate* isolate,
                                  DirectHandle<NativeContext> context,
                                  DirectHandle<JSObject> webassembly) {
  DCHECK(webassembly->map()->is_extensible());

  v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);
  v8::Local<v8::FunctionTemplate> constructor_template =
      v8::FunctionTemplate::New(v8_isolate,
                                wasm::WebAssemblyMemoryMapDescriptor);
  v8::Local<v8::ObjectTemplate> templ =
      constructor_template->InstanceTemplate();

  v8::Local<v8::Signature> signature =
      v8::Signature::New(v8_isolate, constructor_template);
  templ->Set(v8_isolate, "map",
             v8::FunctionTemplate::New(v8_isolate,
                                       wasm::WebAssemblyMemoryMapDescriptorMap,
                                       v8::Local<v8::Value>(), signature));
  templ->Set(v8_isolate, "unmap",
             v8::FunctionTemplate::New(
                 v8_isolate, wasm::WebAssemblyMemoryMapDescriptorUnmap,
                 v8::Local<v8::Value>(), signature));
  context->set_wasm_memory_map_descriptor_template(
      *Utils::OpenDirectHandle(*templ));

  DirectHandle<FunctionTemplateInfo> constructor_info =
      Utils::OpenDirectHandle(*constructor_template);
  DirectHandle<String> name = v8_str(isolate, "MemoryMapDescriptor");
  DirectHandle<JSFunction> constructor_handle =
      ApiNatives::InstantiateFunction(isolate, constructor_info, name)
          .ToHandleChecked();

  JSObject::AddProperty(isolate, webassembly, "MemoryMapDescriptor",
                        constructor_handle, DONT_ENUM);
  context->set_wasm_memory_map_descriptor_constructor(*constructor_handle);
}

namespace wasm {
CompileTimeImports CompileTimeImportsFromOptions(
    const v8::WasmModuleObject::CompileOptions& options) {
  CompileTimeImports result;
  using Builtins = v8::WasmModuleObject::CompileOptions::Builtins;
  if (options.builtins & Builtins::kJsString) {
    result.Add(CompileTimeImport::kJsString);
  }
  if (options.imported_string_constants_module != nullptr) {
    result.constants_module() = options.imported_string_constants_module;
    result.Add(CompileTimeImport::kStringConstants);
  }
  return result;
}
}  // namespace wasm

// static
CompileTimeImports WasmJs::CompileTimeImportsFromArgument(
    DirectHandle<Object> arg, Isolate* isolate,
    WasmEnabledFeatures enabled_features) {
  CompileTimeImports result;
  if (!IsJSReceiver(*arg)) return result;
  DirectHandle<JSReceiver> receiver = Cast<JSReceiver>(arg);

  // ==================== Builtins ====================
  DirectHandle<JSAny> builtins;
  ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate, builtins,
      Cast<JSAny>(JSReceiver::GetProperty(isolate, receiver, "builtins")), {});
  if (IsJSReceiver(*builtins)) {
    DirectHandle<Object> length_obj;
    ASSIGN_RETURN_ON_EXCEPTION_VALUE(
        isolate, length_obj,
        Object::GetLengthFromArrayLike(isolate, Cast<JSReceiver>(builtins)),
        {});
    double raw_length = Object::NumberValue(*length_obj);
    // Technically we should probably iterate up to 2^53-1 if {length_obj} says
    // so, but lengths above 2^32 probably don't happen in practice (and would
    // be very slow if they do), so just use a saturating to-uint32 conversion
    // for simplicity.
    uint32_t len = raw_length >= kMaxUInt32 ? kMaxUInt32
                                            : static_cast<uint32_t>(raw_length);
    for (uint32_t i = 0; i < len; i++) {
      LookupIterator it(isolate, builtins, i);
      Maybe<bool> maybe_found = JSReceiver::HasProperty(&it);
      MAYBE_RETURN(maybe_found, {});
      if (!maybe_found.FromJust()) continue;
      DirectHandle<Object> value;
      ASSIGN_RETURN_ON_EXCEPTION_VALUE(isolate, value, Object::GetProperty(&it),
                                       {});
      if (IsString(*value)) {
        Tagged<String> builtin = Cast<String>(*value);
        if (builtin->IsEqualTo("js-string")) {
          result.Add(CompileTimeImport::kJsString);
          continue;
        }
        if (enabled_features.has_imported_strings_utf8()) {
          if (builtin->IsEqualTo("text-encoder")) {
            result.Add(CompileTimeImport::kTextEncoder);
            continue;
          }
          if (builtin->IsEqualTo("text-decoder")) {
            result.Add(CompileTimeImport::kTextDecoder);
            continue;
          }
        }
        if (enabled_features.has_custom_descriptors() &&
            v8_flags.wasm_js_interop) {
          if (builtin->IsEqualTo("js-prototypes")) {
            result.Add(CompileTimeImport::kJsPrototypes);
            continue;
          }
        }
      }
    }
  }

  // ==================== String constants ====================
  DirectHandle<String> importedStringConstants =
      isolate->factory()->InternalizeUtf8String("importedStringConstants");
  if (JSReceiver::HasProperty(isolate, receiver, importedStringConstants)
          .FromMaybe(false)) {
    DirectHandle<Object> constants_value;
    ASSIGN_RETURN_ON_EXCEPTION_VALUE(
        isolate, constants_value,
        JSReceiver::GetProperty(isolate, receiver, importedStringConstants),
        {});
    if (IsString(*constants_value)) {
      ToUtf8Lossy(isolate, Cast<String>(constants_value),
                  result.constants_module());
      result.Add(CompileTimeImport::kStringConstants);
    }
  }

  return result;
}

namespace wasm {
// static
std::unique_ptr<WasmStreaming> StartStreamingForTesting(
    Isolate* isolate,
    std::shared_ptr<wasm::CompilationResultResolver> resolver) {
  return std::make_unique<WasmStreaming>(
      std::make_unique<WasmStreaming::WasmStreamingImpl>(
          isolate, "StartStreamingForTesting", CompileTimeImports{}, resolver));
}
}  // namespace wasm

#undef ASSIGN
#undef EXTRACT_THIS

}  // namespace internal
}  // namespace v8
