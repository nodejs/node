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
#include "connect_wrap.h"
#include "connection_wrap.h"
#include "env-inl.h"
#include "handle_wrap.h"
#include "node.h"
#include "node_buffer.h"
#include "node_errors.h"
#include "node_external_reference.h"
#include "stream_base-inl.h"
#include "stream_wrap.h"
#include "util-inl.h"

namespace node {

using v8::Context;
using v8::EscapableHandleScope;
using v8::Function;
using v8::FunctionCallbackInfo;
using v8::FunctionTemplate;
using v8::Int32;
using v8::Isolate;
using v8::Local;
using v8::MaybeLocal;
using v8::Object;
using v8::Value;

MaybeLocal<Object> PipeWrap::Instantiate(Environment* env,
                                         AsyncWrap* parent,
                                         PipeWrap::SocketType type) {
  EscapableHandleScope handle_scope(env->isolate());
  AsyncHooks::DefaultTriggerAsyncIdScope trigger_scope(parent);
  CHECK_EQ(false, env->pipe_constructor_template().IsEmpty());
  Local<Function> constructor;
  if (!env->pipe_constructor_template()
           ->GetFunction(env->context())
           .ToLocal(&constructor)) {
    return {};
  }
  Local<Value> type_value = Int32::New(env->isolate(), type);
  return handle_scope.EscapeMaybe(
      constructor->NewInstance(env->context(), 1, &type_value));
}

void PipeWrap::Initialize(Local<Object> target,
                          Local<Value> unused,
                          Local<Context> context,
                          void* priv) {
  Environment* env = Environment::GetCurrent(context);
  Isolate* isolate = env->isolate();

  Local<FunctionTemplate> t = NewFunctionTemplate(isolate, New);
  t->InstanceTemplate()->SetInternalFieldCount(PipeWrap::kInternalFieldCount);

  t->Inherit(LibuvStreamWrap::GetConstructorTemplate(env));

  SetProtoMethod(isolate, t, "bind", Bind);
  SetProtoMethod(isolate, t, "listen", Listen);
  SetProtoMethod(isolate, t, "connect", Connect);
  SetProtoMethod(isolate, t, "open", Open);
  SetProtoMethod(isolate, t, "watchPeerClose", WatchPeerClose);

#ifdef _WIN32
  SetProtoMethod(isolate, t, "setPendingInstances", SetPendingInstances);
#endif

  SetProtoMethod(isolate, t, "fchmod", Fchmod);

  SetConstructorFunction(context, target, "Pipe", t);
  env->set_pipe_constructor_template(t);

  // Create FunctionTemplate for PipeConnectWrap.
  auto cwt = AsyncWrap::MakeLazilyInitializedJSTemplate(env);
  SetConstructorFunction(context, target, "PipeConnectWrap", cwt);

  // Define constants
  Local<Object> constants = Object::New(env->isolate());
  NODE_DEFINE_CONSTANT(constants, SOCKET);
  NODE_DEFINE_CONSTANT(constants, SERVER);
  NODE_DEFINE_CONSTANT(constants, IPC);
  NODE_DEFINE_CONSTANT(constants, UV_READABLE);
  NODE_DEFINE_CONSTANT(constants, UV_WRITABLE);
  target->Set(context, env->constants_string(), constants).Check();
}

void PipeWrap::RegisterExternalReferences(ExternalReferenceRegistry* registry) {
  registry->Register(New);
  registry->Register(Bind);
  registry->Register(Listen);
  registry->Register(Connect);
  registry->Register(Open);
  registry->Register(WatchPeerClose);
#ifdef _WIN32
  registry->Register(SetPendingInstances);
#endif
  registry->Register(Fchmod);
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

PipeWrap::~PipeWrap() {
  peer_close_watching_ = false;
  peer_close_cb_.Reset();
}

void PipeWrap::Bind(const FunctionCallbackInfo<Value>& args) {
  PipeWrap* wrap;
  ASSIGN_OR_RETURN_UNWRAP(&wrap, args.This());
  node::Utf8Value name(args.GetIsolate(), args[0]);
  int err =
      uv_pipe_bind2(&wrap->handle_, *name, name.length(), UV_PIPE_NO_TRUNCATE);
  args.GetReturnValue().Set(err);
}

#ifdef _WIN32
void PipeWrap::SetPendingInstances(const FunctionCallbackInfo<Value>& args) {
  PipeWrap* wrap;
  ASSIGN_OR_RETURN_UNWRAP(&wrap, args.This());
  CHECK(args[0]->IsInt32());
  int instances = args[0].As<Int32>()->Value();
  uv_pipe_pending_instances(&wrap->handle_, instances);
}
#endif

void PipeWrap::Fchmod(const v8::FunctionCallbackInfo<v8::Value>& args) {
  PipeWrap* wrap;
  ASSIGN_OR_RETURN_UNWRAP(&wrap, args.This());
  CHECK(args[0]->IsInt32());
  int mode = args[0].As<Int32>()->Value();
  int err = uv_pipe_chmod(&wrap->handle_, mode);
  args.GetReturnValue().Set(err);
}

void PipeWrap::Listen(const FunctionCallbackInfo<Value>& args) {
  PipeWrap* wrap;
  ASSIGN_OR_RETURN_UNWRAP(&wrap, args.This());
  Environment* env = wrap->env();
  int backlog;
  if (!args[0]->Int32Value(env->context()).To(&backlog)) return;
  int err = uv_listen(
      reinterpret_cast<uv_stream_t*>(&wrap->handle_), backlog, OnConnection);
  args.GetReturnValue().Set(err);
}

void PipeWrap::Open(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  PipeWrap* wrap;
  ASSIGN_OR_RETURN_UNWRAP(&wrap, args.This());

  int fd;
  if (!args[0]->Int32Value(env->context()).To(&fd)) return;

  int err = uv_pipe_open(&wrap->handle_, fd);
  if (err == 0) wrap->set_fd(fd);

  args.GetReturnValue().Set(err);
}

void PipeWrap::WatchPeerClose(const FunctionCallbackInfo<Value>& args) {
  PipeWrap* wrap;
  ASSIGN_OR_RETURN_UNWRAP(&wrap, args.This());

  CHECK_GT(args.Length(), 0);
  CHECK(args[0]->IsBoolean());
  const bool enable = args[0].As<v8::Boolean>()->Value();

  // UnwatchPeerClose
  if (!enable) {
    if (!wrap->peer_close_watching_) {
      wrap->peer_close_cb_.Reset();
      return;
    }

    wrap->peer_close_watching_ = false;
    wrap->peer_close_cb_.Reset();
    uv_read_stop(wrap->stream());
    return;
  }

  if (!wrap->IsAlive()) {
    return;
  }

  if (wrap->peer_close_watching_) {
    return;
  }

  CHECK_GT(args.Length(), 1);
  CHECK(args[1]->IsFunction());

  Environment* env = wrap->env();
  Isolate* isolate = env->isolate();

  // Store the JS callback securely so it isn't garbage collected.
  wrap->peer_close_cb_.Reset(isolate, args[1].As<Function>());
  wrap->peer_close_watching_ = true;

  // Start reading to detect EOF/ECONNRESET from the peer.
  // We use our custom allocator and reader, ignoring actual data.
  int err = uv_read_start(wrap->stream(), PeerCloseAlloc, PeerCloseRead);
  if (err != 0) {
    wrap->peer_close_watching_ = false;
    wrap->peer_close_cb_.Reset();
  }
}

void PipeWrap::PeerCloseAlloc(uv_handle_t* handle,
                              size_t suggested_size,
                              uv_buf_t* buf) {
  // We only care about EOF, not the actual data.
  // Using a static 1-byte buffer avoids dynamic memory allocation overhead.
  static char scratch;
  *buf = uv_buf_init(&scratch, 1);
}

void PipeWrap::PeerCloseRead(uv_stream_t* stream,
                             ssize_t nread,
                             const uv_buf_t* buf) {
  PipeWrap* wrap = static_cast<PipeWrap*>(stream->data);
  if (wrap == nullptr || !wrap->peer_close_watching_) return;

  // Ignore actual data reads or EAGAIN (0). We only watch for disconnects.
  if (nread > 0 || nread == 0) return;

  // Wait specifically for EOF or connection reset (peer closed).
  if (nread != UV_EOF && nread != UV_ECONNRESET) return;

  // Peer has closed the connection. Stop reading immediately.
  wrap->peer_close_watching_ = false;
  uv_read_stop(stream);

  if (wrap->peer_close_cb_.IsEmpty()) return;
  Environment* env = wrap->env();
  Isolate* isolate = env->isolate();

  // Set up V8 context and handles to safely execute the JS callback.
  v8::HandleScope handle_scope(isolate);
  v8::Context::Scope context_scope(env->context());
  Local<Function> cb = wrap->peer_close_cb_.Get(isolate);
  // Reset before calling to prevent re-entrancy issues
  wrap->peer_close_cb_.Reset();

  errors::TryCatchScope try_catch(env);
  try_catch.SetVerbose(true);

  // MakeCallback properly tracks AsyncHooks context and flushes microtasks.
  wrap->MakeCallback(cb, 0, nullptr);
}

void PipeWrap::Connect(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  PipeWrap* wrap;
  ASSIGN_OR_RETURN_UNWRAP(&wrap, args.This());

  CHECK(args[0]->IsObject());
  CHECK(args[1]->IsString());

  Local<Object> req_wrap_obj = args[0].As<Object>();
  node::Utf8Value name(env->isolate(), args[1]);

  ERR_ACCESS_DENIED_IF_INSUFFICIENT_PERMISSIONS(
      env, permission::PermissionScope::kNet, name.ToStringView(), args);

  ConnectWrap* req_wrap =
      new ConnectWrap(env, req_wrap_obj, AsyncWrap::PROVIDER_PIPECONNECTWRAP);
  int err = req_wrap->Dispatch(uv_pipe_connect2,
                               &wrap->handle_,
                               *name,
                               name.length(),
                               UV_PIPE_NO_TRUNCATE,
                               AfterConnect);
  if (err) {
    delete req_wrap;
  } else {
    const char* path_type = (*name)[0] == '\0' ? "abstract socket" : "file";
    const char* pipe_path = (*name)[0] == '\0' ? (*name) + 1 : *name;
    TRACE_EVENT_NESTABLE_ASYNC_BEGIN2(TRACING_CATEGORY_NODE2(net, native),
                                      "connect",
                                      req_wrap,
                                      "path_type",
                                      path_type,
                                      "pipe_path",
                                      TRACE_STR_COPY(pipe_path));
  }

  args.GetReturnValue().Set(err);
}
}  // namespace node

NODE_BINDING_CONTEXT_AWARE_INTERNAL(pipe_wrap, node::PipeWrap::Initialize)
NODE_BINDING_EXTERNAL_REFERENCE(pipe_wrap,
                                node::PipeWrap::RegisterExternalReferences)
