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

#include "node.h"
#include "node_buffer.h"
#include "handle_wrap.h"
#include "slab_allocator.h"
#include "stream_wrap.h"
#include "pipe_wrap.h"
#include "tcp_wrap.h"
#include "req_wrap.h"
#include "udp_wrap.h"
#include "node_counters.h"

#include <stdlib.h> // abort()
#include <limits.h> // INT_MAX

#define SLAB_SIZE (1024 * 1024)


namespace node {

using v8::AccessorInfo;
using v8::Arguments;
using v8::Context;
using v8::Exception;
using v8::Function;
using v8::FunctionTemplate;
using v8::Handle;
using v8::HandleScope;
using v8::Integer;
using v8::Local;
using v8::Number;
using v8::Object;
using v8::Persistent;
using v8::String;
using v8::TryCatch;
using v8::Value;

typedef class ReqWrap<uv_shutdown_t> ShutdownWrap;

class WriteWrap: public ReqWrap<uv_write_t> {
 public:
  void* operator new(size_t size, char* storage) { return storage; }

  // This is just to keep the compiler happy. It should never be called, since
  // we don't use exceptions in node.
  void operator delete(void* ptr, char* storage) { assert(0); }

 protected:
  // People should not be using the non-placement new and delete operator on a
  // WriteWrap. Ensure this never happens.
  void* operator new (size_t size) { assert(0); };
  void operator delete(void* ptr) { assert(0); };
};


static Persistent<String> buffer_sym;
static Persistent<String> bytes_sym;
static Persistent<String> write_queue_size_sym;
static Persistent<String> onread_sym;
static Persistent<String> oncomplete_sym;
static Persistent<String> handle_sym;
static SlabAllocator* slab_allocator;
static bool initialized;


static void DeleteSlabAllocator(void*) {
  delete slab_allocator;
  slab_allocator = NULL;
}


void StreamWrap::Initialize(Handle<Object> target) {
  if (initialized) return;
  initialized = true;

  slab_allocator = new SlabAllocator(SLAB_SIZE);
  AtExit(DeleteSlabAllocator, NULL);

  HandleScope scope;

  HandleWrap::Initialize(target);

  buffer_sym = NODE_PSYMBOL("buffer");
  bytes_sym = NODE_PSYMBOL("bytes");
  write_queue_size_sym = NODE_PSYMBOL("writeQueueSize");
  onread_sym = NODE_PSYMBOL("onread");
  oncomplete_sym = NODE_PSYMBOL("oncomplete");
}


StreamWrap::StreamWrap(Handle<Object> object, uv_stream_t* stream)
    : HandleWrap(object, (uv_handle_t*)stream) {
  stream_ = stream;
  if (stream) {
    stream->data = this;
  }
}


Handle<Value> StreamWrap::GetFD(Local<String>, const AccessorInfo& args) {
#if defined(_WIN32)
  return v8::Null();
#else
  HandleScope scope;
  UNWRAP_NO_ABORT(StreamWrap)
  int fd = -1;
  if (wrap != NULL && wrap->stream_ != NULL) fd = wrap->stream_->io_watcher.fd;
  return scope.Close(Integer::New(fd));
#endif
}


void StreamWrap::SetHandle(uv_handle_t* h) {
  HandleWrap::SetHandle(h);
  stream_ = reinterpret_cast<uv_stream_t*>(h);
  stream_->data = this;
}


void StreamWrap::UpdateWriteQueueSize() {
  HandleScope scope;
  object_->Set(write_queue_size_sym,
               Integer::New(stream_->write_queue_size));
}


Handle<Value> StreamWrap::ReadStart(const Arguments& args) {
  HandleScope scope;

  UNWRAP(StreamWrap)

  bool ipc_pipe = wrap->stream_->type == UV_NAMED_PIPE &&
                  ((uv_pipe_t*)wrap->stream_)->ipc;
  int r;
  if (ipc_pipe) {
    r = uv_read2_start(wrap->stream_, OnAlloc, OnRead2);
  } else {
    r = uv_read_start(wrap->stream_, OnAlloc, OnRead);
  }

  // Error starting the tcp.
  if (r) SetErrno(uv_last_error(uv_default_loop()));

  return scope.Close(Integer::New(r));
}


Handle<Value> StreamWrap::ReadStop(const Arguments& args) {
  HandleScope scope;

  UNWRAP(StreamWrap)

  int r = uv_read_stop(wrap->stream_);

  // Error starting the tcp.
  if (r) SetErrno(uv_last_error(uv_default_loop()));

  return scope.Close(Integer::New(r));
}


uv_buf_t StreamWrap::OnAlloc(uv_handle_t* handle, size_t suggested_size) {
  StreamWrap* wrap = static_cast<StreamWrap*>(handle->data);
  assert(wrap->stream_ == reinterpret_cast<uv_stream_t*>(handle));
  char* buf = slab_allocator->Allocate(wrap->object_, suggested_size);
  return uv_buf_init(buf, suggested_size);
}


template <class WrapType, class UVType>
static Local<Object> AcceptHandle(uv_stream_t* pipe) {
  HandleScope scope;
  Local<Object> wrap_obj;
  WrapType* wrap;
  UVType* handle;

  wrap_obj = WrapType::Instantiate();
  if (wrap_obj.IsEmpty())
    return Local<Object>();

  wrap = static_cast<WrapType*>(
      wrap_obj->GetPointerFromInternalField(0));
  handle = wrap->UVHandle();

  if (uv_accept(pipe, reinterpret_cast<uv_stream_t*>(handle)))
    abort();

  return scope.Close(wrap_obj);
}


void StreamWrap::OnReadCommon(uv_stream_t* handle, ssize_t nread,
    uv_buf_t buf, uv_handle_type pending) {
  HandleScope scope;

  StreamWrap* wrap = static_cast<StreamWrap*>(handle->data);

  // We should not be getting this callback if someone as already called
  // uv_close() on the handle.
  assert(wrap->object_.IsEmpty() == false);

  if (nread < 0)  {
    // If libuv reports an error or EOF it *may* give us a buffer back. In that
    // case, return the space to the slab.
    if (buf.base != NULL) {
      slab_allocator->Shrink(wrap->object_, buf.base, 0);
    }

    SetErrno(uv_last_error(uv_default_loop()));
    MakeCallback(wrap->object_, onread_sym, 0, NULL);
    return;
  }

  assert(buf.base != NULL);
  Local<Object> slab = slab_allocator->Shrink(wrap->object_,
                                              buf.base,
                                              nread);

  if (nread == 0) return;
  assert(static_cast<size_t>(nread) <= buf.len);

  int argc = 3;
  Local<Value> argv[4] = {
    slab,
    Integer::NewFromUnsigned(buf.base - Buffer::Data(slab)),
    Integer::NewFromUnsigned(nread)
  };

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
    argv[3] = pending_obj;
    argc++;
  }

