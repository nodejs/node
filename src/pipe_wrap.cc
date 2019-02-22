// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

#include "pipe_wrap.h"

#include "async_wrap.h"
#include "connection_wrap.h"
#include "env-inl.h"
#include "handle_wrap.h"
#include "node.h"
#include "node_buffer.h"
#include "connect_wrap.h"
#include "stream_base-inl.h"
#include "stream_wrap.h"
#include "util-inl.h"

namespace node {

using v8::Context;
using v8::EscapableHandleScope;
using v8::Function;
using v8::FunctionCallbackInfo;
using v8::FunctionTemplate;
using v8::HandleScope;
using v8::Int32;
using v8::Local;
using v8::MaybeLocal;
using v8::Object;
using v8::String;
using v8::Value;

using AsyncHooks = Environment::AsyncHooks;

MaybeLocal<Object> PipeWrap::Instantiate(Environment* env,
                                         AsyncWrap* parent,
                                         PipeWrap::SocketType type) {
  EscapableHandleScope handle_scope(env->isolate());
  AsyncHooks::DefaultTriggerAsyncIdScope trigger_scope(parent);
  CHECK_EQ(false, env->pipe_constructor_template().IsEmpty());
  Local<Function> constructor = env->pipe_constructor_template()
                                    ->GetFunction(env->context())
                                    .ToLocalChecked();
  CHECK_EQ(false, constructor.IsEmpty());
  Local<Value> type_value = Int32::New(env->isolate(), type);
  return handle_scope.EscapeMaybe(
      constructor->NewInstance(env->context(), 1, &type_value));
}


void PipeWrap::Initialize(Local<Object> target,
                          Local<Value> unused,
                          Local<Context> context,
                          void* priv) {
  Environment* env = Environment::GetCurrent(context);

  Local<FunctionTemplate> t = env->NewFunctionTemplate(New);
  Local<String> pipeString = FIXED_ONE_BYTE_STRING(env->isolate(), "Pipe");
  t->SetClassName(pipeString);
  t->InstanceTemplate()->SetInternalFieldCount(1);

  t->Inherit(LibuvStreamWrap::GetConstructorTemplate(env));

  env->SetProtoMethod(t, "bind", Bind);
  env->SetProtoMethod(t, "listen", Listen);
  env->SetProtoMethod(t, "connect", Connect);
  env->SetProtoMethod(t, "open", Open);

#ifdef _WIN32
  env->SetProtoMethod(t, "setPendingInstances", SetPendingInstances);
#endif

  env->SetProtoMethod(t, "fchmod", Fchmod);

  target->Set(env->context(),
              pipeString,
              t->GetFunction(env->context()).ToLocalChecked()).FromJust();
  env->set_pipe_constructor_template(t);

  // Create FunctionTemplate for PipeConnectWrap.
  auto cwt = BaseObject::MakeLazilyInitializedJSTemplate(env);
  cwt->Inherit(AsyncWrap::GetConstructorTemplate(env));
  Local<String> wrapString =
      FIXED_ONE_BYTE_STRING(env->isolate(), "PipeConnectWrap");
  cwt->SetClassName(wrapString);
  target->Set(env->context(),
              wrapString,
              cwt->GetFunction(env->context()).ToLocalChecked()).FromJust();

  // Define constants
  Local<Object> constants = Object::New(env->isolate());
  NODE_DEFINE_CONSTANT(constants, SOCKET);
  NODE_DEFINE_CONSTANT(constants, SERVER);
  NODE_DEFINE_CONSTANT(constants, IPC);
  NODE_DEFINE_CONSTANT(constants, UV_READABLE);
  NODE_DEFINE_CONSTANT(constants, UV_WRITABLE);
  target->Set(context,
              env->constants_string(),
              constants).FromJust();
}


void PipeWrap::New(const FunctionCallbackInfo<Value>& args) {
  // This constructor should not be exposed to public javascript.
  // Therefore we assert that we are not trying to call this as a
  // normal function.
  CHECK(args.IsConstructCall());
  CHECK(args[0]->IsInt32());
  Environment* env = Environment::GetCurrent(args);

  int type_value = args[0].As<Int32>()->Value();
  PipeWrap::SocketType type = static_cast<PipeWrap::SocketType>(type_value);

  bool ipc;
  ProviderType provider;
  switch (type) {
    case SOCKET:
      provider = PROVIDER_PIPEWRAP;
      ipc = false;
      break;
    case SERVER:
      provider = PROVIDER_PIPESERVERWRAP;
      ipc = false;
      break;
    case IPC:
      provider = PROVIDER_PIPEWRAP;
      ipc = true;
      break;
    default:
      UNREACHABLE();
  }

  new PipeWrap(env, args.This(), provider, ipc);
}


PipeWrap::PipeWrap(Environment* env,
                   Local<Object> object,
                   ProviderType provider,
                   bool ipc)
    : ConnectionWrap(env, object, provider) {
  int r = uv_pipe_init(env->event_loop(), &handle_, ipc);
  CHECK_EQ(r, 0);  // How do we proxy this error up to javascript?
                   // Suggestion: uv_pipe_init() returns void.
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
  CHECK(args[0]->IsInt32());
  int instances = args[0].As<Int32>()->Value();
  uv_pipe_pending_instances(&wrap->handle_, instances);
}
#endif


void PipeWrap::Fchmod(const v8::FunctionCallbackInfo<v8::Value>& args) {
  PipeWrap* wrap;
  ASSIGN_OR_RETURN_UNWRAP(&wrap, args.Holder());
  CHECK(args[0]->IsInt32());
  int mode = args[0].As<Int32>()->Value();
  int err = uv_pipe_chmod(reinterpret_cast<uv_pipe_t*>(&wrap->handle_),
                          mode);
  args.GetReturnValue().Set(err);
}


void PipeWrap::Listen(const FunctionCallbackInfo<Value>& args) {
  PipeWrap* wrap;
  ASSIGN_OR_RETURN_UNWRAP(&wrap, args.Holder());
  Environment* env = wrap->env();
  int backlog;
  if (!args[0]->Int32Value(env->context()).To(&backlog)) return;
  int err = uv_listen(reinterpret_cast<uv_stream_t*>(&wrap->handle_),
                      backlog,
                      OnConnection);
  args.GetReturnValue().Set(err);
}


void PipeWrap::Open(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  PipeWrap* wrap;
  ASSIGN_OR_RETURN_UNWRAP(&wrap, args.Holder());

  int fd;
  if (!args[0]->Int32Value(env->context()).To(&fd)) return;

  int err = uv_pipe_open(&wrap->handle_, fd);
  wrap->set_fd(fd);

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
  req_wrap->Dispatch(uv_pipe_connect,
                     &wrap->handle_,
                     *name,
                     AfterConnect);

  args.GetReturnValue().Set(0);  // uv_pipe_connect() doesn't return errors.
}


}  // namespace node

NODE_MODULE_CONTEXT_AWARE_INTERNAL(pipe_wrap, node::PipeWrap::Initialize)
