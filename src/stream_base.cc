#include "stream_base-inl.h"
#include "stream_wrap.h"

#include "node.h"
#include "node_buffer.h"
#include "env-inl.h"
#include "js_stream.h"
#include "string_bytes.h"
#include "util-inl.h"
#include "v8.h"

#include <limits.h>  // INT_MAX

namespace node {

using v8::Array;
using v8::Context;
using v8::FunctionCallbackInfo;
using v8::HandleScope;
using v8::Integer;
using v8::Local;
using v8::Number;
using v8::Object;
using v8::String;
using v8::Value;

template int StreamBase::WriteString<ASCII>(
    const FunctionCallbackInfo<Value>& args);
template int StreamBase::WriteString<UTF8>(
    const FunctionCallbackInfo<Value>& args);
template int StreamBase::WriteString<UCS2>(
    const FunctionCallbackInfo<Value>& args);
template int StreamBase::WriteString<LATIN1>(
    const FunctionCallbackInfo<Value>& args);


int StreamBase::ReadStart(const FunctionCallbackInfo<Value>& args) {
  return ReadStart();
}


int StreamBase::ReadStop(const FunctionCallbackInfo<Value>& args) {
  return ReadStop();
}


int StreamBase::Shutdown(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  CHECK(args[0]->IsObject());
  Local<Object> req_wrap_obj = args[0].As<Object>();

  AsyncWrap* wrap = GetAsyncWrap();
  CHECK_NE(wrap, nullptr);
  AsyncHooks::DefaultTriggerAsyncIdScope(env, wrap->get_async_id());
  ShutdownWrap* req_wrap = new ShutdownWrap(env,
                                            req_wrap_obj,
                                            this);

  int err = DoShutdown(req_wrap);
  if (err)
    delete req_wrap;
  return err;
}


void StreamBase::AfterShutdown(ShutdownWrap* req_wrap, int status) {
  Environment* env = req_wrap->env();

  // The wrap and request objects should still be there.
  CHECK_EQ(req_wrap->persistent().IsEmpty(), false);

  HandleScope handle_scope(env->isolate());
  Context::Scope context_scope(env->context());

  Local<Object> req_wrap_obj = req_wrap->object();
  Local<Value> argv[3] = {
    Integer::New(env->isolate(), status),
    GetObject(),
    req_wrap_obj
  };

  if (req_wrap_obj->Has(env->context(), env->oncomplete_string()).FromJust())
    req_wrap->MakeCallback(env->oncomplete_string(), arraysize(argv), argv);

  delete req_wrap;
}


int StreamBase::Writev(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  CHECK(args[0]->IsObject());
  CHECK(args[1]->IsArray());

  Local<Object> req_wrap_obj = args[0].As<Object>();
  Local<Array> chunks = args[1].As<Array>();
  bool all_buffers = args[2]->IsTrue();

  size_t count;
  if (all_buffers)
    count = chunks->Length();
  else
    count = chunks->Length() >> 1;

  MaybeStackBuffer<uv_buf_t, 16> bufs(count);
  uv_buf_t* buf_list = *bufs;

  size_t storage_size = 0;
  uint32_t bytes = 0;
  size_t offset;
  WriteWrap* req_wrap;
  int err;

  if (!all_buffers) {
    // Determine storage size first
    for (size_t i = 0; i < count; i++) {
      storage_size = ROUND_UP(storage_size, WriteWrap::kAlignSize);

      Local<Value> chunk = chunks->Get(i * 2);

      if (Buffer::HasInstance(chunk))
        continue;
        // Buffer chunk, no additional storage required

      // String chunk
      Local<String> string = chunk->ToString(env->context()).ToLocalChecked();
      enum encoding encoding = ParseEncoding(env->isolate(),
                                             chunks->Get(i * 2 + 1));
      size_t chunk_size;
      if (encoding == UTF8 && string->Length() > 65535)
        chunk_size = StringBytes::Size(env->isolate(), string, encoding);
      else
        chunk_size = StringBytes::StorageSize(env->isolate(), string, encoding);

      storage_size += chunk_size;
    }

    if (storage_size > INT_MAX)
      return UV_ENOBUFS;
  } else {
    for (size_t i = 0; i < count; i++) {
      Local<Value> chunk = chunks->Get(i);
      bufs[i].base = Buffer::Data(chunk);
      bufs[i].len = Buffer::Length(chunk);
      bytes += bufs[i].len;
    }

    // Try writing immediately without allocation
    err = DoTryWrite(&buf_list, &count);
    if (err != 0 || count == 0)
      goto done;
  }

  {
    AsyncWrap* wrap = GetAsyncWrap();
    CHECK_NE(wrap, nullptr);
    AsyncHooks::DefaultTriggerAsyncIdScope trigger_scope(env,
                                                         wrap->get_async_id());
    req_wrap = WriteWrap::New(env, req_wrap_obj, this, storage_size);
  }

  offset = 0;
  if (!all_buffers) {
    for (size_t i = 0; i < count; i++) {
      Local<Value> chunk = chunks->Get(i * 2);

      // Write buffer
      if (Buffer::HasInstance(chunk)) {
        bufs[i].base = Buffer::Data(chunk);
        bufs[i].len = Buffer::Length(chunk);
        bytes += bufs[i].len;
        continue;
      }

      // Write string
      offset = ROUND_UP(offset, WriteWrap::kAlignSize);
      CHECK_LE(offset, storage_size);
      char* str_storage = req_wrap->Extra(offset);
      size_t str_size = storage_size - offset;

      Local<String> string = chunk->ToString(env->context()).ToLocalChecked();
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

    err = DoTryWrite(&buf_list, &count);
    if (err != 0 || count == 0) {
      req_wrap->Dispatched();
      req_wrap->Dispose();
      goto done;
    }
  }

  err = DoWrite(req_wrap, buf_list, count, nullptr);
  req_wrap_obj->Set(env->async(), True(env->isolate()));

  if (err)
    req_wrap->Dispose();

 done:
  const char* msg = Error();
  if (msg != nullptr) {
    req_wrap_obj->Set(env->error_string(), OneByteString(env->isolate(), msg));
    ClearError();
  }
  req_wrap_obj->Set(env->bytes_string(), Number::New(env->isolate(), bytes));

  return err;
}




int StreamBase::WriteBuffer(const FunctionCallbackInfo<Value>& args) {
  CHECK(args[0]->IsObject());

  Environment* env = Environment::GetCurrent(args);

  if (!args[1]->IsUint8Array()) {
    env->ThrowTypeError("Second argument must be a buffer");
    return 0;
  }

  Local<Object> req_wrap_obj = args[0].As<Object>();
  const char* data = Buffer::Data(args[1]);
  size_t length = Buffer::Length(args[1]);

  WriteWrap* req_wrap;
  uv_buf_t buf;
  buf.base = const_cast<char*>(data);
  buf.len = length;

  // Try writing immediately without allocation
  uv_buf_t* bufs = &buf;
  size_t count = 1;
  int err = DoTryWrite(&bufs, &count);
  if (err != 0)
    goto done;
  if (count == 0)
    goto done;
  CHECK_EQ(count, 1);

  // Allocate, or write rest
  {
    AsyncWrap* wrap = GetAsyncWrap();
    CHECK_NE(wrap, nullptr);
    AsyncHooks::DefaultTriggerAsyncIdScope trigger_scope(env,
                                                         wrap->get_async_id());
    req_wrap = WriteWrap::New(env, req_wrap_obj, this);
  }

  err = DoWrite(req_wrap, bufs, count, nullptr);
  req_wrap_obj->Set(env->async(), True(env->isolate()));
  req_wrap_obj->Set(env->buffer_string(), args[1]);

  if (err)
    req_wrap->Dispose();

 done:
  const char* msg = Error();
  if (msg != nullptr) {
    req_wrap_obj->Set(env->error_string(), OneByteString(env->isolate(), msg));
    ClearError();
  }
  req_wrap_obj->Set(env->bytes_string(),
                    Integer::NewFromUnsigned(env->isolate(), length));
  return err;
}


template <enum encoding enc>
int StreamBase::WriteString(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  CHECK(args[0]->IsObject());
  CHECK(args[1]->IsString());

  Local<Object> req_wrap_obj = args[0].As<Object>();
  Local<String> string = args[1].As<String>();
  Local<Object> send_handle_obj;
  if (args[2]->IsObject())
    send_handle_obj = args[2].As<Object>();

  int err;

  // Compute the size of the storage that the string will be flattened into.
  // For UTF8 strings that are very long, go ahead and take the hit for
  // computing their actual size, rather than tripling the storage.
  size_t storage_size;
  if (enc == UTF8 && string->Length() > 65535)
    storage_size = StringBytes::Size(env->isolate(), string, enc);
  else
    storage_size = StringBytes::StorageSize(env->isolate(), string, enc);

  if (storage_size > INT_MAX)
    return UV_ENOBUFS;

  // Try writing immediately if write size isn't too big
  WriteWrap* req_wrap;
  char* data;
  char stack_storage[16384];  // 16kb
  size_t data_size;
  uv_buf_t buf;

  bool try_write = storage_size <= sizeof(stack_storage) &&
                   (!IsIPCPipe() || send_handle_obj.IsEmpty());
  if (try_write) {
    data_size = StringBytes::Write(env->isolate(),
                                   stack_storage,
                                   storage_size,
                                   string,
                                   enc);
    buf = uv_buf_init(stack_storage, data_size);

    uv_buf_t* bufs = &buf;
    size_t count = 1;
    err = DoTryWrite(&bufs, &count);

    // Failure
    if (err != 0)
      goto done;

    // Success
    if (count == 0)
      goto done;

    // Partial write
    CHECK_EQ(count, 1);
  }

  {
    AsyncWrap* wrap = GetAsyncWrap();
    CHECK_NE(wrap, nullptr);
    AsyncHooks::DefaultTriggerAsyncIdScope trigger_scope(env,
                                                         wrap->get_async_id());
    req_wrap = WriteWrap::New(env, req_wrap_obj, this, storage_size);
  }

  data = req_wrap->Extra();

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
                                   enc);
  }