  if (wrap->stream_->type == UV_TCP) {
    NODE_COUNT_NET_BYTES_RECV(nread);
  } else if (wrap->stream_->type == UV_NAMED_PIPE) {
    NODE_COUNT_PIPE_BYTES_RECV(nread);
  }

  MakeCallback(wrap->object_, onread_sym, argc, argv);
}


void StreamWrap::OnRead(uv_stream_t* handle, ssize_t nread, uv_buf_t buf) {
  OnReadCommon(handle, nread, buf, UV_UNKNOWN_HANDLE);
}


void StreamWrap::OnRead2(uv_pipe_t* handle, ssize_t nread, uv_buf_t buf,
    uv_handle_type pending) {
  OnReadCommon(reinterpret_cast<uv_stream_t*>(handle), nread, buf, pending);
}


Handle<Value> StreamWrap::WriteBuffer(const Arguments& args) {
  HandleScope scope;

  UNWRAP(StreamWrap)

  // The first argument is a buffer.
  assert(args.Length() >= 1 && Buffer::HasInstance(args[0]));
  Local<Object> buffer_obj = args[0]->ToObject();
  size_t offset = 0;
  size_t length = Buffer::Length(buffer_obj);
  char* storage = new char[sizeof(WriteWrap)];
  WriteWrap* req_wrap = new (storage) WriteWrap();

  req_wrap->object_->SetHiddenValue(buffer_sym, buffer_obj);

  uv_buf_t buf;
  buf.base = Buffer::Data(buffer_obj) + offset;
  buf.len = length;

  int r = uv_write(&req_wrap->req_,
                   wrap->stream_,
                   &buf,
                   1,
                   StreamWrap::AfterWrite);

  req_wrap->Dispatched();
  req_wrap->object_->Set(bytes_sym,
                         Integer::NewFromUnsigned(length));

  wrap->UpdateWriteQueueSize();

  if (r) {
    SetErrno(uv_last_error(uv_default_loop()));
    req_wrap->~WriteWrap();
    delete[] storage;
    return scope.Close(v8::Null());
  } else {
    if (wrap->stream_->type == UV_TCP) {
      NODE_COUNT_NET_BYTES_SENT(length);
    } else if (wrap->stream_->type == UV_NAMED_PIPE) {
      NODE_COUNT_PIPE_BYTES_SENT(length);
    }

    return scope.Close(req_wrap->object_);
  }
}


