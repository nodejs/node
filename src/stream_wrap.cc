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
#include "env-inl.h"
#include "env.h"
#include "handle_wrap.h"
#include "node_buffer.h"
#include "node_counters.h"
#include "pipe_wrap.h"
#include "req_wrap.h"
#include "tcp_wrap.h"
#include "udp_wrap.h"
#include "util.h"
#include "util-inl.h"

#include <stdlib.h>  // abort()
#include <string.h>  // memcpy()
#include <limits.h>  // INT_MAX


namespace node {

using v8::Array;
using v8::Context;
using v8::EscapableHandleScope;
using v8::FunctionCallbackInfo;
using v8::FunctionTemplate;
using v8::Handle;
using v8::HandleScope;
using v8::Integer;
using v8::Local;
using v8::Number;
using v8::Object;
using v8::PropertyCallbackInfo;
using v8::String;
using v8::True;
using v8::Undefined;
using v8::Value;


void StreamWrap::Initialize(Handle<Object> target,
                         Handle<Value> unused,
                         Handle<Context> context) {
  Environment* env = Environment::GetCurrent(context);

  Local<FunctionTemplate> sw =
      FunctionTemplate::New(env->isolate(), ShutdownWrap::NewShutdownWrap);
  sw->InstanceTemplate()->SetInternalFieldCount(1);
  sw->SetClassName(FIXED_ONE_BYTE_STRING(env->isolate(), "ShutdownWrap"));
  target->Set(FIXED_ONE_BYTE_STRING(env->isolate(), "ShutdownWrap"),
              sw->GetFunction());

  Local<FunctionTemplate> ww =
      FunctionTemplate::New(env->isolate(), WriteWrap::NewWriteWrap);
  ww->InstanceTemplate()->SetInternalFieldCount(1);
  ww->SetClassName(FIXED_ONE_BYTE_STRING(env->isolate(), "WriteWrap"));
  target->Set(FIXED_ONE_BYTE_STRING(env->isolate(), "WriteWrap"),
              ww->GetFunction());
}


StreamWrap::StreamWrap(Environment* env,
                       Local<Object> object,
                       uv_stream_t* stream,
                       AsyncWrap::ProviderType provider,
                       AsyncWrap* parent)
    : HandleWrap(env,
                 object,
                 reinterpret_cast<uv_handle_t*>(stream),
                 provider,
                 parent),
      stream_(stream),
      default_callbacks_(this),
      callbacks_(&default_callbacks_),
      callbacks_gc_(false) {
}


void StreamWrap::GetFD(Local<String>, const PropertyCallbackInfo<Value>& args) {
#if !defined(_WIN32)
  Environment* env = Environment::GetCurrent(args.GetIsolate());
  HandleScope scope(env->isolate());
  StreamWrap* wrap = Unwrap<StreamWrap>(args.Holder());
  int fd = -1;
  if (wrap != NULL && wrap->stream() != NULL) {
    fd = wrap->stream()->io_watcher.fd;
  }
  args.GetReturnValue().Set(fd);
#endif
}


void StreamWrap::UpdateWriteQueueSize() {
  HandleScope scope(env()->isolate());
  Local<Integer> write_queue_size =
      Integer::NewFromUnsigned(env()->isolate(), stream()->write_queue_size);
  object()->Set(env()->write_queue_size_string(), write_queue_size);
}


void StreamWrap::ReadStart(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args.GetIsolate());
  HandleScope scope(env->isolate());

  StreamWrap* wrap = Unwrap<StreamWrap>(args.Holder());

  int err = uv_read_start(wrap->stream(), OnAlloc, OnRead);

  args.GetReturnValue().Set(err);
}


void StreamWrap::ReadStop(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args.GetIsolate());
  HandleScope scope(env->isolate());

  StreamWrap* wrap = Unwrap<StreamWrap>(args.Holder());

  int err = uv_read_stop(wrap->stream());
  args.GetReturnValue().Set(err);
}


