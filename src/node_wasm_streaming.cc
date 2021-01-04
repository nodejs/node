#include "node_buffer.h"
#include "node_errors.h"
#include "node_external_reference.h"
#include "node_wasm_streaming.h"

#include "env-inl.h"

namespace node {

using errors::TryCatchScope;
using v8::ArrayBuffer;
using v8::Context;
using v8::DataView;
using v8::Function;
using v8::FunctionCallbackInfo;
using v8::FunctionTemplate;
using v8::Global;
using v8::HandleScope;
using v8::Isolate;
using v8::Local;
using v8::Object;
using v8::ObjectTemplate;
using v8::Value;

namespace WasmStreaming {
namespace {

class WasmStreaming final : public BaseObject {
 public:
  enum InternalFields {
    kSlot = BaseObject::kSlot,
    kCompiledModuleBytes = BaseObject::kInternalFieldCount,
    kInternalFieldCount
  };

  WasmStreaming(Environment* env,
                Local<Object> wrap,
                std::shared_ptr<v8::WasmStreaming> wasm_streaming);

  static void OnBytesReceived(const FunctionCallbackInfo<Value>& args);
  static void Finish(const FunctionCallbackInfo<Value>& args);
  static void Abort(const FunctionCallbackInfo<Value>& args);
  static void SetCompiledModuleBytes(const FunctionCallbackInfo<Value>& args);
  static void SetUrl(const FunctionCallbackInfo<Value>& args);

  SET_NO_MEMORY_INFO()
  SET_MEMORY_INFO_NAME(WasmStreaming)
  SET_SELF_SIZE(WasmStreaming)

 private:
  class OnModuleCompiledCallbackReg final : public v8::Task {
   public:
    OnModuleCompiledCallbackReg(Environment* env, Local<Function> callback);
    ~OnModuleCompiledCallbackReg();
    static void Invoke(std::unique_ptr<OnModuleCompiledCallbackReg> reg,
                       v8::OwnedBuffer compiledModuleData);
    void Run() override;

   private:
    std::shared_ptr<v8::TaskRunner> runner_;
    v8::OwnedBuffer compiledModuleData_;
    Environment* env_;
    Global<Function> callback_;
    async_context async_context_;
  };

  class Client final : public v8::WasmStreaming::Client {
   public:
    ~Client() override {
      // Must support being deleted from any thread.
      //
      // Invoke with dummy zero-sized buffer if no compiled module was
      // produced.  Could happend if Module was GC-ed before top tier
      // compilation completes.  Takes care of reg_; has to be destroyed
      // on the main thread.
      OnModuleCompiledCallbackReg::Invoke(std::move(reg_), {});
    }
    // Passes the fully compiled module to the client.
    void OnModuleCompiled(v8::CompiledWasmModule module) override {
      OnModuleCompiledCallbackReg::Invoke(std::move(reg_), module.Serialize());
    }
    void InstallCallback(std::unique_ptr<OnModuleCompiledCallbackReg> reg) {
      reg_ = std::move(reg);
    }
   private:
    std::unique_ptr<OnModuleCompiledCallbackReg> reg_;
  };

 private:
  std::shared_ptr<v8::WasmStreaming> wasm_streaming_;
  std::shared_ptr<Client> client_;

