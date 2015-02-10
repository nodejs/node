#include "stream_base.h"

#include "env.h"
#include "env-inl.h"
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


StreamBase::StreamBase(Environment* env, Local<Object> object) {
  CHECK_EQ(false, object.IsEmpty());
  CHECK_GT(object->InternalFieldCount(), 1);
  object->SetAlignedPointerInInternalField(1, this);
}


void StreamBase::AddMethods(Environment* env, Handle<FunctionTemplate> t) {
  HandleScope scope(env->isolate());

  enum PropertyAttribute attributes =
      static_cast<PropertyAttribute>(v8::ReadOnly | v8::DontDelete);
  t->InstanceTemplate()->SetAccessor(env->fd_string(),
                                     GetFD,
                                     nullptr,
                                     Handle<Value>(),
                                     v8::DEFAULT,
                                     attributes);

  env->SetProtoMethod(t, "readStart", JSMethod<&StreamBase::ReadStart>);
  env->SetProtoMethod(t, "readStop", JSMethod<&StreamBase::ReadStop>);
  env->SetProtoMethod(t, "shutdown", JSMethod<&StreamBase::Shutdown>);

  env->SetProtoMethod(t, "writev", JSMethod<&StreamBase::Writev>);
  env->SetProtoMethod(t, "writeBuffer", JSMethod<&StreamBase::WriteBuffer>);
  env->SetProtoMethod(t,
                      "writeAsciiString",
                      JSMethod<&StreamBase::WriteAsciiString>);
  env->SetProtoMethod(t,
                      "writeUtf8String",
                      JSMethod<&StreamBase::WriteUtf8String>);
  env->SetProtoMethod(t,
                      "writeUcs2String",
                      JSMethod<&StreamBase::WriteUcs2String>);
  env->SetProtoMethod(t,
                      "writeBinaryString",
                      JSMethod<&StreamBase::WriteBinaryString>);
  env->SetProtoMethod(t, "setBlocking", JSMethod<&StreamBase::SetBlocking>);
}


inline StreamBase* Unwrap(Local<Object> object) {
  CHECK_EQ(false, object.IsEmpty());
  CHECK_GT(object->InternalFieldCount(), 1);
  void* pointer = object->GetAlignedPointerFromInternalField(1);
  return static_cast<StreamBase*>(pointer);
}


void StreamBase::GetFD(Local<String>, const PropertyCallbackInfo<Value>& args) {
  HandleScope scope(args.GetIsolate());
  StreamBase* wrap = Unwrap(args.Holder());
  if (!wrap->IsAlive())
    return args.GetReturnValue().Set(UV_EINVAL);

  args.GetReturnValue().Set(wrap->GetFD());
}


template <int (StreamBase::*Method)(const FunctionCallbackInfo<Value>& args)>
void StreamBase::JSMethod(const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(args.GetIsolate());
  StreamBase* wrap = Unwrap(args.Holder());
  if (!wrap->IsAlive())
    return args.GetReturnValue().Set(UV_EINVAL);

  args.GetReturnValue().Set((wrap->*Method)(args));
}


}  // namespace node