template <WriteEncoding encoding>
Handle<Value> StreamWrap::WriteStringImpl(const Arguments& args) {
  HandleScope scope;
  int r;

  UNWRAP(StreamWrap)

  if (args.Length() < 1)
    return ThrowTypeError("Not enough arguments");

  Local<String> string = args[0]->ToString();

  // Compute the size of the storage that the string will be flattened into.
  size_t storage_size;
  switch (encoding) {
    case kAscii:
      storage_size = string->Length();
      break;

    case kUtf8:
      if (!(string->MayContainNonAscii())) {
        // If the string has only ascii characters, we know exactly how big
        // the storage should be.
        storage_size = string->Length();
      } else if (string->Length() < 65536) {
        // A single UCS2 codepoint never takes up more than 3 utf8 bytes.
        // Unless the string is really long we just allocate so much space that
        // we're certain the string fits in there entirely.
        // TODO: maybe check handle->write_queue_size instead of string length?
        storage_size = 3 * string->Length();
      } else {
        // The string is really long. Compute the allocation size that we
        // actually need.
        storage_size = string->Utf8Length();
      }
      break;

    case kUcs2:
      storage_size = string->Length() * sizeof(uint16_t);
      break;

    default:
      // Unreachable.
      assert(0);
  }

  if (storage_size > INT_MAX) {
    uv_err_t err;
    err.code = UV_ENOBUFS;
    SetErrno(err);
    return scope.Close(v8::Null());
  }

  char* storage = new char[sizeof(WriteWrap) + storage_size + 15];
  WriteWrap* req_wrap = new (storage) WriteWrap();

  char* data = reinterpret_cast<char*>(ROUND_UP(
      reinterpret_cast<uintptr_t>(storage) + sizeof(WriteWrap), 16));
  size_t data_size;
  switch (encoding) {
  case kAscii:
      data_size = string->WriteAscii(data, 0, -1,
          String::NO_NULL_TERMINATION | String::HINT_MANY_WRITES_EXPECTED);
      break;

    case kUtf8:
      data_size = string->WriteUtf8(data, -1, NULL,
          String::NO_NULL_TERMINATION | String::HINT_MANY_WRITES_EXPECTED);
      break;

    case kUcs2: {
      int chars_copied = string->Write((uint16_t*) data, 0, -1,
          String::NO_NULL_TERMINATION | String::HINT_MANY_WRITES_EXPECTED);
      data_size = chars_copied * sizeof(uint16_t);
      break;
    }

    default:
      // Unreachable
      assert(0);
  }

  assert(data_size <= storage_size);

  uv_buf_t buf;
  buf.base = data;
  buf.len = data_size;

  bool ipc_pipe = wrap->stream_->type == UV_NAMED_PIPE &&
                  ((uv_pipe_t*)wrap->stream_)->ipc;

  if (!ipc_pipe) {
    r = uv_write(&req_wrap->req_,
                 wrap->stream_,
                 &buf,
                 1,
                 StreamWrap::AfterWrite);

  } else {
    uv_handle_t* send_handle = NULL;

    if (args[1]->IsObject()) {
      Local<Object> send_handle_obj = args[1]->ToObject();
      assert(send_handle_obj->InternalFieldCount() > 0);
      HandleWrap* send_handle_wrap = static_cast<HandleWrap*>(
          send_handle_obj->GetPointerFromInternalField(0));
      send_handle = send_handle_wrap->GetHandle();

      // Reference StreamWrap instance to prevent it from being garbage
      // collected before `AfterWrite` is called.
      if (handle_sym.IsEmpty()) {
        handle_sym = NODE_PSYMBOL("handle");
      }
      assert(!req_wrap->object_.IsEmpty());
      req_wrap->object_->Set(handle_sym, send_handle_obj);
    }

    r = uv_write2(&req_wrap->req_,
                  wrap->stream_,
                  &buf,
                  1,
                  reinterpret_cast<uv_stream_t*>(send_handle),
                  StreamWrap::AfterWrite);
  }

  req_wrap->Dispatched();
  req_wrap->object_->Set(bytes_sym, Number::New((uint32_t) data_size));

  wrap->UpdateWriteQueueSize();

  if (r) {
    SetErrno(uv_last_error(uv_default_loop()));
    req_wrap->~WriteWrap();
    delete[] storage;
    return scope.Close(v8::Null());
  } else {
    if (wrap->stream_->type == UV_TCP) {
      NODE_COUNT_NET_BYTES_SENT(buf.len);
    } else if (wrap->stream_->type == UV_NAMED_PIPE) {
      NODE_COUNT_PIPE_BYTES_SENT(buf.len);
    }

    return scope.Close(req_wrap->object_);
  }
}


