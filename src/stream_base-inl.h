#ifndef SRC_STREAM_BASE_INL_H_
#define SRC_STREAM_BASE_INL_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "stream_base.h"

#include "node.h"
#include "env-inl.h"
#include "v8.h"

namespace node {

using v8::Signature;
using v8::External;
using v8::FunctionCallbackInfo;
using v8::FunctionTemplate;
using v8::HandleScope;
using v8::Local;
using v8::Object;
using v8::PropertyAttribute;
using v8::PropertyCallbackInfo;
using v8::String;
using v8::Value;

using AsyncHooks = Environment::AsyncHooks;


inline StreamListener::~StreamListener() {
  if (stream_ != nullptr)
    stream_->RemoveStreamListener(this);
}

inline void StreamListener::PassReadErrorToPreviousListener(ssize_t nread) {
  CHECK_NE(previous_listener_, nullptr);
  previous_listener_->OnStreamRead(nread, uv_buf_init(nullptr, 0));
}


inline StreamResource::~StreamResource() {
  while (listener_ != nullptr) {
    listener_->OnStreamDestroy();
    RemoveStreamListener(listener_);
  }
}

inline void StreamResource::PushStreamListener(StreamListener* listener) {
  CHECK_NE(listener, nullptr);
  CHECK_EQ(listener->stream_, nullptr);

  listener->previous_listener_ = listener_;
  listener->stream_ = this;

  listener_ = listener;
}

inline void StreamResource::RemoveStreamListener(StreamListener* listener) {
  CHECK_NE(listener, nullptr);

  StreamListener* previous;
  StreamListener* current;

  // Remove from the linked list.
  for (current = listener_, previous = nullptr;
       /* No loop condition because we want a crash if listener is not found */
       ; previous = current, current = current->previous_listener_) {
    CHECK_NE(current, nullptr);
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


inline uv_buf_t StreamResource::EmitAlloc(size_t suggested_size) {
  return listener_->OnStreamAlloc(suggested_size);
}

inline void StreamResource::EmitRead(ssize_t nread, const uv_buf_t& buf) {
  if (nread > 0)
    bytes_read_ += static_cast<uint64_t>(nread);
  listener_->OnStreamRead(nread, buf);
}

inline void StreamResource::EmitAfterWrite(WriteWrap* w, int status) {
  listener_->OnStreamAfterWrite(w, status);
}


inline StreamBase::StreamBase(Environment* env) : env_(env) {
  PushStreamListener(&default_listener_);
}

inline Environment* StreamBase::stream_env() const {
  return env_;
}

template <class Base>
void StreamBase::AddMethods(Environment* env,
                            Local<FunctionTemplate> t,
                            int flags) {
  HandleScope scope(env->isolate());

  enum PropertyAttribute attributes =
      static_cast<PropertyAttribute>(
          v8::ReadOnly | v8::DontDelete | v8::DontEnum);

  Local<Signature> signature = Signature::New(env->isolate(), t);

  Local<FunctionTemplate> get_fd_templ =
      FunctionTemplate::New(env->isolate(),
                            GetFD<Base>,
                            env->as_external(),
                            signature);

  Local<FunctionTemplate> get_external_templ =
      FunctionTemplate::New(env->isolate(),
                            GetExternal<Base>,
                            env->as_external(),
                            signature);

  Local<FunctionTemplate> get_bytes_read_templ =
      FunctionTemplate::New(env->isolate(),
                            GetBytesRead<Base>,
                            env->as_external(),
                            signature);

  t->PrototypeTemplate()->SetAccessorProperty(env->fd_string(),
                                              get_fd_templ,
                                              Local<FunctionTemplate>(),
                                              attributes);

  t->PrototypeTemplate()->SetAccessorProperty(env->external_stream_string(),
                                              get_external_templ,
                                              Local<FunctionTemplate>(),
                                              attributes);

  t->PrototypeTemplate()->SetAccessorProperty(env->bytes_read_string(),
                                              get_bytes_read_templ,
                                              Local<FunctionTemplate>(),
                                              attributes);

  env->SetProtoMethod(t, "readStart", JSMethod<Base, &StreamBase::ReadStartJS>);
  env->SetProtoMethod(t, "readStop", JSMethod<Base, &StreamBase::ReadStopJS>);
  if ((flags & kFlagNoShutdown) == 0)
    env->SetProtoMethod(t, "shutdown", JSMethod<Base, &StreamBase::Shutdown>);
  if ((flags & kFlagHasWritev) != 0)
    env->SetProtoMethod(t, "writev", JSMethod<Base, &StreamBase::Writev>);
  env->SetProtoMethod(t,
                      "writeBuffer",
                      JSMethod<Base, &StreamBase::WriteBuffer>);
  env->SetProtoMethod(t,
                      "writeAsciiString",
                      JSMethod<Base, &StreamBase::WriteString<ASCII> >);
  env->SetProtoMethod(t,
                      "writeUtf8String",
                      JSMethod<Base, &StreamBase::WriteString<UTF8> >);
  env->SetProtoMethod(t,
                      "writeUcs2String",
                      JSMethod<Base, &StreamBase::WriteString<UCS2> >);
  env->SetProtoMethod(t,
                      "writeLatin1String",
                      JSMethod<Base, &StreamBase::WriteString<LATIN1> >);
}


template <class Base>
void StreamBase::GetFD(const FunctionCallbackInfo<Value>& args) {
  // Mimic implementation of StreamBase::GetFD() and UDPWrap::GetFD().
  Base* handle;
  ASSIGN_OR_RETURN_UNWRAP(&handle,
                          args.This(),
                          args.GetReturnValue().Set(UV_EINVAL));

  StreamBase* wrap = static_cast<StreamBase*>(handle);
  if (!wrap->IsAlive())
    return args.GetReturnValue().Set(UV_EINVAL);

  args.GetReturnValue().Set(wrap->GetFD());
}

template <class Base>
void StreamBase::GetBytesRead(const FunctionCallbackInfo<Value>& args) {
  // The handle instance hasn't been set. So no bytes could have been read.
  Base* handle;
  ASSIGN_OR_RETURN_UNWRAP(&handle,
                          args.This(),
                          args.GetReturnValue().Set(0));

  StreamBase* wrap = static_cast<StreamBase*>(handle);
  // uint64_t -> double. 53bits is enough for all real cases.
  args.GetReturnValue().Set(static_cast<double>(wrap->bytes_read_));
}

template <class Base>
void StreamBase::GetExternal(const FunctionCallbackInfo<Value>& args) {
  Base* handle;
  ASSIGN_OR_RETURN_UNWRAP(&handle, args.This());

  StreamBase* wrap = static_cast<StreamBase*>(handle);
  Local<External> ext = External::New(args.GetIsolate(), wrap);
  args.GetReturnValue().Set(ext);
}


template <class Base,
          int (StreamBase::*Method)(const FunctionCallbackInfo<Value>& args)>
void StreamBase::JSMethod(const FunctionCallbackInfo<Value>& args) {
  Base* handle;
  ASSIGN_OR_RETURN_UNWRAP(&handle, args.Holder());

  StreamBase* wrap = static_cast<StreamBase*>(handle);
  if (!wrap->IsAlive())
    return args.GetReturnValue().Set(UV_EINVAL);

  AsyncHooks::DefaultTriggerAsyncIdScope trigger_scope(
    handle->env(), handle->get_async_id());
  args.GetReturnValue().Set((wrap->*Method)(args));
}


inline void ShutdownWrap::OnDone(int status) {
  stream()->AfterShutdown(this, status);
}


WriteWrap* WriteWrap::New(Environment* env,
                          Local<Object> obj,
                          StreamBase* wrap,
                          size_t extra) {
  size_t storage_size = ROUND_UP(sizeof(WriteWrap), kAlignSize) + extra;
  char* storage = new char[storage_size];

  return new(storage) WriteWrap(env, obj, wrap, storage_size);
}


void WriteWrap::Dispose() {
  this->~WriteWrap();
  delete[] reinterpret_cast<char*>(this);
}


char* WriteWrap::Extra(size_t offset) {
  return reinterpret_cast<char*>(this) +
         ROUND_UP(sizeof(*this), kAlignSize) +
         offset;
}

size_t WriteWrap::ExtraSize() const {
  return storage_size_ - ROUND_UP(sizeof(*this), kAlignSize);
}

inline void WriteWrap::OnDone(int status) {
  stream()->AfterWrite(this, status);
}

}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_STREAM_BASE_INL_H_
