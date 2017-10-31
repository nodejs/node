#ifndef SRC_STREAM_BASE_INL_H_
#define SRC_STREAM_BASE_INL_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "stream_base.h"

#include "node.h"
#include "env.h"
#include "env-inl.h"
#include "v8.h"

namespace node {

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

template <class Base>
void StreamBase::AddMethods(Environment* env,
                            Local<FunctionTemplate> t,
                            int flags) {
  HandleScope scope(env->isolate());

  enum PropertyAttribute attributes =
      static_cast<PropertyAttribute>(v8::ReadOnly | v8::DontDelete);
  t->PrototypeTemplate()->SetAccessor(env->fd_string(),
                                      GetFD<Base>,
                                      nullptr,
                                      env->as_external(),
                                      v8::DEFAULT,
                                      attributes);

  t->PrototypeTemplate()->SetAccessor(env->external_stream_string(),
                                      GetExternal<Base>,
                                      nullptr,
                                      env->as_external(),
                                      v8::DEFAULT,
                                      attributes);

  t->PrototypeTemplate()->SetAccessor(env->bytes_read_string(),
                                      GetBytesRead<Base>,
                                      nullptr,
                                      env->as_external(),
                                      v8::DEFAULT,
                                      attributes);

  env->SetProtoMethod(t, "readStart", JSMethod<Base, &StreamBase::ReadStart>);
  env->SetProtoMethod(t, "readStop", JSMethod<Base, &StreamBase::ReadStop>);
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
void StreamBase::GetFD(Local<String> key,
                       const PropertyCallbackInfo<Value>& args) {
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
void StreamBase::GetBytesRead(Local<String> key,
                              const PropertyCallbackInfo<Value>& args) {
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
void StreamBase::GetExternal(Local<String> key,
                             const PropertyCallbackInfo<Value>& args) {
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

  AsyncHooks::InitScope init_scope(handle->env(), handle->get_async_id());
  args.GetReturnValue().Set((wrap->*Method)(args));
}


WriteWrap* WriteWrap::New(Environment* env,
                          Local<Object> obj,
                          StreamBase* wrap,
                          DoneCb cb,
                          size_t extra) {
  size_t storage_size = ROUND_UP(sizeof(WriteWrap), kAlignSize) + extra;
  char* storage = new char[storage_size];

  return new(storage) WriteWrap(env, obj, wrap, cb, storage_size);
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

}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_STREAM_BASE_INL_H_