void StreamWrap::OnAlloc(uv_handle_t* handle,
                         size_t suggested_size,
                         uv_buf_t* buf) {
  StreamWrap* wrap = static_cast<StreamWrap*>(handle->data);
  assert(wrap->stream() == reinterpret_cast<uv_stream_t*>(handle));
  wrap->callbacks()->DoAlloc(handle, suggested_size, buf);
}


template <class WrapType, class UVType>
static Local<Object> AcceptHandle(Environment* env,
                                  uv_stream_t* pipe,
                                  AsyncWrap* parent) {
  EscapableHandleScope scope(env->isolate());
  Local<Object> wrap_obj;
  UVType* handle;

  wrap_obj = WrapType::Instantiate(env, parent);
  if (wrap_obj.IsEmpty())
    return Local<Object>();

  WrapType* wrap = Unwrap<WrapType>(wrap_obj);
  handle = wrap->UVHandle();

  if (uv_accept(pipe, reinterpret_cast<uv_stream_t*>(handle)))
    abort();

  return scope.Escape(wrap_obj);
}


void StreamWrap::OnReadCommon(uv_stream_t* handle,
                              ssize_t nread,
                              const uv_buf_t* buf,
                              uv_handle_type pending) {
  StreamWrap* wrap = static_cast<StreamWrap*>(handle->data);

  // We should not be getting this callback if someone as already called
  // uv_close() on the handle.
  assert(wrap->persistent().IsEmpty() == false);

  if (nread > 0) {
    if (wrap->is_tcp()) {
      NODE_COUNT_NET_BYTES_RECV(nread);
    } else if (wrap->is_named_pipe()) {
      NODE_COUNT_PIPE_BYTES_RECV(nread);
    }
  }

  wrap->callbacks()->DoRead(handle, nread, buf, pending);
}


void StreamWrap::OnRead(uv_stream_t* handle,
                        ssize_t nread,
                        const uv_buf_t* buf) {
  StreamWrap* wrap = static_cast<StreamWrap*>(handle->data);
  uv_handle_type type = UV_UNKNOWN_HANDLE;

  if (wrap->is_named_pipe_ipc() &&
      uv_pipe_pending_count(reinterpret_cast<uv_pipe_t*>(handle)) > 0) {
    type = uv_pipe_pending_type(reinterpret_cast<uv_pipe_t*>(handle));
  }

  OnReadCommon(handle, nread, buf, type);
}


size_t StreamWrap::WriteBuffer(Handle<Value> val, uv_buf_t* buf) {
  assert(Buffer::HasInstance(val));

  // Simple non-writev case
  buf->base = Buffer::Data(val);
  buf->len = Buffer::Length(val);

  return buf->len;
}


void StreamWrap::WriteBuffer(const FunctionCallbackInfo<Value>& args) {
  HandleScope handle_scope(args.GetIsolate());
  Environment* env = Environment::GetCurrent(args.GetIsolate());

  StreamWrap* wrap = Unwrap<StreamWrap>(args.Holder());

  assert(args[0]->IsObject());
  assert(Buffer::HasInstance(args[1]));

  Local<Object> req_wrap_obj = args[0].As<Object>();
  Local<Object> buf_obj = args[1].As<Object>();

  size_t length = Buffer::Length(buf_obj);

  char* storage;
  WriteWrap* req_wrap;
  uv_buf_t buf;
  WriteBuffer(buf_obj, &buf);

  // Try writing immediately without allocation
  uv_buf_t* bufs = &buf;
  size_t count = 1;
  int err = wrap->callbacks()->TryWrite(&bufs, &count);
  if (err != 0)
    goto done;
  if (count == 0)
    goto done;
  assert(count == 1);

  // Allocate, or write rest
  storage = new char[sizeof(WriteWrap)];
  req_wrap = new(storage) WriteWrap(env, req_wrap_obj, wrap);

  err = wrap->callbacks()->DoWrite(req_wrap,
                                   bufs,
                                   count,
                                   NULL,
                                   StreamWrap::AfterWrite);
  req_wrap->Dispatched();
  req_wrap_obj->Set(env->async(), True(env->isolate()));

  if (err) {
    req_wrap->~WriteWrap();
    delete[] storage;
  }

 done:
  const char* msg = wrap->callbacks()->Error();
  if (msg != NULL)
    req_wrap_obj->Set(env->error_string(), OneByteString(env->isolate(), msg));
  req_wrap_obj->Set(env->bytes_string(),
                    Integer::NewFromUnsigned(env->isolate(), length));
  args.GetReturnValue().Set(err);
}


