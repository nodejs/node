#ifndef SRC_STREAM_BASE_INL_H_
#define SRC_STREAM_BASE_INL_H_

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

template <class Base>
void StreamBase::AddMethods(Environment* env,
                            Local<FunctionTemplate> t,
                            int flags) {
  HandleScope scope(env->isolate());

  enum PropertyAttribute attributes =
      static_cast<PropertyAttribute>(v8::ReadOnly | v8::DontDelete);
  t->InstanceTemplate()->SetAccessor(env->fd_string(),
                                     GetFD<Base>,
                                     nullptr,
                                     env->as_external(),
                                     v8::DEFAULT,
                                     attributes);

  t->InstanceTemplate()->SetAccessor(env->external_stream_string(),
                                     GetExternal<Base>,
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
                      "writeBinaryString",
                      JSMethod<Base, &StreamBase::WriteString<BINARY> >);
}


template <class Base>
void StreamBase::GetFD(Local<String> key,
                       const PropertyCallbackInfo<Value>& args) {
  StreamBase* wrap = Unwrap<Base>(args.Holder());

  if (!wrap->IsAlive())
    return args.GetReturnValue().Set(UV_EINVAL);

  args.GetReturnValue().Set(wrap->GetFD());
}


template <class Base>
void StreamBase::GetExternal(Local<String> key,
                             const PropertyCallbackInfo<Value>& args) {
  StreamBase* wrap = Unwrap<Base>(args.Holder());

  Local<External> ext = External::New(args.GetIsolate(), wrap);
  args.GetReturnValue().Set(ext);
}


template <class Base,
          int (StreamBase::*Method)(const FunctionCallbackInfo<Value>& args)>
void StreamBase::JSMethod(const FunctionCallbackInfo<Value>& args) {
  StreamBase* wrap = Unwrap<Base>(args.Holder());

  if (!wrap->IsAlive())
    return args.GetReturnValue().Set(UV_EINVAL);

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

#endif  // SRC_STREAM_BASE_INL_H_