  CHECK_LE(data_size, storage_size);

  buf = uv_buf_init(data, data_size);

  if (!IsIPCPipe()) {
    err = DoWrite(req_wrap, &buf, 1, nullptr);
  } else {
    uv_handle_t* send_handle = nullptr;

    if (!send_handle_obj.IsEmpty()) {
      HandleWrap* wrap;
      ASSIGN_OR_RETURN_UNWRAP(&wrap, send_handle_obj, UV_EINVAL);
      send_handle = wrap->GetHandle();
      // Reference LibuvStreamWrap instance to prevent it from being garbage
      // collected before `AfterWrite` is called.
      CHECK_EQ(false, req_wrap->persistent().IsEmpty());
      req_wrap_obj->Set(env->handle_string(), send_handle_obj);
    }

    err = DoWrite(
        req_wrap,
        &buf,
        1,
        reinterpret_cast<uv_stream_t*>(send_handle));
  }

  req_wrap_obj->Set(env->async(), True(env->isolate()));

  if (err)
    req_wrap->Dispose();

 done:
  const char* msg = Error();
  if (msg != nullptr) {
    req_wrap_obj->Set(env->error_string(), OneByteString(env->isolate(), msg));
    ClearError();
  }
  req_wrap_obj->Set(env->bytes_string(),
                    Integer::NewFromUnsigned(env->isolate(), data_size));
  return err;
}