template <enum encoding encoding>
void StreamWrap::WriteStringImpl(const FunctionCallbackInfo<Value>& args) {
  HandleScope handle_scope(args.GetIsolate());
  Environment* env = Environment::GetCurrent(args.GetIsolate());
  int err;

  StreamWrap* wrap = Unwrap<StreamWrap>(args.Holder());

  assert(args[0]->IsObject());
  assert(args[1]->IsString());

  Local<Object> req_wrap_obj = args[0].As<Object>();
  Local<String> string = args[1].As<String>();

  // Compute the size of the storage that the string will be flattened into.
  // For UTF8 strings that are very long, go ahead and take the hit for
  // computing their actual size, rather than tripling the storage.
  size_t storage_size;
  if (encoding == UTF8 && string->Length() > 65535)
    storage_size = StringBytes::Size(env->isolate(), string, encoding);
  else
    storage_size = StringBytes::StorageSize(env->isolate(), string, encoding);

  if (storage_size > INT_MAX) {
    args.GetReturnValue().Set(UV_ENOBUFS);
    return;
  }

  // Try writing immediately if write size isn't too big
  char* storage;
  WriteWrap* req_wrap;
  char* data;
  char stack_storage[16384];  // 16kb
  size_t data_size;
  uv_buf_t buf;

  bool try_write = storage_size + 15 <= sizeof(stack_storage) &&
                   (!wrap->is_named_pipe_ipc() || !args[2]->IsObject());
  if (try_write) {
    data_size = StringBytes::Write(env->isolate(),
                                   stack_storage,
                                   storage_size,
                                   string,
                                   encoding);
    buf = uv_buf_init(stack_storage, data_size);

    uv_buf_t* bufs = &buf;
    size_t count = 1;
    err = wrap->callbacks()->TryWrite(&bufs, &count);

    // Failure
    if (err != 0)
      goto done;

    // Success
    if (count == 0)
      goto done;

    // Partial write
    assert(count == 1);
  }

  storage = new char[sizeof(WriteWrap) + storage_size + 15];
  req_wrap = new(storage) WriteWrap(env, req_wrap_obj, wrap);

  data = reinterpret_cast<char*>(ROUND_UP(
      reinterpret_cast<uintptr_t>(storage) + sizeof(WriteWrap), 16));

  if (try_write) {
    // Copy partial data
    memcpy(data, buf.base, buf.len);
    data_size = buf.len;
  } else {
    // Write it
    data_size = StringBytes::Write(env->isolate(),
                                   data,
                                   storage_size,
                                   string,
                                   encoding);
  }

  assert(data_size <= storage_size);

  buf = uv_buf_init(data, data_size);

  if (!wrap->is_named_pipe_ipc()) {
    err = wrap->callbacks()->DoWrite(req_wrap,
                                     &buf,
                                     1,
                                     NULL,
                                     StreamWrap::AfterWrite);
  } else {
    uv_handle_t* send_handle = NULL;

    if (args[2]->IsObject()) {
      Local<Object> send_handle_obj = args[2].As<Object>();
      HandleWrap* wrap = Unwrap<HandleWrap>(send_handle_obj);
      send_handle = wrap->GetHandle();
      // Reference StreamWrap instance to prevent it from being garbage
      // collected before `AfterWrite` is called.
      assert(!req_wrap->persistent().IsEmpty());
      req_wrap->object()->Set(env->handle_string(), send_handle_obj);
    }

    err = wrap->callbacks()->DoWrite(
        req_wrap,
        &buf,
        1,
        reinterpret_cast<uv_stream_t*>(send_handle),
        StreamWrap::AfterWrite);
  }

  req_wrap->Dispatched();
  req_wrap->object()->Set(env->async(), True(env->isolate()));

  if (err) {
    req_wrap->~WriteWrap();
    delete[] storage;
  }

 done:
  const char* msg = wrap->callbacks()->Error();
  if (msg != NULL)
    req_wrap_obj->Set(env->error_string(), OneByteString(env->isolate(), msg));
  req_wrap_obj->Set(env->bytes_string(),
                    Integer::NewFromUnsigned(env->isolate(), data_size));
  args.GetReturnValue().Set(err);
}


