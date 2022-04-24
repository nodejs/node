#include "node_wasm_web_api.h"

#include "memory_tracker-inl.h"
#include "node_errors.h"

namespace node {
namespace wasm_web_api {

using v8::ArrayBuffer;
using v8::ArrayBufferView;
using v8::BackingStore;
using v8::CompiledWasmModule;
using v8::Context;
using v8::Function;
using v8::FunctionCallbackInfo;
using v8::FunctionTemplate;
using v8::Local;
using v8::MaybeLocal;
using v8::Object;
using v8::OwnedBuffer;
using v8::String;
using v8::Value;
using v8::WasmStreaming;

Local<Function> WasmStreamingObject::Initialize(Environment* env) {
  Local<Function> templ = env->wasm_streaming_object_constructor();
  if (!templ.IsEmpty()) {
    return templ;
  }

  Local<FunctionTemplate> t = env->NewFunctionTemplate(New);
  t->Inherit(BaseObject::GetConstructorTemplate(env));
  t->InstanceTemplate()->SetInternalFieldCount(
      WasmStreamingObject::kInternalFieldCount);

  env->SetProtoMethod(t, "push", Push);
  env->SetProtoMethod(t, "finish", Finish);
  env->SetProtoMethod(t, "abort", Abort);
  env->SetProtoMethod(t, "setCompiledModuleBytes", SetCompiledModuleBytes);

  auto function = t->GetFunction(env->context()).ToLocalChecked();
  env->set_wasm_streaming_object_constructor(function);
  return function;
}

void WasmStreamingObject::RegisterExternalReferences(
    ExternalReferenceRegistry* registry) {
  registry->Register(Push);
  registry->Register(Finish);
  registry->Register(Abort);
  registry->Register(SetCompiledModuleBytes);
}

void WasmStreamingObject::MemoryInfo(MemoryTracker* tracker) const {
  // v8::WasmStreaming is opaque. We assume that the size of the WebAssembly
  // module that is being compiled is roughly what V8 allocates (as in, off by
  // only a small factor).
  tracker->TrackFieldWithSize("streaming", wasm_size_);
}

class WasmStreamingClient : public WasmStreaming::Client {
 public:
  explicit WasmStreamingClient(Environment* env) : env_(env) {}

 private:
  void OnModuleCompiled(CompiledWasmModule compiled_module) {
    env_->SetImmediateThreadsafe([compiled_module](Environment* env) mutable {
      OwnedBuffer owned = compiled_module.Serialize();
      if (owned.size == 0) {
        return;
      }

      std::shared_ptr<BackingStore> store =
          ArrayBuffer::NewBackingStore(env->isolate(), owned.size);
      unsigned char* dest = static_cast<unsigned char*>(store->Data());
      memcpy(dest, &owned.buffer, owned.size);
      Local<ArrayBuffer> ab = ArrayBuffer::New(env->isolate(), store);
      Local<String> url =
          String::NewFromUtf8(env->isolate(),
                              compiled_module.source_url().c_str(),
                              {},
                              compiled_module.source_url().size())
              .ToLocalChecked();

      Local<Value> args[] = {url, ab};
      env->wasm_streaming_compilation_callback()
          ->Call(
              env->context(), Undefined(env->isolate()), arraysize(args), args)
          .ToLocalChecked();
    });
  }

