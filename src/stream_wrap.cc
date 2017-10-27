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

#include "stream_wrap.h"
#include "stream_base-inl.h"

#include "env-inl.h"
#include "handle_wrap.h"
#include "node_buffer.h"
#include "node_counters.h"
#include "pipe_wrap.h"
#include "req-wrap-inl.h"
#include "tcp_wrap.h"
#include "udp_wrap.h"
#include "util-inl.h"

#include <stdlib.h>  // abort()
#include <string.h>  // memcpy()
#include <limits.h>  // INT_MAX


namespace node {

using v8::Context;
using v8::EscapableHandleScope;
using v8::FunctionCallbackInfo;
using v8::FunctionTemplate;
using v8::HandleScope;
using v8::Integer;
using v8::Local;
using v8::Object;
using v8::Value;


void LibuvStreamWrap::Initialize(Local<Object> target,
                                 Local<Value> unused,
                                 Local<Context> context) {
  Environment* env = Environment::GetCurrent(context);

  auto is_construct_call_callback =
      [](const FunctionCallbackInfo<Value>& args) {
    CHECK(args.IsConstructCall());
    ClearWrap(args.This());
  };
  Local<FunctionTemplate> sw =
      FunctionTemplate::New(env->isolate(), is_construct_call_callback);
  sw->InstanceTemplate()->SetInternalFieldCount(1);
  Local<String> wrapString =
      FIXED_ONE_BYTE_STRING(env->isolate(), "ShutdownWrap");
  sw->SetClassName(wrapString);
  AsyncWrap::AddWrapMethods(env, sw);
  target->Set(wrapString, sw->GetFunction());

  Local<FunctionTemplate> ww =
      FunctionTemplate::New(env->isolate(), is_construct_call_callback);
  ww->InstanceTemplate()->SetInternalFieldCount(1);
  Local<String> writeWrapString =
      FIXED_ONE_BYTE_STRING(env->isolate(), "WriteWrap");
  ww->SetClassName(writeWrapString);
  AsyncWrap::AddWrapMethods(env, ww);
  target->Set(writeWrapString, ww->GetFunction());
  env->set_write_wrap_constructor_function(ww->GetFunction());
}


LibuvStreamWrap::LibuvStreamWrap(Environment* env,
                                 Local<Object> object,
                                 uv_stream_t* stream,
                                 AsyncWrap::ProviderType provider)
    : HandleWrap(env,
                 object,
                 reinterpret_cast<uv_handle_t*>(stream),
                 provider),
      StreamBase(env),
      stream_(stream) {
  set_after_write_cb({ OnAfterWriteImpl, this });
  set_alloc_cb({ OnAllocImpl, this });
  set_read_cb({ OnReadImpl, this });
}


void LibuvStreamWrap::AddMethods(Environment* env,
                                 v8::Local<v8::FunctionTemplate> target,
                                 int flags) {
  env->SetProtoMethod(target, "updateWriteQueueSize", UpdateWriteQueueSize);
  env->SetProtoMethod(target, "setBlocking", SetBlocking);
  StreamBase::AddMethods<LibuvStreamWrap>(env, target, flags);
}


int LibuvStreamWrap::GetFD() {
  int fd = -1;
#if !defined(_WIN32)
  if (stream() != nullptr)
    uv_fileno(reinterpret_cast<uv_handle_t*>(stream()), &fd);
#endif
  return fd;
}


bool LibuvStreamWrap::IsAlive() {
  return HandleWrap::IsAlive(this);
}


bool LibuvStreamWrap::IsClosing() {
  return uv_is_closing(reinterpret_cast<uv_handle_t*>(stream()));
}


void* LibuvStreamWrap::Cast() {
  return reinterpret_cast<void*>(this);
}


AsyncWrap* LibuvStreamWrap::GetAsyncWrap() {
  return static_cast<AsyncWrap*>(this);
}


bool LibuvStreamWrap::IsIPCPipe() {
  return is_named_pipe_ipc();
}


uint32_t LibuvStreamWrap::UpdateWriteQueueSize() {
  HandleScope scope(env()->isolate());
  uint32_t write_queue_size = stream()->write_queue_size;
  object()->Set(env()->context(),
                env()->write_queue_size_string(),
                Integer::NewFromUnsigned(env()->isolate(),
                                         write_queue_size)).FromJust();
  return write_queue_size;
}


int LibuvStreamWrap::ReadStart() {
  return uv_read_start(stream(), OnAlloc, OnRead);
}


int LibuvStreamWrap::ReadStop() {
  return uv_read_stop(stream());
}


void LibuvStreamWrap::OnAlloc(uv_handle_t* handle,
                         size_t suggested_size,
                         uv_buf_t* buf) {
  LibuvStreamWrap* wrap = static_cast<LibuvStreamWrap*>(handle->data);
  HandleScope scope(wrap->env()->isolate());
  Context::Scope context_scope(wrap->env()->context());

  CHECK_EQ(wrap->stream(), reinterpret_cast<uv_stream_t*>(handle));

  return static_cast<StreamBase*>(wrap)->OnAlloc(suggested_size, buf);
}


void LibuvStreamWrap::OnAllocImpl(size_t size, uv_buf_t* buf, void* ctx) {
  buf->base = node::Malloc(size);
  buf->len = size;
}


template <class WrapType, class UVType>
static Local<Object> AcceptHandle(Environment* env, LibuvStreamWrap* parent) {
  EscapableHandleScope scope(env->isolate());
  Local<Object> wrap_obj;
  UVType* handle;

  wrap_obj = WrapType::Instantiate(env, parent);
  if (wrap_obj.IsEmpty())
    return Local<Object>();

  WrapType* wrap;
  ASSIGN_OR_RETURN_UNWRAP(&wrap, wrap_obj, Local<Object>());
  handle = wrap->UVHandle();

  if (uv_accept(parent->stream(), reinterpret_cast<uv_stream_t*>(handle)))
    ABORT();

  return scope.Escape(wrap_obj);
}


void LibuvStreamWrap::OnReadImpl(ssize_t nread,
                            const uv_buf_t* buf,
                            uv_handle_type pending,
                            void* ctx) {
  LibuvStreamWrap* wrap = static_cast<LibuvStreamWrap*>(ctx);
  Environment* env = wrap->env();
  HandleScope handle_scope(env->isolate());
  Context::Scope context_scope(env->context());

  Local<Object> pending_obj;

  if (nread < 0)  {
    if (buf->base != nullptr)
      free(buf->base);
    wrap->EmitData(nread, Local<Object>(), pending_obj);
    return;
  }

  if (nread == 0) {
    if (buf->base != nullptr)
      free(buf->base);
    return;
  }

  CHECK_LE(static_cast<size_t>(nread), buf->len);
  char* base = node::Realloc(buf->base, nread);

  if (pending == UV_TCP) {
    pending_obj = AcceptHandle<TCPWrap, uv_tcp_t>(env, wrap);
  } else if (pending == UV_NAMED_PIPE) {
    pending_obj = AcceptHandle<PipeWrap, uv_pipe_t>(env, wrap);
  } else if (pending == UV_UDP) {
    pending_obj = AcceptHandle<UDPWrap, uv_udp_t>(env, wrap);
  } else {
    CHECK_EQ(pending, UV_UNKNOWN_HANDLE);
  }

  Local<Object> obj = Buffer::New(env, base, nread).ToLocalChecked();
  wrap->EmitData(nread, obj, pending_obj);
}


void LibuvStreamWrap::OnRead(uv_stream_t* handle,
                        ssize_t nread,
                        const uv_buf_t* buf) {
  LibuvStreamWrap* wrap = static_cast<LibuvStreamWrap*>(handle->data);
  HandleScope scope(wrap->env()->isolate());
  Context::Scope context_scope(wrap->env()->context());
  uv_handle_type type = UV_UNKNOWN_HANDLE;

  if (wrap->is_named_pipe_ipc() &&
      uv_pipe_pending_count(reinterpret_cast<uv_pipe_t*>(handle)) > 0) {
    type = uv_pipe_pending_type(reinterpret_cast<uv_pipe_t*>(handle));
  }

  // We should not be getting this callback if someone as already called
  // uv_close() on the handle.
  CHECK_EQ(wrap->persistent().IsEmpty(), false);

  if (nread > 0) {
    if (wrap->is_tcp()) {
      NODE_COUNT_NET_BYTES_RECV(nread);
    } else if (wrap->is_named_pipe()) {
      NODE_COUNT_PIPE_BYTES_RECV(nread);
    }
  }

  static_cast<StreamBase*>(wrap)->OnRead(nread, buf, type);
}


void LibuvStreamWrap::UpdateWriteQueueSize(
    const FunctionCallbackInfo<Value>& args) {
  LibuvStreamWrap* wrap;
  ASSIGN_OR_RETURN_UNWRAP(&wrap, args.Holder());

  uint32_t write_queue_size = wrap->UpdateWriteQueueSize();
  args.GetReturnValue().Set(write_queue_size);
}


void LibuvStreamWrap::SetBlocking(const FunctionCallbackInfo<Value>& args) {
  LibuvStreamWrap* wrap;
  ASSIGN_OR_RETURN_UNWRAP(&wrap, args.Holder());

  CHECK_GT(args.Length(), 0);
  if (!wrap->IsAlive())
    return args.GetReturnValue().Set(UV_EINVAL);

  bool enable = args[0]->IsTrue();
  args.GetReturnValue().Set(uv_stream_set_blocking(wrap->stream(), enable));
}


int LibuvStreamWrap::DoShutdown(ShutdownWrap* req_wrap) {
  int err;
  err = uv_shutdown(req_wrap->req(), stream(), AfterShutdown);
  req_wrap->Dispatched();
  return err;
}


void LibuvStreamWrap::AfterShutdown(uv_shutdown_t* req, int status) {
  ShutdownWrap* req_wrap = ShutdownWrap::from_req(req);
  CHECK_NE(req_wrap, nullptr);
  HandleScope scope(req_wrap->env()->isolate());
  Context::Scope context_scope(req_wrap->env()->context());
  req_wrap->Done(status);
}


// NOTE: Call to this function could change both `buf`'s and `count`'s
// values, shifting their base and decrementing their length. This is
// required in order to skip the data that was successfully written via
// uv_try_write().
int LibuvStreamWrap::DoTryWrite(uv_buf_t** bufs, size_t* count) {
  int err;
  size_t written;
  uv_buf_t* vbufs = *bufs;
  size_t vcount = *count;

  err = uv_try_write(stream(), vbufs, vcount);
  if (err == UV_ENOSYS || err == UV_EAGAIN)
    return 0;
  if (err < 0)
    return err;

  // Slice off the buffers: skip all written buffers and slice the one that
  // was partially written.
  written = err;
  for (; vcount > 0; vbufs++, vcount--) {
    // Slice
    if (vbufs[0].len > written) {
      vbufs[0].base += written;
      vbufs[0].len -= written;
      written = 0;
      break;

    // Discard
    } else {
      written -= vbufs[0].len;
    }
  }

  *bufs = vbufs;
  *count = vcount;

  return 0;
}


int LibuvStreamWrap::DoWrite(WriteWrap* w,
                        uv_buf_t* bufs,
                        size_t count,
                        uv_stream_t* send_handle) {
  int r;
  if (send_handle == nullptr) {
    r = uv_write(w->req(), stream(), bufs, count, AfterWrite);
  } else {
    r = uv_write2(w->req(), stream(), bufs, count, send_handle, AfterWrite);
  }

  if (!r) {
    size_t bytes = 0;
    for (size_t i = 0; i < count; i++)
      bytes += bufs[i].len;
    if (stream()->type == UV_TCP) {
      NODE_COUNT_NET_BYTES_SENT(bytes);
    } else if (stream()->type == UV_NAMED_PIPE) {
      NODE_COUNT_PIPE_BYTES_SENT(bytes);
    }
  }

  w->Dispatched();
  UpdateWriteQueueSize();

  return r;
}


void LibuvStreamWrap::AfterWrite(uv_write_t* req, int status) {
  WriteWrap* req_wrap = WriteWrap::from_req(req);
  CHECK_NE(req_wrap, nullptr);
  HandleScope scope(req_wrap->env()->isolate());
  Context::Scope context_scope(req_wrap->env()->context());
  req_wrap->Done(status);
}


void LibuvStreamWrap::OnAfterWriteImpl(WriteWrap* w, void* ctx) {
  LibuvStreamWrap* wrap = static_cast<LibuvStreamWrap*>(ctx);
  wrap->UpdateWriteQueueSize();
}

}  // namespace node

NODE_MODULE_CONTEXT_AWARE_BUILTIN(stream_wrap,
                                  node::LibuvStreamWrap::Initialize)