void StreamWrap::Writev(const FunctionCallbackInfo<Value>& args) {
  HandleScope handle_scope(args.GetIsolate());
  Environment* env = Environment::GetCurrent(args.GetIsolate());

  StreamWrap* wrap = Unwrap<StreamWrap>(args.Holder());

  assert(args[0]->IsObject());
  assert(args[1]->IsArray());

  Local<Object> req_wrap_obj = args[0].As<Object>();
  Local<Array> chunks = args[1].As<Array>();
  size_t count = chunks->Length() >> 1;

  uv_buf_t bufs_[16];
  uv_buf_t* bufs = bufs_;

  // Determine storage size first
  size_t storage_size = 0;
  for (size_t i = 0; i < count; i++) {
    Handle<Value> chunk = chunks->Get(i * 2);

    if (Buffer::HasInstance(chunk))
      continue;
      // Buffer chunk, no additional storage required

    // String chunk
    Handle<String> string = chunk->ToString();
    enum encoding encoding = ParseEncoding(env->isolate(),
                                           chunks->Get(i * 2 + 1));
    size_t chunk_size;
    if (encoding == UTF8 && string->Length() > 65535)
      chunk_size = StringBytes::Size(env->isolate(), string, encoding);
    else
      chunk_size = StringBytes::StorageSize(env->isolate(), string, encoding);

    storage_size += chunk_size + 15;
  }

  if (storage_size > INT_MAX) {
    args.GetReturnValue().Set(UV_ENOBUFS);
    return;
  }

  if (ARRAY_SIZE(bufs_) < count)
    bufs = new uv_buf_t[count];

  storage_size += sizeof(WriteWrap);
  char* storage = new char[storage_size];
  WriteWrap* req_wrap =
      new(storage) WriteWrap(env, req_wrap_obj, wrap);

  uint32_t bytes = 0;
  size_t offset = sizeof(WriteWrap);
  for (size_t i = 0; i < count; i++) {
    Handle<Value> chunk = chunks->Get(i * 2);

    // Write buffer
    if (Buffer::HasInstance(chunk)) {
      bufs[i].base = Buffer::Data(chunk);
      bufs[i].len = Buffer::Length(chunk);
      bytes += bufs[i].len;
      continue;
    }

    // Write string
    offset = ROUND_UP(offset, 16);
    assert(offset < storage_size);
    char* str_storage = storage + offset;
    size_t str_size = storage_size - offset;

    Handle<String> string = chunk->ToString();
    enum encoding encoding = ParseEncoding(env->isolate(),
                                           chunks->Get(i * 2 + 1));
    str_size = StringBytes::Write(env->isolate(),
                                  str_storage,
                                  str_size,
                                  string,
                                  encoding);
    bufs[i].base = str_storage;
    bufs[i].len = str_size;
    offset += str_size;
    bytes += str_size;
  }

  int err = wrap->callbacks()->DoWrite(req_wrap,
                                       bufs,
                                       count,
                                       NULL,
                                       StreamWrap::AfterWrite);

  // Deallocate space
  if (bufs != bufs_)
    delete[] bufs;

  req_wrap->Dispatched();
  req_wrap->object()->Set(env->async(), True(env->isolate()));
  req_wrap->object()->Set(env->bytes_string(),
                          Number::New(env->isolate(), bytes));
  const char* msg = wrap->callbacks()->Error();
  if (msg != NULL)
    req_wrap_obj->Set(env->error_string(), OneByteString(env->isolate(), msg));

  if (err) {
    req_wrap->~WriteWrap();
    delete[] storage;
  }

  args.GetReturnValue().Set(err);
}


void StreamWrap::WriteAsciiString(const FunctionCallbackInfo<Value>& args) {
  WriteStringImpl<ASCII>(args);
}