  Environment* env_;
};

MaybeLocal<Object> WasmStreamingObject::Create(
    Environment* env, std::shared_ptr<WasmStreaming> streaming) {
  Local<Function> ctor = Initialize(env);
  Local<Object> obj;
  if (!ctor->NewInstance(env->context(), 0, nullptr).ToLocal(&obj)) {
    return MaybeLocal<Object>();
  }

  CHECK(streaming);

  WasmStreamingObject* ptr = Unwrap<WasmStreamingObject>(obj);
  CHECK_NOT_NULL(ptr);
  ptr->streaming_ = streaming;
  ptr->wasm_size_ = 0;

  ptr->streaming_->SetClient(std::make_shared<WasmStreamingClient>(env));

  return obj;
}

void WasmStreamingObject::New(const FunctionCallbackInfo<Value>& args) {
  CHECK(args.IsConstructCall());
  Environment* env = Environment::GetCurrent(args);
  new WasmStreamingObject(env, args.This());
}

void WasmStreamingObject::Push(const FunctionCallbackInfo<Value>& args) {
  WasmStreamingObject* obj;
  ASSIGN_OR_RETURN_UNWRAP(&obj, args.Holder());
  CHECK(obj->streaming_);

  CHECK_EQ(args.Length(), 1);
  Local<Value> chunk = args[0];

  // The start of the memory section backing the ArrayBuffer(View), the offset
  // of the ArrayBuffer(View) within the memory section, and its size in bytes.
  const void* bytes;
  size_t offset;
  size_t size;

  if (LIKELY(chunk->IsArrayBufferView())) {
    Local<ArrayBufferView> view = chunk.As<ArrayBufferView>();
    bytes = view->Buffer()->GetBackingStore()->Data();
    offset = view->ByteOffset();
    size = view->ByteLength();
  } else if (LIKELY(chunk->IsArrayBuffer())) {
    Local<ArrayBuffer> buffer = chunk.As<ArrayBuffer>();
    bytes = buffer->GetBackingStore()->Data();
    offset = 0;
    size = buffer->ByteLength();
  } else {
    return node::THROW_ERR_INVALID_ARG_TYPE(
        Environment::GetCurrent(args),
        "chunk must be an ArrayBufferView or an ArrayBuffer");
  }

  // Forward the data to V8. Internally, V8 will make a copy.
  obj->streaming_->OnBytesReceived(static_cast<const uint8_t*>(bytes) + offset,
                                   size);
  obj->wasm_size_ += size;
}

void WasmStreamingObject::SetCompiledModuleBytes(
    const FunctionCallbackInfo<Value>& args) {
  WasmStreamingObject* obj;
  ASSIGN_OR_RETURN_UNWRAP(&obj, args.Holder());
  CHECK(obj->streaming_);

  CHECK_EQ(args.Length(), 1);
  Local<Value> chunk = args[0];

  // The start of the memory section backing the ArrayBuffer(View), the offset
  // of the ArrayBuffer(View) within the memory section, and its size in bytes.
  const void* bytes;
  size_t offset;
  size_t size;

  if (chunk->IsArrayBufferView()) {
    Local<ArrayBufferView> view = chunk.As<ArrayBufferView>();
    bytes = view->Buffer()->GetBackingStore()->Data();
    offset = view->ByteOffset();
    size = view->ByteLength();
  } else if (chunk->IsArrayBuffer()) {
    Local<ArrayBuffer> buffer = chunk.As<ArrayBuffer>();
    bytes = buffer->GetBackingStore()->Data();
    offset = 0;
    size = buffer->ByteLength();
  } else {
    return node::THROW_ERR_INVALID_ARG_TYPE(
        Environment::GetCurrent(args),
        "buffer must be an ArrayBufferView or an ArrayBuffer");
  }

  bool bytes_used = obj->streaming_->SetCompiledModuleBytes(
      static_cast<const uint8_t*>(bytes) + offset, size);
  if (bytes_used) {
    obj->wasm_size_ += size;
  }
  args.GetReturnValue().Set(bytes_used);
}

void WasmStreamingObject::Finish(const FunctionCallbackInfo<Value>& args) {
  WasmStreamingObject* obj;
  ASSIGN_OR_RETURN_UNWRAP(&obj, args.Holder());
  CHECK(obj->streaming_);

  CHECK_EQ(args.Length(), 0);
  obj->streaming_->Finish();
}

void WasmStreamingObject::Abort(const FunctionCallbackInfo<Value>& args) {
  WasmStreamingObject* obj;
  ASSIGN_OR_RETURN_UNWRAP(&obj, args.Holder());
  CHECK(obj->streaming_);

  CHECK_EQ(args.Length(), 1);
  obj->streaming_->Abort(args[0]);
}

void StartStreamingCompilation(const FunctionCallbackInfo<Value>& info) {
  // V8 passes an instance of v8::WasmStreaming to this callback, which we can
  // use to pass the WebAssembly module bytes to V8 as we receive them.
  // Unfortunately, our fetch() implementation is a JavaScript dependency, so it
  // is difficult to implement the required logic here. Instead, we create a
  // a WasmStreamingObject that encapsulates v8::WasmStreaming and that we can
  // pass to the JavaScript implementation. The JavaScript implementation can
  // then push() bytes from the Response and eventually either finish() or
  // abort() the operation.

  // Create the wrapper object.
  std::shared_ptr<WasmStreaming> streaming =
      WasmStreaming::Unpack(info.GetIsolate(), info.Data());
  Environment* env = Environment::GetCurrent(info);
  Local<Object> obj;
  if (!WasmStreamingObject::Create(env, streaming).ToLocal(&obj)) {
    // A JavaScript exception is pending. Let V8 deal with it.
    return;
  }

  // V8 always passes one argument to this callback.
  CHECK_EQ(info.Length(), 1);

  // Prepare the JavaScript implementation for invocation. We will pass the
  // WasmStreamingObject as the first argument, followed by the argument that we
  // received from V8, i.e., the first argument passed to compileStreaming (or
  // instantiateStreaming).
  Local<Function> impl = env->wasm_streaming_compilation_impl();
  CHECK(!impl.IsEmpty());
  Local<Value> args[] = {obj, info[0]};

  // Hand control to the JavaScript implementation. It should never throw an
  // error, but if it does, we leave it to the calling V8 code to handle that
  // gracefully. Otherwise, we assert that the JavaScript function does not
  // return anything.
  MaybeLocal<Value> maybe_ret =
      impl->Call(env->context(), info.This(), 2, args);
  Local<Value> ret;
  CHECK_IMPLIES(maybe_ret.ToLocal(&ret), ret->IsUndefined());
}

// Called once by JavaScript during initialization.
void SetImplementation(const FunctionCallbackInfo<Value>& info) {
  Environment* env = Environment::GetCurrent(info);
  env->set_wasm_streaming_compilation_impl(info[0].As<Function>());
  env->set_wasm_streaming_compilation_callback(info[1].As<Function>());
}

void Initialize(Local<Object> target,
                Local<Value>,
                Local<Context> context,
                void*) {
  Environment* env = Environment::GetCurrent(context);
  env->SetMethod(target, "setImplementation", SetImplementation);
}

void RegisterExternalReferences(ExternalReferenceRegistry* registry) {
  registry->Register(SetImplementation);
}

}  // namespace wasm_web_api
}  // namespace node

NODE_MODULE_CONTEXT_AWARE_INTERNAL(wasm_web_api, node::wasm_web_api::Initialize)
NODE_MODULE_EXTERNAL_REFERENCE(wasm_web_api,
                               node::wasm_web_api::RegisterExternalReferences)
