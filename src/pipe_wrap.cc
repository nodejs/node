#include "pipe_wrap.h"

#include "async-wrap.h"
#include "connection_wrap.h"
#include "env.h"
#include "env-inl.h"
#include "handle_wrap.h"
#include "node.h"
#include "node_buffer.h"
#include "node_wrap.h"
#include "connect_wrap.h"
#include "stream_wrap.h"
#include "util-inl.h"
#include "util.h"

namespace node {

using v8::Context;
using v8::EscapableHandleScope;
using v8::External;
using v8::Function;
using v8::FunctionCallbackInfo;
using v8::FunctionTemplate;
using v8::HandleScope;
using v8::Local;
using v8::Object;
using v8::Value;


Local<Object> PipeWrap::Instantiate(Environment* env, AsyncWrap* parent) {
  EscapableHandleScope handle_scope(env->isolate());
  CHECK_EQ(false, env->pipe_constructor_template().IsEmpty());
  Local<Function> constructor = env->pipe_constructor_template()->GetFunction();
  CHECK_EQ(false, constructor.IsEmpty());
  Local<Value> ptr = External::New(env->isolate(), parent);
  Local<Object> instance =
      constructor->NewInstance(env->context(), 1, &ptr).ToLocalChecked();
  return handle_scope.Escape(instance);
}


void PipeWrap::Initialize(Local<Object> target,
                          Local<Value> unused,
                          Local<Context> context) {
  Environment* env = Environment::GetCurrent(context);

  Local<FunctionTemplate> t = env->NewFunctionTemplate(New);
  t->SetClassName(FIXED_ONE_BYTE_STRING(env->isolate(), "Pipe"));
  t->InstanceTemplate()->SetInternalFieldCount(1);

  env->SetProtoMethod(t, "close", HandleWrap::Close);
  env->SetProtoMethod(t, "unref", HandleWrap::Unref);
  env->SetProtoMethod(t, "ref", HandleWrap::Ref);
  env->SetProtoMethod(t, "hasRef", HandleWrap::HasRef);

  StreamWrap::AddMethods(env, t);

  env->SetProtoMethod(t, "bind", Bind);
  env->SetProtoMethod(t, "listen", Listen);
  env->SetProtoMethod(t, "connect", Connect);
  env->SetProtoMethod(t, "open", Open);

#ifdef _WIN32
  env->SetProtoMethod(t, "setPendingInstances", SetPendingInstances);
#endif

  target->Set(FIXED_ONE_BYTE_STRING(env->isolate(), "Pipe"), t->GetFunction());
  env->set_pipe_constructor_template(t);

  // Create FunctionTemplate for PipeConnectWrap.
  auto constructor = [](const FunctionCallbackInfo<Value>& args) {
    CHECK(args.IsConstructCall());
  };
  auto cwt = FunctionTemplate::New(env->isolate(), constructor);
  cwt->InstanceTemplate()->SetInternalFieldCount(1);
  cwt->SetClassName(FIXED_ONE_BYTE_STRING(env->isolate(), "PipeConnectWrap"));
  target->Set(FIXED_ONE_BYTE_STRING(env->isolate(), "PipeConnectWrap"),
              cwt->GetFunction());
}


void PipeWrap::New(const FunctionCallbackInfo<Value>& args) {
  // This constructor should not be exposed to public javascript.
  // Therefore we assert that we are not trying to call this as a
  // normal function.
  CHECK(args.IsConstructCall());
  Environment* env = Environment::GetCurrent(args);
  if (args[0]->IsExternal()) {
    void* ptr = args[0].As<External>()->Value();
    new PipeWrap(env, args.This(), false, static_cast<AsyncWrap*>(ptr));
  } else {
    new PipeWrap(env, args.This(), args[0]->IsTrue(), nullptr);
  }
}


PipeWrap::PipeWrap(Environment* env,
                   Local<Object> object,
                   bool ipc,
                   AsyncWrap* parent)
    : ConnectionWrap(env,
                     object,
                     AsyncWrap::PROVIDER_PIPEWRAP,
                     parent) {
  int r = uv_pipe_init(env->event_loop(), &handle_, ipc);
  CHECK_EQ(r, 0);  // How do we proxy this error up to javascript?
                   // Suggestion: uv_pipe_init() returns void.
  UpdateWriteQueueSize();
}


void PipeWrap::Bind(const FunctionCallbackInfo<Value>& args) {
  PipeWrap* wrap;
  ASSIGN_OR_RETURN_UNWRAP(&wrap, args.Holder());
  node::Utf8Value name(args.GetIsolate(), args[0]);
  int err = uv_pipe_bind(&wrap->handle_, *name);
  args.GetReturnValue().Set(err);
}


#ifdef _WIN32
void PipeWrap::SetPendingInstances(const FunctionCallbackInfo<Value>& args) {
  PipeWrap* wrap;
  ASSIGN_OR_RETURN_UNWRAP(&wrap, args.Holder());
  int instances = args[0]->Int32Value();
  uv_pipe_pending_instances(&wrap->handle_, instances);
}
#endif


void PipeWrap::Listen(const FunctionCallbackInfo<Value>& args) {
  PipeWrap* wrap;
  ASSIGN_OR_RETURN_UNWRAP(&wrap, args.Holder());
  int backlog = args[0]->Int32Value();
  int err = uv_listen(reinterpret_cast<uv_stream_t*>(&wrap->handle_),
                      backlog,
                      OnConnection);
  args.GetReturnValue().Set(err);
}


void PipeWrap::Open(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  PipeWrap* wrap;
  ASSIGN_OR_RETURN_UNWRAP(&wrap, args.Holder());

  int fd = args[0]->Int32Value();

  int err = uv_pipe_open(&wrap->handle_, fd);

  if (err != 0)
    env->isolate()->ThrowException(UVException(err, "uv_pipe_open"));
}


void PipeWrap::Connect(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  PipeWrap* wrap;
  ASSIGN_OR_RETURN_UNWRAP(&wrap, args.Holder());

  CHECK(args[0]->IsObject());
  CHECK(args[1]->IsString());

  Local<Object> req_wrap_obj = args[0].As<Object>();
  node::Utf8Value name(env->isolate(), args[1]);

  ConnectWrap* req_wrap =
      new ConnectWrap(env, req_wrap_obj, AsyncWrap::PROVIDER_PIPECONNECTWRAP);
  uv_pipe_connect(req_wrap->req(),
                  &wrap->handle_,
                  *name,
                  AfterConnect);
  req_wrap->Dispatched();

  args.GetReturnValue().Set(0);  // uv_pipe_connect() doesn't return errors.
}


}  // namespace node

NODE_MODULE_CONTEXT_AWARE_BUILTIN(pipe_wrap, node::PipeWrap::Initialize)