void StreamWrap::WriteUtf8String(const FunctionCallbackInfo<Value>& args) {
  WriteStringImpl<UTF8>(args);
}


void StreamWrap::WriteUcs2String(const FunctionCallbackInfo<Value>& args) {
  WriteStringImpl<UCS2>(args);
}

void StreamWrap::WriteBinaryString(const FunctionCallbackInfo<Value>& args) {
  WriteStringImpl<BINARY>(args);
}

void StreamWrap::SetBlocking(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args.GetIsolate());
  HandleScope scope(env->isolate());

  StreamWrap* wrap = Unwrap<StreamWrap>(args.Holder());

  assert(args.Length() > 0);
  int err = uv_stream_set_blocking(wrap->stream(), args[0]->IsTrue());

  args.GetReturnValue().Set(err);
}

void StreamWrap::AfterWrite(uv_write_t* req, int status) {
  WriteWrap* req_wrap = ContainerOf(&WriteWrap::req_, req);
  StreamWrap* wrap = req_wrap->wrap();
  Environment* env = wrap->env();

  HandleScope handle_scope(env->isolate());
  Context::Scope context_scope(env->context());

  // The wrap and request objects should still be there.
  assert(req_wrap->persistent().IsEmpty() == false);
  assert(wrap->persistent().IsEmpty() == false);

  // Unref handle property
  Local<Object> req_wrap_obj = req_wrap->object();
  req_wrap_obj->Delete(env->handle_string());
  wrap->callbacks()->AfterWrite(req_wrap);

  Local<Value> argv[] = {
    Integer::New(env->isolate(), status),
    wrap->object(),
    req_wrap_obj,
    Undefined(env->isolate())
  };

  const char* msg = wrap->callbacks()->Error();
  if (msg != NULL)
    argv[3] = OneByteString(env->isolate(), msg);

  req_wrap->MakeCallback(env->oncomplete_string(), ARRAY_SIZE(argv), argv);

  req_wrap->~WriteWrap();
  delete[] reinterpret_cast<char*>(req_wrap);
}


void StreamWrap::Shutdown(const FunctionCallbackInfo<Value>& args) {
  HandleScope handle_scope(args.GetIsolate());
  Environment* env = Environment::GetCurrent(args.GetIsolate());

  StreamWrap* wrap = Unwrap<StreamWrap>(args.Holder());

  assert(args[0]->IsObject());
  Local<Object> req_wrap_obj = args[0].As<Object>();

  ShutdownWrap* req_wrap = new ShutdownWrap(env, req_wrap_obj);
  int err = wrap->callbacks()->DoShutdown(req_wrap, AfterShutdown);
  req_wrap->Dispatched();
  if (err)
    delete req_wrap;
  args.GetReturnValue().Set(err);
}


void StreamWrap::AfterShutdown(uv_shutdown_t* req, int status) {
  ShutdownWrap* req_wrap = static_cast<ShutdownWrap*>(req->data);
  StreamWrap* wrap = static_cast<StreamWrap*>(req->handle->data);
  Environment* env = wrap->env();

  // The wrap and request objects should still be there.
  assert(req_wrap->persistent().IsEmpty() == false);
  assert(wrap->persistent().IsEmpty() == false);

  HandleScope handle_scope(env->isolate());
  Context::Scope context_scope(env->context());

  Local<Object> req_wrap_obj = req_wrap->object();
  Local<Value> argv[3] = {
    Integer::New(env->isolate(), status),
    wrap->object(),
    req_wrap_obj
  };

  req_wrap->MakeCallback(env->oncomplete_string(), ARRAY_SIZE(argv), argv);

  delete req_wrap;
}


const char* StreamWrapCallbacks::Error() {
  return NULL;
}


