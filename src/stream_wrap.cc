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
#include "node.h"
#include "node_buffer.h"
#include "node_counters.h"
#include "handle_wrap.h"
#include "pipe_wrap.h"
#include "req_wrap.h"
#include "tcp_wrap.h"
#include "udp_wrap.h"

#include <stdlib.h>  // abort()
#include <limits.h>  // INT_MAX


namespace node {

using v8::Array;
using v8::FunctionCallbackInfo;
using v8::Handle;
using v8::HandleScope;
using v8::Integer;
using v8::Local;
using v8::Number;
using v8::Object;
using v8::PropertyCallbackInfo;
using v8::String;
using v8::Undefined;
using v8::Value;


static Cached<String> buffer_sym;
static Cached<String> bytes_sym;
static Cached<String> write_queue_size_sym;
static Cached<String> onread_sym;
static Cached<String> oncomplete_sym;
static Cached<String> handle_sym;
static bool initialized;


void StreamWrap::Initialize(Handle<Object> target) {
  if (initialized) return;
  initialized = true;

  HandleScope scope(node_isolate);
  buffer_sym = String::New("buffer");
  bytes_sym = String::New("bytes");
  write_queue_size_sym = String::New("writeQueueSize");
  onread_sym = String::New("onread");
  oncomplete_sym = String::New("oncomplete");
}


StreamWrap::StreamWrap(Handle<Object> object, uv_stream_t* stream)
    : HandleWrap(object, reinterpret_cast<uv_handle_t*>(stream)),
      stream_(stream),
      default_callbacks_(this),
      callbacks_(&default_callbacks_) {
}


void StreamWrap::GetFD(Local<String>, const PropertyCallbackInfo<Value>& args) {
#if !defined(_WIN32)
  HandleScope scope(node_isolate);
  UNWRAP_NO_ABORT(StreamWrap)
  int fd = -1;
  if (wrap != NULL && wrap->stream() != NULL) {
    fd = wrap->stream()->io_watcher.fd;
  }
  args.GetReturnValue().Set(fd);
#endif
}


void StreamWrap::UpdateWriteQueueSize() {
  HandleScope scope(node_isolate);
  object()->Set(write_queue_size_sym,
                Integer::New(stream()->write_queue_size, node_isolate));
}


void StreamWrap::ReadStart(const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(node_isolate);

  UNWRAP(StreamWrap)

  int err;
  if (wrap->is_named_pipe_ipc()) {
    err = uv_read2_start(wrap->stream(), OnAlloc, OnRead2);
  } else {
    err = uv_read_start(wrap->stream(), OnAlloc, OnRead);
  }

  args.GetReturnValue().Set(err);
}


void StreamWrap::ReadStop(const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(node_isolate);

  UNWRAP(StreamWrap)

  int err = uv_read_stop(wrap->stream());
  args.GetReturnValue().Set(err);
}


uv_buf_t StreamWrap::OnAlloc(uv_handle_t* handle, size_t suggested_size) {
  StreamWrap* wrap = static_cast<StreamWrap*>(handle->data);
  assert(wrap->stream() == reinterpret_cast<uv_stream_t*>(handle));
  return wrap->callbacks()->DoAlloc(handle, suggested_size);
}


template <class WrapType, class UVType>
static Local<Object> AcceptHandle(uv_stream_t* pipe) {
  HandleScope scope(node_isolate);
  Local<Object> wrap_obj;
  WrapType* wrap;
  UVType* handle;

  wrap_obj = WrapType::Instantiate();
  if (wrap_obj.IsEmpty())
    return Local<Object>();

  wrap = static_cast<WrapType*>(
      wrap_obj->GetAlignedPointerFromInternalField(0));
  handle = wrap->UVHandle();

  if (uv_accept(pipe, reinterpret_cast<uv_stream_t*>(handle)))
    abort();

  return scope.Close(wrap_obj);
}


void StreamWrap::OnReadCommon(uv_stream_t* handle,
                              ssize_t nread,
                              uv_buf_t buf,
                              uv_handle_type pending) {
  HandleScope scope(node_isolate);

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


void StreamWrap::OnRead(uv_stream_t* handle, ssize_t nread, uv_buf_t buf) {
  OnReadCommon(handle, nread, buf, UV_UNKNOWN_HANDLE);
}


void StreamWrap::OnRead2(uv_pipe_t* handle, ssize_t nread, uv_buf_t buf,
    uv_handle_type pending) {
  OnReadCommon(reinterpret_cast<uv_stream_t*>(handle), nread, buf, pending);
}


size_t StreamWrap::WriteBuffer(Handle<Value> val, uv_buf_t* buf) {
  assert(Buffer::HasInstance(val));

  // Simple non-writev case
  buf->base = Buffer::Data(val);
  buf->len = Buffer::Length(val);

  return buf->len;
}


void StreamWrap::WriteBuffer(const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(node_isolate);

  UNWRAP(StreamWrap)

  assert(args[0]->IsObject());
  assert(Buffer::HasInstance(args[1]));

  Local<Object> req_wrap_obj = args[0].As<Object>();
  Local<Object> buf_obj = args[1].As<Object>();

  size_t length = Buffer::Length(buf_obj);
  char* storage = new char[sizeof(WriteWrap)];
  WriteWrap* req_wrap = new(storage) WriteWrap(req_wrap_obj, wrap);

  req_wrap_obj->SetHiddenValue(buffer_sym, buf_obj);

  uv_buf_t buf;
  WriteBuffer(buf_obj, &buf);

  int err = wrap->callbacks()->DoWrite(req_wrap,
                                       &buf,
                                       1,
                                       NULL,
                                       StreamWrap::AfterWrite);
  req_wrap->Dispatched();
  req_wrap_obj->Set(bytes_sym, Integer::NewFromUnsigned(length, node_isolate));

  if (err) {
    req_wrap->~WriteWrap();
    delete[] storage;
  }

  args.GetReturnValue().Set(err);
}


template <enum encoding encoding>
void StreamWrap::WriteStringImpl(const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(node_isolate);
  int err;

  UNWRAP(StreamWrap)

  assert(args[0]->IsObject());
  assert(args[1]->IsString());

  Local<Object> req_wrap_obj = args[0].As<Object>();
  Local<String> string = args[1].As<String>();

  // Compute the size of the storage that the string will be flattened into.
  // For UTF8 strings that are very long, go ahead and take the hit for
  // computing their actual size, rather than tripling the storage.
  size_t storage_size;
  if (encoding == UTF8 && string->Length() > 65535)
    storage_size = StringBytes::Size(string, encoding);
  else
    storage_size = StringBytes::StorageSize(string, encoding);

  if (storage_size > INT_MAX) {
    args.GetReturnValue().Set(UV_ENOBUFS);
    return;
  }

  char* storage = new char[sizeof(WriteWrap) + storage_size + 15];
  WriteWrap* req_wrap = new(storage) WriteWrap(req_wrap_obj, wrap);

  char* data = reinterpret_cast<char*>(ROUND_UP(
      reinterpret_cast<uintptr_t>(storage) + sizeof(WriteWrap), 16));

  size_t data_size;
  data_size = StringBytes::Write(data, storage_size, string, encoding);

  assert(data_size <= storage_size);

  uv_buf_t buf;

  buf.base = data;
  buf.len = data_size;

  if (!wrap->is_named_pipe_ipc()) {
    err = wrap->callbacks()->DoWrite(req_wrap,
                                     &buf,
                                     1,
                                     NULL,
                                     StreamWrap::AfterWrite);
  } else {
    uv_handle_t* send_handle = NULL;

    if (args[2]->IsObject()) {
      Local<Object> send_handle_obj = args[2]->ToObject();
      assert(send_handle_obj->InternalFieldCount() > 0);
      HandleWrap* send_handle_wrap = static_cast<HandleWrap*>(
          send_handle_obj->GetAlignedPointerFromInternalField(0));
      send_handle = send_handle_wrap->GetHandle();

      // Reference StreamWrap instance to prevent it from being garbage
      // collected before `AfterWrite` is called.
      if (handle_sym.IsEmpty()) {
        handle_sym = String::New("handle");
      }
      assert(!req_wrap->persistent().IsEmpty());
      req_wrap->object()->Set(handle_sym, send_handle_obj);
    }

    err = wrap->callbacks()->DoWrite(req_wrap,
                                     &buf,
                                     1,
                                     reinterpret_cast<uv_stream_t*>(send_handle),
                                     StreamWrap::AfterWrite);
  }

  req_wrap->Dispatched();
  req_wrap->object()->Set(bytes_sym, Number::New(node_isolate, data_size));

  if (err) {
    req_wrap->~WriteWrap();
    delete[] storage;
  }

  args.GetReturnValue().Set(err);
}


void StreamWrap::Writev(const FunctionCallbackInfo<Value>& args) {
  HandleScope scope;

  UNWRAP(StreamWrap)

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
    enum encoding encoding = ParseEncoding(chunks->Get(i * 2 + 1));
    size_t chunk_size;
    if (encoding == UTF8 && string->Length() > 65535)
      chunk_size = StringBytes::Size(string, encoding);
    else
      chunk_size = StringBytes::StorageSize(string, encoding);

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
  WriteWrap* req_wrap = new(storage) WriteWrap(req_wrap_obj, wrap);

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
    enum encoding encoding = ParseEncoding(chunks->Get(i * 2 + 1));
    str_size = StringBytes::Write(str_storage, str_size, string, encoding);
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
  req_wrap->object()->Set(bytes_sym, Number::New(node_isolate, bytes));

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


void StreamWrap::AfterWrite(uv_write_t* req, int status) {
  WriteWrap* req_wrap = container_of(req, WriteWrap, req_);
  StreamWrap* wrap = req_wrap->wrap();

  HandleScope scope(node_isolate);

  // The wrap and request objects should still be there.
  assert(req_wrap->persistent().IsEmpty() == false);
  assert(wrap->persistent().IsEmpty() == false);

  // Unref handle property
  Local<Object> req_wrap_obj = req_wrap->object();
  if (!handle_sym.IsEmpty()) {
    req_wrap_obj->Delete(handle_sym);
  }

  wrap->callbacks()->AfterWrite(req_wrap);

  Local<Value> argv[] = {
    Integer::New(status, node_isolate),
    wrap->object(),
    req_wrap_obj
  };

  MakeCallback(req_wrap_obj, oncomplete_sym, ARRAY_SIZE(argv), argv);

  req_wrap->~WriteWrap();
  delete[] reinterpret_cast<char*>(req_wrap);
}


void StreamWrap::Shutdown(const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(node_isolate);

  UNWRAP(StreamWrap)

  assert(args[0]->IsObject());
  Local<Object> req_wrap_obj = args[0].As<Object>();

  ShutdownWrap* req_wrap = new ShutdownWrap(req_wrap_obj);
  int err = wrap->callbacks()->DoShutdown(req_wrap, AfterShutdown);
  req_wrap->Dispatched();
  if (err) delete req_wrap;
  args.GetReturnValue().Set(err);
}


void StreamWrap::AfterShutdown(uv_shutdown_t* req, int status) {
  ShutdownWrap* req_wrap = static_cast<ShutdownWrap*>(req->data);
  StreamWrap* wrap = static_cast<StreamWrap*>(req->handle->data);

  // The wrap and request objects should still be there.
  assert(req_wrap->persistent().IsEmpty() == false);
  assert(wrap->persistent().IsEmpty() == false);

  HandleScope scope(node_isolate);

  Local<Object> req_wrap_obj = req_wrap->object();
  Local<Value> argv[3] = {
    Integer::New(status, node_isolate),
    wrap->object(),
    req_wrap_obj
  };

  MakeCallback(req_wrap_obj, oncomplete_sym, ARRAY_SIZE(argv), argv);

  delete req_wrap;
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


uv_buf_t StreamWrapCallbacks::DoAlloc(uv_handle_t* handle,
                                      size_t suggested_size) {
  char* data = static_cast<char*>(malloc(suggested_size));
  if (data == NULL && suggested_size > 0) {
    FatalError("node::StreamWrapCallbacks::DoAlloc(uv_handle_t*, size_t)",
               "Out Of Memory");
  }
  return uv_buf_init(data, suggested_size);
}


void StreamWrapCallbacks::DoRead(uv_stream_t* handle,
                                 ssize_t nread,
                                 uv_buf_t buf,
                                 uv_handle_type pending) {
  HandleScope scope(node_isolate);

  Local<Value> argv[] = {
    Integer::New(nread, node_isolate),
    Undefined(),
    Undefined()
  };

  if (nread < 0)  {
    if (buf.base != NULL)
      free(buf.base);
    MakeCallback(Self(), onread_sym, ARRAY_SIZE(argv), argv);
    return;
  }

  if (nread == 0) {
    if (buf.base != NULL)
      free(buf.base);
    return;
  }

  buf.base = static_cast<char*>(realloc(buf.base, nread));
  assert(static_cast<size_t>(nread) <= buf.len);
  argv[1] = Buffer::Use(buf.base, nread);

  Local<Object> pending_obj;
  if (pending == UV_TCP) {
    pending_obj = AcceptHandle<TCPWrap, uv_tcp_t>(handle);
  } else if (pending == UV_NAMED_PIPE) {
    pending_obj = AcceptHandle<PipeWrap, uv_pipe_t>(handle);
  } else if (pending == UV_UDP) {
    pending_obj = AcceptHandle<UDPWrap, uv_udp_t>(handle);
  } else {
    assert(pending == UV_UNKNOWN_HANDLE);
  }

  if (!pending_obj.IsEmpty()) {
    argv[2] = pending_obj;
  }

  MakeCallback(wrap()->object(), onread_sym, ARRAY_SIZE(argv), argv);
}


int StreamWrapCallbacks::DoShutdown(ShutdownWrap* req_wrap, uv_shutdown_cb cb) {
  return uv_shutdown(&req_wrap->req_, wrap()->stream(), cb);
}


Handle<Object> StreamWrapCallbacks::Self() {
  return wrap()->object();
}

}  // namespace node