void StreamBase::AfterWrite(WriteWrap* req_wrap, int status) {
  Environment* env = req_wrap->env();

  HandleScope handle_scope(env->isolate());
  Context::Scope context_scope(env->context());

  // The wrap and request objects should still be there.
  CHECK_EQ(req_wrap->persistent().IsEmpty(), false);

  // Unref handle property
  Local<Object> req_wrap_obj = req_wrap->object();
  req_wrap_obj->Delete(env->context(), env->handle_string()).FromJust();
  EmitAfterWrite(req_wrap, status);

  Local<Value> argv[] = {
    Integer::New(env->isolate(), status),
    GetObject(),
    req_wrap_obj,
    Undefined(env->isolate())
  };

  const char* msg = Error();
  if (msg != nullptr) {
    argv[3] = OneByteString(env->isolate(), msg);
    ClearError();
  }

  if (req_wrap_obj->Has(env->context(), env->oncomplete_string()).FromJust())
    req_wrap->MakeCallback(env->oncomplete_string(), arraysize(argv), argv);

  req_wrap->Dispose();
}


void StreamBase::EmitData(ssize_t nread,
                          Local<Object> buf,
                          Local<Object> handle) {
  Environment* env = env_;

  Local<Value> argv[] = {
    Integer::New(env->isolate(), nread),
    buf,
    handle
  };

  if (argv[1].IsEmpty())
    argv[1] = Undefined(env->isolate());

  if (argv[2].IsEmpty())
    argv[2] = Undefined(env->isolate());

  AsyncWrap* wrap = GetAsyncWrap();
  CHECK_NE(wrap, nullptr);
  wrap->MakeCallback(env->onread_string(), arraysize(argv), argv);
}


bool StreamBase::IsIPCPipe() {
  return false;
}


int StreamBase::GetFD() {
  return -1;
}


Local<Object> StreamBase::GetObject() {
  return GetAsyncWrap()->object();
}


int StreamResource::DoTryWrite(uv_buf_t** bufs, size_t* count) {
  // No TryWrite by default
  return 0;
}


const char* StreamResource::Error() const {
  return nullptr;
}


void StreamResource::ClearError() {
  // No-op
}

}  // namespace node