// NOTE: Call to this function could change both `buf`'s and `count`'s
// values, shifting their base and decrementing their length. This is
// required in order to skip the data that was successfully written via
// uv_try_write().
int StreamWrapCallbacks::TryWrite(uv_buf_t** bufs, size_t* count) {
  int err;
  size_t written;
  uv_buf_t* vbufs = *bufs;
  size_t vcount = *count;

  err = uv_try_write(wrap()->stream(), vbufs, vcount);
  if (err == UV_ENOSYS || err == UV_EAGAIN)
    return 0;
  if (err < 0)
    return err;

  // Slice off the buffers: skip all written buffers and slice the one that
  // was partially written.
  written = err;
  for (; written != 0 && vcount > 0; vbufs++, vcount--) {
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


int StreamWrapCallbacks::DoWrite(WriteWrap* w,
                                 uv_buf_t* bufs,
                                 size_t count,
                                 uv_stream_t* send_handle,
                                 uv_write_cb cb) {
  int r;
  if (send_handle == NULL) {
    r = uv_write(&w->req_, wrap()->stream(), bufs, count, cb);
  } else {
    r = uv_write2(&w->req_, wrap()->stream(), bufs, count, send_handle, cb);
  }

  if (!r) {
    size_t bytes = 0;
    for (size_t i = 0; i < count; i++)
      bytes += bufs[i].len;
    if (wrap()->stream()->type == UV_TCP) {
      NODE_COUNT_NET_BYTES_SENT(bytes);
    } else if (wrap()->stream()->type == UV_NAMED_PIPE) {
      NODE_COUNT_PIPE_BYTES_SENT(bytes);
    }
  }

  wrap()->UpdateWriteQueueSize();

  return r;
}


void StreamWrapCallbacks::AfterWrite(WriteWrap* w) {
  wrap()->UpdateWriteQueueSize();
}


void StreamWrapCallbacks::DoAlloc(uv_handle_t* handle,
                                  size_t suggested_size,
                                  uv_buf_t* buf) {
  buf->base = static_cast<char*>(malloc(suggested_size));
  buf->len = suggested_size;

  if (buf->base == NULL && suggested_size > 0) {
    FatalError(
        "node::StreamWrapCallbacks::DoAlloc(uv_handle_t*, size_t, uv_buf_t*)",
        "Out Of Memory");
  }
}


void StreamWrapCallbacks::DoRead(uv_stream_t* handle,
                                 ssize_t nread,
                                 const uv_buf_t* buf,
                                 uv_handle_type pending) {
  Environment* env = wrap()->env();
  HandleScope handle_scope(env->isolate());
  Context::Scope context_scope(env->context());

  Local<Value> argv[] = {
    Integer::New(env->isolate(), nread),
    Undefined(env->isolate()),
    Undefined(env->isolate())
  };

  if (nread < 0)  {
    if (buf->base != NULL)
      free(buf->base);
    wrap()->MakeCallback(env->onread_string(), ARRAY_SIZE(argv), argv);
    return;
  }

  if (nread == 0) {
    if (buf->base != NULL)
      free(buf->base);
    return;
  }

  char* base = static_cast<char*>(realloc(buf->base, nread));
  assert(static_cast<size_t>(nread) <= buf->len);
  argv[1] = Buffer::Use(env, base, nread);

  Local<Object> pending_obj;
  if (pending == UV_TCP) {
    pending_obj = AcceptHandle<TCPWrap, uv_tcp_t>(env, handle, wrap());
  } else if (pending == UV_NAMED_PIPE) {
    pending_obj = AcceptHandle<PipeWrap, uv_pipe_t>(env, handle, wrap());
  } else if (pending == UV_UDP) {
    pending_obj = AcceptHandle<UDPWrap, uv_udp_t>(env, handle, wrap());
  } else {
    assert(pending == UV_UNKNOWN_HANDLE);
  }

  if (!pending_obj.IsEmpty()) {
    argv[2] = pending_obj;
  }

  wrap()->MakeCallback(env->onread_string(), ARRAY_SIZE(argv), argv);
}


int StreamWrapCallbacks::DoShutdown(ShutdownWrap* req_wrap, uv_shutdown_cb cb) {
  return uv_shutdown(&req_wrap->req_, wrap()->stream(), cb);
}

}  // namespace node

NODE_MODULE_CONTEXT_AWARE_BUILTIN(stream_wrap, node::StreamWrap::Initialize)