  void Reset() {
    wasm_streaming_.reset();
    client_.reset();

    Isolate* isolate = env()->isolate();
    object()->SetInternalField(kCompiledModuleBytes, v8::Undefined(isolate));
  }
};

WasmStreaming::WasmStreaming(Environment* env,
                             Local<Object> wrap,
                             std::shared_ptr<v8::WasmStreaming> wasm_streaming)
  : BaseObject(env, wrap), wasm_streaming_(wasm_streaming) {
  client_ = std::make_shared<Client>();
  wasm_streaming_->SetClient(client_);
  MakeWeak();
}

//  Pass a new chunk of bytes to WebAssembly streaming compilation.
void WasmStreaming::OnBytesReceived(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  WasmStreaming* ws;
  ASSIGN_OR_RETURN_UNWRAP(&ws, args.Holder());

  if (args.Length() == 0 || !args[0]->IsArrayBufferView()) {
    env->ThrowError("First argument must be an ArrayBufferView");
    return;
  }

  auto const* p = reinterpret_cast<uint8_t *>(Buffer::Data(args[0]));
  if (ws->wasm_streaming_)
    ws->wasm_streaming_->OnBytesReceived(p, Buffer::Length(args[0]));
    // (makes a copy)
}

// Finish should be called after all received bytes were passed to
// .onBytesReceived to tell V8 that there will be no more bytes.
//
// Optionally accepts a callback function to be invoked when compilation
// completes.  The callback receive compiled module bytes.
void WasmStreaming::Finish(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  WasmStreaming* ws;
  ASSIGN_OR_RETURN_UNWRAP(&ws, args.Holder());

  if (args.Length() != 0) {
    if (!args[0]->IsFunction()) {
      env->ThrowError("First argument must be a function");
      return;
    }
    if (ws->client_) {
      auto reg = std::make_unique<OnModuleCompiledCallbackReg>(
          env, args[0].As<Function>());
      ws->client_->InstallCallback(std::move(reg));
    }
  }

  if (ws->wasm_streaming_)
    ws->wasm_streaming_->Finish();

  ws->Reset();
}

// Abort streaming compilation with exception.
void WasmStreaming::Abort(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  WasmStreaming* ws;
  ASSIGN_OR_RETURN_UNWRAP(&ws, args.Holder());

  if (args.Length() == 0) {
    env->ThrowError("No arguments provided");
    return;
  }

  if (ws->wasm_streaming_)
    ws->wasm_streaming_->Abort(args[0]);

  ws->Reset();
}

// Passes previously compiled module bytes. This must be called before
// .onBytesReceived, .finish, or .abort. Returns true if the module bytes
// can be used, false otherwise.
void WasmStreaming::SetCompiledModuleBytes(
                                      const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  Isolate* isolate = env->isolate();

  WasmStreaming* ws;
  ASSIGN_OR_RETURN_UNWRAP(&ws, args.Holder());

  if (args.Length() == 0 || !args[0]->IsArrayBufferView()) {
    env->ThrowError("First argument must be an ArrayBufferView");
    return;
  }

  if (ws->wasm_streaming_) {
    // "If SetCompiledModuleBytes returns true, the buffer must remain
    // valid until either Finish or Abort completes."
    // Compiled module could be quite large therefore making a copy has
    // a performance impact.  Not making a copy, hence TOCTOU problem is
    // possible.  We can live with that since it is only used internally.
    ws->object()->SetInternalField(kCompiledModuleBytes, args[0]);

    auto const* p = reinterpret_cast<uint8_t *>(Buffer::Data(args[0]));

    bool res =
      ws->wasm_streaming_->SetCompiledModuleBytes(p, Buffer::Length(args[0]));
    args.GetReturnValue().Set(res ? v8::True(isolate) : v8::False(isolate));
  }
}

// Sets the source URL for the Script object. This must be called
// before .finish.
void WasmStreaming::SetUrl(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  WasmStreaming* ws;
  ASSIGN_OR_RETURN_UNWRAP(&ws, args.Holder());

  if (args.Length() == 0 || !args[0]->IsString()) {
    env->ThrowError("First argument must be a string");
    return;
  }

  Utf8Value url(args.GetIsolate(), args[0]);
  if (ws->wasm_streaming_)
    ws->wasm_streaming_->SetUrl(url.out(), url.length());  // makes a copy
}

WasmStreaming::OnModuleCompiledCallbackReg::OnModuleCompiledCallbackReg(
    Environment* env, Local<Function> callback)
  : runner_(env->isolate_data()->platform()->
        GetForegroundTaskRunner(env->isolate())),
    env_(env), callback_(env->isolate(), callback),
    async_context_(EmitAsyncInit(env->isolate(), callback,
                                 "wait_for_wasm_compiled_module_bytes")) {
}

WasmStreaming::OnModuleCompiledCallbackReg::~OnModuleCompiledCallbackReg() {
  EmitAsyncDestroy(env_, async_context_);
}

void WasmStreaming::OnModuleCompiledCallbackReg::Invoke(
    std::unique_ptr<WasmStreaming::OnModuleCompiledCallbackReg> p,
    v8::OwnedBuffer compiledModuleData) {

  if (!p) return;

  p->compiledModuleData_ = std::move(compiledModuleData);

  auto* runner = p->runner_.get();
  runner->PostTask(std::move(p));
}

void WasmStreaming::OnModuleCompiledCallbackReg::Run() {
  Isolate* isolate = env_->isolate();
  HandleScope handle_scope(isolate);
  Context::Scope context_scope(env_->context());

  auto &data = compiledModuleData_;

  auto bs = ArrayBuffer::NewBackingStore(
      const_cast<uint8_t*>(data.buffer.release()), data.size,
      [](void* p, size_t length, void* deleter_data) {
        delete [] static_cast<uint8_t*>(p);
      }, nullptr);

  Local<Value> args[] = {DataView::New(
    ArrayBuffer::New(isolate, std::move(bs)), 0, data.size)};

  Local<Function> cb = callback_.Get(isolate);
  MakeCallback(isolate, cb, cb, arraysize(args), args,
               async_context_);
}

// Registers a JS callback to be invoked by WebAssembly streaming
// functions (e.g. .compileStreaming).  The callback receives the
// resolved value of the Promise passed to .compileStreaming (aka
// "Response") and a WasmStreaming object.  The callback should submit
// WASM data by calling .onBytesReceived on the WASM streaming object
// followed by .Finish.
void SetCallback(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  if (args.Length() == 0 || !args[0]->IsFunction())
    env->ThrowError("First argument must be a function");
  else
    env->set_wasm_streaming_callback(args[0].As<Function>());
}

void Initialize(Local<Object> target,
                Local<Value> unused,
                Local<Context> context,
                void* priv) {
  Environment* env = Environment::GetCurrent(context);
  Isolate* isolate = env->isolate();

  env->SetMethod(target, "setCallback", SetCallback);

  Local<FunctionTemplate> wasm_streaming = FunctionTemplate::New(isolate);
  auto classname = FIXED_ONE_BYTE_STRING(env->isolate(), "WasmStreaming");
  wasm_streaming->SetClassName(classname);

  env->SetProtoMethod(wasm_streaming, "onBytesReceived",
                      WasmStreaming::OnBytesReceived);
  env->SetProtoMethod(wasm_streaming, "finish",
                      WasmStreaming::Finish);
  env->SetProtoMethod(wasm_streaming, "abort",
                      WasmStreaming::Abort);
  env->SetProtoMethod(wasm_streaming, "setCompiledModuleBytes",
                      WasmStreaming::SetCompiledModuleBytes);
  env->SetProtoMethod(wasm_streaming, "setUrl",
                      WasmStreaming::SetUrl);

  Local<ObjectTemplate> wasm_streaming_tmpl =
    wasm_streaming->InstanceTemplate();
  wasm_streaming_tmpl->SetInternalFieldCount(
    WasmStreaming::kInternalFieldCount);

  DCHECK(env->wasm_streaming_instance_template().IsEmpty());
  env->set_wasm_streaming_instance_template(wasm_streaming_tmpl);
}

}  // anonymous namespace

// Implements streaming for WebAssembly.compileStreaming (V8 callback)
// by forwarding the call to a user-registered JS function.
void WasmStreamingCallback(const FunctionCallbackInfo<Value>& args) {
  Isolate* isolate = args.GetIsolate();
  Environment* env = Environment::GetCurrent(isolate);
  CHECK_NOT_NULL(env);
  auto wasm_streaming = v8::WasmStreaming::Unpack(isolate, args.Data());
  DCHECK(wasm_streaming);
  Local<Function> cb = env->wasm_streaming_callback();
  TryCatchScope try_catch(env);
  if (cb.IsEmpty()) {
    env->ThrowError("WASM streaming not ready");
  } else {
    auto tmpl = env->wasm_streaming_instance_template();
    DCHECK(!tmpl.IsEmpty());
    Local<Object> wasm_streaming_object;
    if (tmpl->NewInstance(env->context()).ToLocal(&wasm_streaming_object)) {
      new WasmStreaming(env, wasm_streaming_object, wasm_streaming);
      // cb(Response, WasmStreaming)
      Local<Value> cb_args[] = {args[0], wasm_streaming_object};
      USE(cb->Call(env->context(), Undefined(isolate),
                   arraysize(cb_args), cb_args));
    }
  }
  if (try_catch.HasCaught() && !try_catch.HasTerminated())
    wasm_streaming->Abort(try_catch.Exception());
}

void RegisterExternalReferences(ExternalReferenceRegistry* registry) {
  registry->Register(SetCallback);
  registry->Register(WasmStreaming::OnBytesReceived);
  registry->Register(WasmStreaming::Finish);
  registry->Register(WasmStreaming::Abort);
  registry->Register(WasmStreaming::SetCompiledModuleBytes);
  registry->Register(WasmStreaming::SetUrl);
}

}  // namespace WasmStreaming
}  // namespace node

NODE_MODULE_CONTEXT_AWARE_INTERNAL(wasm_streaming,
                                   node::WasmStreaming::Initialize)
NODE_MODULE_EXTERNAL_REFERENCE(wasm_streaming,
                               node::WasmStreaming::RegisterExternalReferences)
