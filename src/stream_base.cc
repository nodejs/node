#include "stream_base.h"

#include "env.h"
#include "env-inl.h"
#include "stream_wrap.h"
#include "util.h"
#include "util-inl.h"
#include "v8.h"

namespace node {

using v8::FunctionCallbackInfo;
using v8::FunctionTemplate;
using v8::Handle;
using v8::HandleScope;
using v8::Local;
using v8::Object;
using v8::PropertyAttribute;
using v8::PropertyCallbackInfo;
using v8::String;
using v8::Value;

template void StreamBase::AddMethods<StreamWrap>(Environment* env,
                                                 Handle<FunctionTemplate> t);


StreamBase::StreamBase(Environment* env, Local<Object> object) {
}


template <class Base>
void StreamBase::AddMethods(Environment* env, Handle<FunctionTemplate> t) {
  HandleScope scope(env->isolate());

  enum PropertyAttribute attributes =
      static_cast<PropertyAttribute>(v8::ReadOnly | v8::DontDelete);
  t->InstanceTemplate()->SetAccessor(env->fd_string(),
                                     GetFD<Base>,
                                     nullptr,
                                     Handle<Value>(),
                                     v8::DEFAULT,
                                     attributes);

  env->SetProtoMethod(t, "readStart", JSMethod<Base, &StreamBase::ReadStart>);
  env->SetProtoMethod(t, "readStop", JSMethod<Base, &StreamBase::ReadStop>);
  env->SetProtoMethod(t, "shutdown", JSMethod<Base, &StreamBase::Shutdown>);
  env->SetProtoMethod(t, "writev", JSMethod<Base, &StreamBase::Writev>);
  env->SetProtoMethod(t,
                      "writeBuffer",
                      JSMethod<Base, &StreamBase::WriteBuffer>);
  env->SetProtoMethod(t,
                      "writeAsciiString",
                      JSMethod<Base, &StreamBase::WriteAsciiString>);
  env->SetProtoMethod(t,
                      "writeUtf8String",
                      JSMethod<Base, &StreamBase::WriteUtf8String>);
  env->SetProtoMethod(t,
                      "writeUcs2String",
                      JSMethod<Base, &StreamBase::WriteUcs2String>);
  env->SetProtoMethod(t,
                      "writeBinaryString",
                      JSMethod<Base, &StreamBase::WriteBinaryString>);
  env->SetProtoMethod(t,
                      "setBlocking",
                      JSMethod<Base, &StreamBase::SetBlocking>);
}


template <class Base>
void StreamBase::GetFD(Local<String>, const PropertyCallbackInfo<Value>& args) {
  HandleScope scope(args.GetIsolate());
  Base* wrap = Unwrap<Base>(args.Holder());
  if (!wrap->IsAlive())
    return args.GetReturnValue().Set(UV_EINVAL);

  args.GetReturnValue().Set(wrap->GetFD());
}


template <class Base,
          int (StreamBase::*Method)(const FunctionCallbackInfo<Value>& args)>
void StreamBase::JSMethod(const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(args.GetIsolate());
  Base* wrap = Unwrap<Base>(args.Holder());
  if (!wrap->IsAlive())
    return args.GetReturnValue().Set(UV_EINVAL);

  args.GetReturnValue().Set((wrap->*Method)(args));
}


}  // namespace node
