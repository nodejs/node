#include "stream_base-inl.h"
#include "stream_wrap.h"

#include "node.h"
#include "node_buffer.h"
#include "node_errors.h"
#include "env-inl.h"
#include "js_stream.h"
#include "string_bytes.h"
#include "util-inl.h"
#include "v8.h"

#include <limits.h>  // INT_MAX

namespace node {

using v8::Array;
using v8::ArrayBuffer;
using v8::Context;
using v8::FunctionCallbackInfo;
using v8::HandleScope;
using v8::Integer;
using v8::Local;
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


int StreamBase::ReadStartJS(const FunctionCallbackInfo<Value>& args) {
  return ReadStart();
}


int StreamBase::ReadStopJS(const FunctionCallbackInfo<Value>& args) {
  return ReadStop();
}


int StreamBase::Shutdown(const FunctionCallbackInfo<Value>& args) {
  CHECK(args[0]->IsObject());
  Local<Object> req_wrap_obj = args[0].As<Object>();

  return Shutdown(req_wrap_obj);
}

void StreamBase::SetWriteResult(const StreamWriteResult& res) {
  env_->stream_base_state()[kBytesWritten] = res.bytes;
  env_->stream_base_state()[kLastWriteWasAsync] = res.async;
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

  size_t storage_size = 0;
  size_t offset;

  if (!all_buffers) {
    // Determine storage size first
    for (size_t i = 0; i < count; i++) {
      Local<Value> chunk = chunks->Get(env->context(), i * 2).ToLocalChecked();

      if (Buffer::HasInstance(chunk))
        continue;
        // Buffer chunk, no additional storage required

      // String chunk
      Local<String> string = chunk->ToString(env->context()).ToLocalChecked();
      enum encoding encoding = ParseEncoding(env->isolate(),
          chunks->Get(env->context(), i * 2 + 1).ToLocalChecked());
      size_t chunk_size;
      if (encoding == UTF8 && string->Length() > 65535 &&
          !StringBytes::Size(env->isolate(), string, encoding).To(&chunk_size))
        return 0;
      else if (!StringBytes::StorageSize(env->isolate(), string, encoding)
                    .To(&chunk_size))
        return 0;
      storage_size += chunk_size;
    }

    if (storage_size > INT_MAX)
      return UV_ENOBUFS;
  } else {
    for (size_t i = 0; i < count; i++) {
      Local<Value> chunk = chunks->Get(env->context(), i).ToLocalChecked();
      bufs[i].base = Buffer::Data(chunk);
      bufs[i].len = Buffer::Length(chunk);
    }
  }

  MallocedBuffer<char> storage;
  if (storage_size > 0)
    storage = MallocedBuffer<char>(storage_size);

  offset = 0;
  if (!all_buffers) {
    for (size_t i = 0; i < count; i++) {
      Local<Value> chunk = chunks->Get(env->context(), i * 2).ToLocalChecked();

      // Write buffer
      if (Buffer::HasInstance(chunk)) {
        bufs[i].base = Buffer::Data(chunk);
        bufs[i].len = Buffer::Length(chunk);
        continue;
      }

      // Write string
      CHECK_LE(offset, storage_size);
      char* str_storage = storage.data + offset;
      size_t str_size = storage_size - offset;

      Local<String> string = chunk->ToString(env->context()).ToLocalChecked();
      enum encoding encoding = ParseEncoding(env->isolate(),
          chunks->Get(env->context(), i * 2 + 1).ToLocalChecked());
      str_size = StringBytes::Write(env->isolate(),
                                    str_storage,
                                    str_size,
                                    string,
                                    encoding);
      bufs[i].base = str_storage;
      bufs[i].len = str_size;
      offset += str_size;
    }
  }

  StreamWriteResult res = Write(*bufs, count, nullptr, req_wrap_obj);
  SetWriteResult(res);
  if (res.wrap != nullptr && storage_size > 0) {
    res.wrap->SetAllocatedStorage(storage.release(), storage_size);
  }
  return res.err;
}


int StreamBase::WriteBuffer(const FunctionCallbackInfo<Value>& args) {
  CHECK(args[0]->IsObject());

  Environment* env = Environment::GetCurrent(args);

  if (!args[1]->IsUint8Array()) {
    node::THROW_ERR_INVALID_ARG_TYPE(env, "Second argument must be a buffer");
    return 0;
  }

  Local<Object> req_wrap_obj = args[0].As<Object>();

  uv_buf_t buf;
  buf.base = Buffer::Data(args[1]);
  buf.len = Buffer::Length(args[1]);

  StreamWriteResult res = Write(&buf, 1, nullptr, req_wrap_obj);
  SetWriteResult(res);

  return res.err;
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

  // Compute the size of the storage that the string will be flattened into.
  // For UTF8 strings that are very long, go ahead and take the hit for
  // computing their actual size, rather than tripling the storage.
  size_t storage_size;
  if (enc == UTF8 && string->Length() > 65535 &&
      !StringBytes::Size(env->isolate(), string, enc).To(&storage_size))
    return 0;
  else if (!StringBytes::StorageSize(env->isolate(), string, enc)
                .To(&storage_size))
    return 0;

  if (storage_size > INT_MAX)
    return UV_ENOBUFS;

  // Try writing immediately if write size isn't too big
  char stack_storage[16384];  // 16kb
  size_t data_size;
  size_t synchronously_written = 0;
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
    const int err = DoTryWrite(&bufs, &count);
    // Keep track of the bytes written here, because we're taking a shortcut
    // by using `DoTryWrite()` directly instead of using the utilities
    // provided by `Write()`.
    synchronously_written = count == 0 ? data_size : data_size - buf.len;
    bytes_written_ += synchronously_written;

    // Immediate failure or success
    if (err != 0 || count == 0) {
      SetWriteResult(StreamWriteResult { false, err, nullptr, data_size });
      return err;
    }

    // Partial write
    CHECK_EQ(count, 1);
  }