Handle<Value> StreamWrap::WriteAsciiString(const Arguments& args) {
  return WriteStringImpl<kAscii>(args);
}


Handle<Value> StreamWrap::WriteUtf8String(const Arguments& args) {
  return WriteStringImpl<kUtf8>(args);
}


Handle<Value> StreamWrap::WriteUcs2String(const Arguments& args) {
  return WriteStringImpl<kUcs2>(args);
}


void StreamWrap::AfterWrite(uv_write_t* req, int status) {
  WriteWrap* req_wrap = (WriteWrap*) req->data;
  StreamWrap* wrap = (StreamWrap*) req->handle->data;

  HandleScope scope;

  // The wrap and request objects should still be there.
  assert(req_wrap->object_.IsEmpty() == false);
  assert(wrap->object_.IsEmpty() == false);

  // Unref handle property
  if (!handle_sym.IsEmpty()) {
    req_wrap->object_->Delete(handle_sym);
  }

  if (status) {
    SetErrno(uv_last_error(uv_default_loop()));
  }

  wrap->UpdateWriteQueueSize();

  Local<Value> argv[] = {
    Integer::New(status),
    Local<Value>::New(wrap->object_),
    Local<Value>::New(req_wrap->object_)
  };

  MakeCallback(req_wrap->object_, oncomplete_sym, ARRAY_SIZE(argv), argv);

  req_wrap->~WriteWrap();
  delete[] reinterpret_cast<char*>(req_wrap);
}


Handle<Value> StreamWrap::Shutdown(const Arguments& args) {
  HandleScope scope;

  UNWRAP(StreamWrap)

  ShutdownWrap* req_wrap = new ShutdownWrap();

  int r = uv_shutdown(&req_wrap->req_, wrap->stream_, AfterShutdown);

  req_wrap->Dispatched();

  if (r) {
    SetErrno(uv_last_error(uv_default_loop()));
    delete req_wrap;
    return scope.Close(v8::Null());
  } else {
    return scope.Close(req_wrap->object_);
  }
}


void StreamWrap::AfterShutdown(uv_shutdown_t* req, int status) {
  ReqWrap<uv_shutdown_t>* req_wrap = (ReqWrap<uv_shutdown_t>*) req->data;
  StreamWrap* wrap = (StreamWrap*) req->handle->data;

  // The wrap and request objects should still be there.
  assert(req_wrap->object_.IsEmpty() == false);
  assert(wrap->object_.IsEmpty() == false);

  HandleScope scope;

  if (status) {
    SetErrno(uv_last_error(uv_default_loop()));
  }

  Local<Value> argv[3] = {
    Integer::New(status),
    Local<Value>::New(wrap->object_),
    Local<Value>::New(req_wrap->object_)
  };

  MakeCallback(req_wrap->object_, oncomplete_sym, ARRAY_SIZE(argv), argv);

  delete req_wrap;
}


}
