#ifndef SRC_STREAM_BASE_INL_H_
#define SRC_STREAM_BASE_INL_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

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

void WriteWrap::SetBackingStore(std::unique_ptr<v8::BackingStore> bs) {
  CHECK(!backing_store_);
  backing_store_ = std::move(bs);
}

void StreamReq::ResetObject(v8::Local<v8::Object> obj) {
  DCHECK_GT(obj->InternalFieldCount(), StreamReq::kStreamReqField);

  obj->SetAlignedPointerInInternalField(StreamReq::kSlot, nullptr);
  obj->SetAlignedPointerInInternalField(StreamReq::kStreamReqField, nullptr);
}

}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_STREAM_BASE_INL_H_
