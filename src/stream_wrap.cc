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

#include <node.h>
#include <node_buffer.h>
#include <handle_wrap.h>
#include <stream_wrap.h>
#include <tcp_wrap.h>
#include <req_wrap.h>


namespace node {


#define SLAB_SIZE (1024 * 1024)
#define MIN(a, b) ((a) < (b) ? (a) : (b))


using v8::Object;
using v8::Handle;
using v8::Local;
using v8::Persistent;
using v8::Value;
using v8::HandleScope;
using v8::FunctionTemplate;
using v8::String;
using v8::Function;
using v8::TryCatch;
using v8::Context;
using v8::Arguments;
using v8::Integer;


#define UNWRAP \
  assert(!args.Holder().IsEmpty()); \
  assert(args.Holder()->InternalFieldCount() > 0); \
  StreamWrap* wrap =  \
      static_cast<StreamWrap*>(args.Holder()->GetPointerFromInternalField(0)); \
  if (!wrap) { \
    uv_err_t err; \
    err.code = UV_EBADF; \
    SetErrno(err); \
    return scope.Close(Integer::New(-1)); \
  }


typedef class ReqWrap<uv_shutdown_t> ShutdownWrap;
typedef class ReqWrap<uv_write_t> WriteWrap;


static size_t slab_used;
static uv_stream_t* handle_that_last_alloced;
static Persistent<String> slab_sym;
static Persistent<String> buffer_sym;
static Persistent<String> write_queue_size_sym;
static bool initialized;


void StreamWrap::Initialize(Handle<Object> target) {
  if (initialized) {
    return;
  } else {
    initialized = true;
  }

  HandleScope scope;

  HandleWrap::Initialize(target);

  slab_sym = Persistent<String>::New(String::NewSymbol("slab"));
  buffer_sym = Persistent<String>::New(String::NewSymbol("buffer"));
  write_queue_size_sym =
    Persistent<String>::New(String::NewSymbol("writeQueueSize"));
}


StreamWrap::StreamWrap(Handle<Object> object, uv_stream_t* stream)
    : HandleWrap(object, (uv_handle_t*)stream) {
  stream_ = stream;
  if (stream) {
    stream->data = this;
  }
}


void StreamWrap::SetHandle(uv_handle_t* h) {
  HandleWrap::SetHandle(h);
  stream_ = (uv_stream_t*)h;
  stream_->data = this;
}


void StreamWrap::UpdateWriteQueueSize() {
  HandleScope scope;
  object_->Set(write_queue_size_sym, Integer::New(stream_->write_queue_size));
}


Handle<Value> StreamWrap::ReadStart(const Arguments& args) {
  HandleScope scope;

  UNWRAP

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

  UNWRAP

  int r = uv_read_stop(wrap->stream_);

  // Error starting the tcp.
  if (r) SetErrno(uv_last_error(uv_default_loop()));

  return scope.Close(Integer::New(r));
}


char* StreamWrap::NewSlab(Handle<Object> global,
                          Handle<Object> wrap_obj) {
  HandleScope scope;
  Local<Value> arg = Integer::NewFromUnsigned(SLAB_SIZE);
  Local<Object> b = Buffer::constructor_template->GetFunction()->
    NewInstance(1, &arg);
  if (b.IsEmpty()) return NULL;
  global->SetHiddenValue(slab_sym, b);
  assert(Buffer::Length(b) == SLAB_SIZE);
  slab_used = 0;
  wrap_obj->SetHiddenValue(slab_sym, b);
  return Buffer::Data(b);
}


uv_buf_t StreamWrap::OnAlloc(uv_handle_t* handle, size_t suggested_size) {
  HandleScope scope;

  StreamWrap* wrap = static_cast<StreamWrap*>(handle->data);
  assert(wrap->stream_ == reinterpret_cast<uv_stream_t*>(handle));

  char* slab = NULL;

  Handle<Object> global = Context::GetCurrent()->Global();
  Local<Value> slab_v = global->GetHiddenValue(slab_sym);

  if (slab_v.IsEmpty()) {
    // No slab currently. Create a new one.
    slab = NewSlab(global, wrap->object_);
  } else {
    // Use existing slab.
    Local<Object> slab_obj = slab_v->ToObject();
    slab = Buffer::Data(slab_obj);
    assert(Buffer::Length(slab_obj) == SLAB_SIZE);
    assert(SLAB_SIZE >= slab_used);

    // If less than 64kb is remaining on the slab allocate a new one.
    if (SLAB_SIZE - slab_used < 64 * 1024) {
      slab = NewSlab(global, wrap->object_);
    } else {
      wrap->object_->SetHiddenValue(slab_sym, slab_obj);
    }
  }

  uv_buf_t buf;
  buf.base = slab + slab_used;
  buf.len = MIN(SLAB_SIZE - slab_used, suggested_size);

  wrap->slab_offset_ = slab_used;
  slab_used += buf.len;

  handle_that_last_alloced = reinterpret_cast<uv_stream_t*>(handle);

  return buf;
}


void StreamWrap::OnReadCommon(uv_stream_t* handle, ssize_t nread,
    uv_buf_t buf, uv_handle_type pending) {
  HandleScope scope;

  StreamWrap* wrap = static_cast<StreamWrap*>(handle->data);

  // We should not be getting this callback if someone as already called
  // uv_close() on the handle.
  assert(wrap->object_.IsEmpty() == false);

  // Remove the reference to the slab to avoid memory leaks;
  Local<Value> slab_v = wrap->object_->GetHiddenValue(slab_sym);
  wrap->object_->SetHiddenValue(slab_sym, v8::Null());

  if (nread < 0)  {
    // EOF or Error
    if (handle_that_last_alloced == handle) {
      slab_used -= buf.len;
    }

    SetErrno(uv_last_error(uv_default_loop()));
    MakeCallback(wrap->object_, "onread", 0, NULL);
    return;
  }

  assert(nread <= buf.len);

  if (handle_that_last_alloced == handle) {
    slab_used -= (buf.len - nread);
  }

  if (nread > 0) {
    int argc = 3;
    Local<Value> argv[4] = {
      slab_v,
      Integer::New(wrap->slab_offset_),
      Integer::New(nread)
    };


    if (pending == UV_TCP) {
      // Instantiate the client javascript object and handle.
      Local<Object> pending_obj = TCPWrap::Instantiate();

      // Unwrap the client javascript object.
      assert(pending_obj->InternalFieldCount() > 0);
      TCPWrap* pending_wrap =
          static_cast<TCPWrap*>(pending_obj->GetPointerFromInternalField(0));

      int r = uv_accept(handle, pending_wrap->GetStream());
      assert(r == 0);

      argv[3] = pending_obj;
      argc++;
    } else {
      // We only support sending UV_TCP right now.
      assert(pending == UV_UNKNOWN_HANDLE);
    }

    MakeCallback(wrap->object_, "onread", argc, argv);
  }
}


void StreamWrap::OnRead(uv_stream_t* handle, ssize_t nread, uv_buf_t buf) {
  OnReadCommon(handle, nread, buf, UV_UNKNOWN_HANDLE);
}


void StreamWrap::OnRead2(uv_pipe_t* handle, ssize_t nread, uv_buf_t buf,
    uv_handle_type pending) {
  OnReadCommon((uv_stream_t*)handle, nread, buf, pending);
}


Handle<Value> StreamWrap::Write(const Arguments& args) {
  HandleScope scope;

  UNWRAP

  bool ipc_pipe = wrap->stream_->type == UV_NAMED_PIPE &&
                  ((uv_pipe_t*)wrap->stream_)->ipc;

  // The first argument is a buffer.
  assert(Buffer::HasInstance(args[0]));
  Local<Object> buffer_obj = args[0]->ToObject();
  size_t offset = 0;
  size_t length = Buffer::Length(buffer_obj);

  if (args.Length() > 1) {
    offset = args[1]->IntegerValue();
  }

  if (args.Length() > 2) {
    length = args[2]->IntegerValue();
  }

  WriteWrap* req_wrap = new WriteWrap();

  req_wrap->object_->SetHiddenValue(buffer_sym, buffer_obj);

  uv_buf_t buf;
  buf.base = Buffer::Data(buffer_obj) + offset;
  buf.len = length;

  int r;

  if (!ipc_pipe) {
    r = uv_write(&req_wrap->req_, wrap->stream_, &buf, 1, StreamWrap::AfterWrite);
  } else {
    uv_stream_t* send_stream = NULL;

    if (args[3]->IsObject()) {
      Local<Object> send_stream_obj = args[3]->ToObject();
      assert(send_stream_obj->InternalFieldCount() > 0);
      StreamWrap* send_stream_wrap = static_cast<StreamWrap*>(
          send_stream_obj->GetPointerFromInternalField(0));
      send_stream = send_stream_wrap->GetStream();
    }

    r = uv_write2(&req_wrap->req_,
                  wrap->stream_,
                  &buf,
                  1,
                  send_stream,
                  StreamWrap::AfterWrite);
  }

  req_wrap->Dispatched();

  wrap->UpdateWriteQueueSize();

  if (r) {
    SetErrno(uv_last_error(uv_default_loop()));
    delete req_wrap;
    return scope.Close(v8::Null());
  } else {
    return scope.Close(req_wrap->object_);
  }
}


void StreamWrap::AfterWrite(uv_write_t* req, int status) {
  WriteWrap* req_wrap = (WriteWrap*) req->data;
  StreamWrap* wrap = (StreamWrap*) req->handle->data;

  HandleScope scope;

  // The wrap and request objects should still be there.
  assert(req_wrap->object_.IsEmpty() == false);
  assert(wrap->object_.IsEmpty() == false);

  if (status) {
    SetErrno(uv_last_error(uv_default_loop()));
  }

  wrap->UpdateWriteQueueSize();

  Local<Value> argv[4] = {
    Integer::New(status),
    Local<Value>::New(wrap->object_),
    Local<Value>::New(req_wrap->object_),
    req_wrap->object_->GetHiddenValue(buffer_sym),
  };

  MakeCallback(req_wrap->object_, "oncomplete", 4, argv);

  delete req_wrap;
}


Handle<Value> StreamWrap::Shutdown(const Arguments& args) {
  HandleScope scope;

  UNWRAP

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

  MakeCallback(req_wrap->object_, "oncomplete", 3, argv);

  delete req_wrap;
}


}