  MallocedBuffer<char> data;

  if (try_write) {
    // Copy partial data
    data = MallocedBuffer<char>(buf.len);
    memcpy(data.data, buf.base, buf.len);
    data_size = buf.len;
  } else {
    // Write it
    data = MallocedBuffer<char>(storage_size);
    data_size = StringBytes::Write(env->isolate(),
                                   data.data,
                                   storage_size,
                                   string,
                                   enc);
  }

  CHECK_LE(data_size, storage_size);

  buf = uv_buf_init(data.data, data_size);

  uv_stream_t* send_handle = nullptr;

  if (IsIPCPipe() && !send_handle_obj.IsEmpty()) {
    HandleWrap* wrap;
    ASSIGN_OR_RETURN_UNWRAP(&wrap, send_handle_obj, UV_EINVAL);
    send_handle = reinterpret_cast<uv_stream_t*>(wrap->GetHandle());
    // Reference LibuvStreamWrap instance to prevent it from being garbage
    // collected before `AfterWrite` is called.
    req_wrap_obj->Set(env->context(),
                      env->handle_string(),
                      send_handle_obj).FromJust();
  }

  StreamWriteResult res = Write(&buf, 1, send_handle, req_wrap_obj);
  res.bytes += synchronously_written;

  SetWriteResult(res);
  if (res.wrap != nullptr) {
    res.wrap->SetAllocatedStorage(data.release(), data_size);
  }

  return res.err;
}


void StreamBase::CallJSOnreadMethod(ssize_t nread,
                                    Local<ArrayBuffer> ab,
                                    size_t offset) {
  Environment* env = env_;

  DCHECK_EQ(static_cast<int32_t>(nread), nread);
  DCHECK_LE(offset, INT32_MAX);

  if (ab.IsEmpty()) {
    DCHECK_EQ(offset, 0);
    DCHECK_LE(nread, 0);
  } else {
    DCHECK_GE(nread, 0);
  }

  env->stream_base_state()[kReadBytesOrError] = nread;
  env->stream_base_state()[kArrayBufferOffset] = offset;

  Local<Value> argv[] = {
    ab.IsEmpty() ? Undefined(env->isolate()).As<Value>() : ab.As<Value>()
  };

  AsyncWrap* wrap = GetAsyncWrap();
  CHECK_NOT_NULL(wrap);
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


uv_buf_t StreamListener::OnStreamAlloc(size_t suggested_size) {
  return uv_buf_init(Malloc(suggested_size), suggested_size);
}


void EmitToJSStreamListener::OnStreamRead(ssize_t nread, const uv_buf_t& buf) {
  CHECK_NOT_NULL(stream_);
  StreamBase* stream = static_cast<StreamBase*>(stream_);
  Environment* env = stream->stream_env();
  HandleScope handle_scope(env->isolate());
  Context::Scope context_scope(env->context());

  if (nread <= 0)  {
    free(buf.base);
    if (nread < 0)
      stream->CallJSOnreadMethod(nread, Local<ArrayBuffer>());
    return;
  }

  CHECK_LE(static_cast<size_t>(nread), buf.len);
  char* base = Realloc(buf.base, nread);

  Local<ArrayBuffer> obj = ArrayBuffer::New(
      env->isolate(),
      base,
      nread,
      v8::ArrayBufferCreationMode::kInternalized);  // Transfer ownership to V8.
  stream->CallJSOnreadMethod(nread, obj);
}


void ReportWritesToJSStreamListener::OnStreamAfterReqFinished(
    StreamReq* req_wrap, int status) {
  StreamBase* stream = static_cast<StreamBase*>(stream_);
  Environment* env = stream->stream_env();
  AsyncWrap* async_wrap = req_wrap->GetAsyncWrap();
  HandleScope handle_scope(env->isolate());
  Context::Scope context_scope(env->context());
  CHECK(!async_wrap->persistent().IsEmpty());
  Local<Object> req_wrap_obj = async_wrap->object();

  Local<Value> argv[] = {
    Integer::New(env->isolate(), status),
    stream->GetObject(),
    Undefined(env->isolate())
  };

  const char* msg = stream->Error();
  if (msg != nullptr) {
    argv[2] = OneByteString(env->isolate(), msg);
    stream->ClearError();
  }

  if (req_wrap_obj->Has(env->context(), env->oncomplete_string()).FromJust())
    async_wrap->MakeCallback(env->oncomplete_string(), arraysize(argv), argv);
}

void ReportWritesToJSStreamListener::OnStreamAfterWrite(
    WriteWrap* req_wrap, int status) {
  OnStreamAfterReqFinished(req_wrap, status);
}

void ReportWritesToJSStreamListener::OnStreamAfterShutdown(
    ShutdownWrap* req_wrap, int status) {
  OnStreamAfterReqFinished(req_wrap, status);
}


}  // namespace node
