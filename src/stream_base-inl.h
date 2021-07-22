#ifndef SRC_STREAM_BASE_INL_H_
#define SRC_STREAM_BASE_INL_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "allocated_buffer-inl.h"
#include "async_wrap-inl.h"
#include "base_object-inl.h"
#include "node.h"
#include "stream_base.h"
#include "v8.h"

namespace node {

StreamReq::StreamReq(
    StreamBase* stream,
    v8::Local<v8::Object> req_wrap_obj) : stream_(stream) {
  AttachToObject(req_wrap_obj);
}

void StreamReq::AttachToObject(v8::Local<v8::Object> req_wrap_obj) {
  CHECK_EQ(req_wrap_obj->GetAlignedPointerFromInternalField(
               StreamReq::kStreamReqField),
           nullptr);
  req_wrap_obj->SetAlignedPointerInInternalField(
      StreamReq::kStreamReqField, this);
}

StreamReq* StreamReq::FromObject(v8::Local<v8::Object> req_wrap_obj) {
  return static_cast<StreamReq*>(
      req_wrap_obj->GetAlignedPointerFromInternalField(
          StreamReq::kStreamReqField));
}

void StreamReq::Dispose() {
  BaseObjectPtr<AsyncWrap> destroy_me{GetAsyncWrap()};
  object()->SetAlignedPointerInInternalField(
      StreamReq::kStreamReqField, nullptr);
  destroy_me->Detach();
}

v8::Local<v8::Object> StreamReq::object() {
  return GetAsyncWrap()->object();
}

ShutdownWrap::ShutdownWrap(
    StreamBase* stream,
    v8::Local<v8::Object> req_wrap_obj)
    : StreamReq(stream, req_wrap_obj) { }

WriteWrap::WriteWrap(
    StreamBase* stream,
    v8::Local<v8::Object> req_wrap_obj)
    : StreamReq(stream, req_wrap_obj) { }

void StreamListener::PassReadErrorToPreviousListener(ssize_t nread) {
  CHECK_NOT_NULL(previous_listener_);
  previous_listener_->OnStreamRead(nread, uv_buf_init(nullptr, 0));
}

void StreamResource::PushStreamListener(StreamListener* listener) {
  CHECK_NOT_NULL(listener);
  CHECK_NULL(listener->stream_);

  listener->previous_listener_ = listener_;
  listener->stream_ = this;

  listener_ = listener;
}

void StreamResource::RemoveStreamListener(StreamListener* listener) {
  CHECK_NOT_NULL(listener);

  StreamListener* previous;
  StreamListener* current;

  // Remove from the linked list.
  for (current = listener_, previous = nullptr;
       /* No loop condition because we want a crash if listener is not found */
       ; previous = current, current = current->previous_listener_) {
    CHECK_NOT_NULL(current);
    if (current == listener) {
      if (previous != nullptr)
        previous->previous_listener_ = current->previous_listener_;
      else
        listener_ = listener->previous_listener_;
      break;
    }
  }

  listener->stream_ = nullptr;
  listener->previous_listener_ = nullptr;
}

uv_buf_t StreamResource::EmitAlloc(size_t suggested_size) {
  DebugSealHandleScope seal_handle_scope;
  return listener_->OnStreamAlloc(suggested_size);
}

void StreamResource::EmitRead(ssize_t nread, const uv_buf_t& buf) {
  DebugSealHandleScope seal_handle_scope;
  if (nread > 0)
    bytes_read_ += static_cast<uint64_t>(nread);
  listener_->OnStreamRead(nread, buf);
}

void StreamResource::EmitAfterWrite(WriteWrap* w, int status) {
  DebugSealHandleScope seal_handle_scope;
  listener_->OnStreamAfterWrite(w, status);
}

void StreamResource::EmitAfterShutdown(ShutdownWrap* w, int status) {
  DebugSealHandleScope seal_handle_scope;
  listener_->OnStreamAfterShutdown(w, status);
}

void StreamResource::EmitWantsWrite(size_t suggested_size) {
  DebugSealHandleScope seal_handle_scope;
  listener_->OnStreamWantsWrite(suggested_size);
}

StreamBase::StreamBase(Environment* env) : env_(env) {
  PushStreamListener(&default_listener_);
}

int StreamBase::Shutdown(v8::Local<v8::Object> req_wrap_obj) {
  Environment* env = stream_env();

  v8::HandleScope handle_scope(env->isolate());

  if (req_wrap_obj.IsEmpty()) {
    if (!env->shutdown_wrap_template()
             ->NewInstance(env->context())
             .ToLocal(&req_wrap_obj)) {
      return UV_EBUSY;
    }
    StreamReq::ResetObject(req_wrap_obj);
  }

  BaseObjectPtr<AsyncWrap> req_wrap_ptr;
  AsyncHooks::DefaultTriggerAsyncIdScope trigger_scope(GetAsyncWrap());
  ShutdownWrap* req_wrap = CreateShutdownWrap(req_wrap_obj);
  if (req_wrap != nullptr)
    req_wrap_ptr.reset(req_wrap->GetAsyncWrap());
  int err = DoShutdown(req_wrap);

  if (err != 0 && req_wrap != nullptr) {
    req_wrap->Dispose();
  }

  const char* msg = Error();
  if (msg != nullptr) {
    req_wrap_obj->Set(
        env->context(),
        env->error_string(), OneByteString(env->isolate(), msg)).Check();
    ClearError();
  }

  return err;
}

StreamWriteResult StreamBase::Write(
    uv_buf_t* bufs,
    size_t count,
    uv_stream_t* send_handle,
    v8::Local<v8::Object> req_wrap_obj) {
  Environment* env = stream_env();
  int err;

  size_t total_bytes = 0;
  for (size_t i = 0; i < count; ++i)
    total_bytes += bufs[i].len;
  bytes_written_ += total_bytes;

  if (send_handle == nullptr) {
    err = DoTryWrite(&bufs, &count);
    if (err != 0 || count == 0) {
      return StreamWriteResult { false, err, nullptr, total_bytes, {} };
    }
  }

  v8::HandleScope handle_scope(env->isolate());

  if (req_wrap_obj.IsEmpty()) {
    if (!env->write_wrap_template()
             ->NewInstance(env->context())
             .ToLocal(&req_wrap_obj)) {
      return StreamWriteResult { false, UV_EBUSY, nullptr, 0, {} };
    }
    StreamReq::ResetObject(req_wrap_obj);
  }

  AsyncHooks::DefaultTriggerAsyncIdScope trigger_scope(GetAsyncWrap());
  WriteWrap* req_wrap = CreateWriteWrap(req_wrap_obj);
  BaseObjectPtr<AsyncWrap> req_wrap_ptr(req_wrap->GetAsyncWrap());

  err = DoWrite(req_wrap, bufs, count, send_handle);
  bool async = err == 0;

  if (!async) {
    req_wrap->Dispose();
    req_wrap = nullptr;
  }

  const char* msg = Error();
  if (msg != nullptr) {
    req_wrap_obj->Set(env->context(),
                      env->error_string(),
                      OneByteString(env->isolate(), msg)).Check();
    ClearError();
  }

  return StreamWriteResult {
      async, err, req_wrap, total_bytes, std::move(req_wrap_ptr) };
}

template <typename OtherBase>
SimpleShutdownWrap<OtherBase>::SimpleShutdownWrap(
    StreamBase* stream,
    v8::Local<v8::Object> req_wrap_obj)
  : ShutdownWrap(stream, req_wrap_obj),
    OtherBase(stream->stream_env(),
              req_wrap_obj,
              AsyncWrap::PROVIDER_SHUTDOWNWRAP) {
}

template <typename OtherBase>
SimpleWriteWrap<OtherBase>::SimpleWriteWrap(
    StreamBase* stream,
    v8::Local<v8::Object> req_wrap_obj)
  : WriteWrap(stream, req_wrap_obj),
    OtherBase(stream->stream_env(),
              req_wrap_obj,
              AsyncWrap::PROVIDER_WRITEWRAP) {
}

void StreamBase::AttachToObject(v8::Local<v8::Object> obj) {
  obj->SetAlignedPointerInInternalField(
      StreamBase::kStreamBaseField, this);
}

StreamBase* StreamBase::FromObject(v8::Local<v8::Object> obj) {
  if (obj->GetAlignedPointerFromInternalField(StreamBase::kSlot) == nullptr)
    return nullptr;

  return static_cast<StreamBase*>(
      obj->GetAlignedPointerFromInternalField(
          StreamBase::kStreamBaseField));
}

WriteWrap* WriteWrap::FromObject(v8::Local<v8::Object> req_wrap_obj) {
  return static_cast<WriteWrap*>(StreamReq::FromObject(req_wrap_obj));
}

template <typename T, bool kIsWeak>
WriteWrap* WriteWrap::FromObject(
    const BaseObjectPtrImpl<T, kIsWeak>& base_obj) {
  if (!base_obj) return nullptr;
  return FromObject(base_obj->object());
}

ShutdownWrap* ShutdownWrap::FromObject(v8::Local<v8::Object> req_wrap_obj) {
  return static_cast<ShutdownWrap*>(StreamReq::FromObject(req_wrap_obj));
}

template <typename T, bool kIsWeak>
ShutdownWrap* ShutdownWrap::FromObject(
    const BaseObjectPtrImpl<T, kIsWeak>& base_obj) {
  if (!base_obj) return nullptr;
  return FromObject(base_obj->object());
}

void WriteWrap::SetAllocatedStorage(AllocatedBuffer&& storage) {
  CHECK_NULL(storage_.data());
  storage_ = std::move(storage);
}

void StreamReq::Done(int status, const char* error_str) {
  AsyncWrap* async_wrap = GetAsyncWrap();
  Environment* env = async_wrap->env();
  if (error_str != nullptr) {
    v8::HandleScope handle_scope(env->isolate());
    async_wrap->object()->Set(env->context(),
                              env->error_string(),
                              OneByteString(env->isolate(), error_str))
                              .Check();
  }

  OnDone(status);
}

void StreamReq::ResetObject(v8::Local<v8::Object> obj) {
  DCHECK_GT(obj->InternalFieldCount(), StreamReq::kStreamReqField);

  obj->SetAlignedPointerInInternalField(StreamReq::kSlot, nullptr);
  obj->SetAlignedPointerInInternalField(StreamReq::kStreamReqField, nullptr);
}

}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_STREAM_BASE_INL_H_
