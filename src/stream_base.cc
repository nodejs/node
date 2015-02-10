#include "stream_base.h"

#include "node_buffer.h"
#include "env.h"
#include "env-inl.h"
#include "stream_wrap.h"
#include "util.h"
#include "util-inl.h"
#include "v8.h"

namespace node {

using v8::Array;
using v8::Context;
using v8::FunctionCallbackInfo;
using v8::FunctionTemplate;
using v8::Handle;
using v8::HandleScope;
using v8::Integer;
using v8::Local;
using v8::Object;
using v8::PropertyAttribute;
using v8::PropertyCallbackInfo;
using v8::String;
using v8::Value;

template void StreamBase::AddMethods<StreamWrap>(Environment* env,
                                                 Handle<FunctionTemplate> t);


StreamBase::StreamBase(Environment* env, Local<Object> object)
    : consumed_(false) {
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
  env->SetProtoMethod(t,
                      "setBlocking",
                      JSMethod<Base, &StreamBase::SetBlocking>);
}


template <class Base>
void StreamBase::GetFD(Local<String>, const PropertyCallbackInfo<Value>& args) {
  HandleScope scope(args.GetIsolate());
  Base* wrap = Unwrap<Base>(args.Holder());
  if (wrap->IsConsumed() || !wrap->IsAlive())
    return args.GetReturnValue().Set(UV_EINVAL);

  args.GetReturnValue().Set(wrap->GetFD());
}


template <class Base,
          int (StreamBase::*Method)(const FunctionCallbackInfo<Value>& args)>
void StreamBase::JSMethod(const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(args.GetIsolate());
  Base* wrap = Unwrap<Base>(args.Holder());
  if (wrap->IsConsumed() || !wrap->IsAlive())
    return args.GetReturnValue().Set(UV_EINVAL);

  args.GetReturnValue().Set((wrap->*Method)(args));
}


int StreamBase::ReadStart(const v8::FunctionCallbackInfo<v8::Value>& args) {
  return ReadStart();
}


int StreamBase::ReadStop(const v8::FunctionCallbackInfo<v8::Value>& args) {
  return ReadStop();
}


int StreamBase::Shutdown(const v8::FunctionCallbackInfo<v8::Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  CHECK(args[0]->IsObject());
  Local<Object> req_wrap_obj = args[0].As<Object>();

  ShutdownWrap* req_wrap = new ShutdownWrap(env, req_wrap_obj, this);
  int err = DoShutdown(req_wrap, AfterShutdown);
  req_wrap->Dispatched();
  if (err)
    delete req_wrap;
  return err;
}


void StreamBase::AfterShutdown(uv_shutdown_t* req, int status) {
  ShutdownWrap* req_wrap = static_cast<ShutdownWrap*>(req->data);
  StreamBase* wrap = req_wrap->wrap();
  Environment* env = req_wrap->env();

  // The wrap and request objects should still be there.
  CHECK_EQ(req_wrap->persistent().IsEmpty(), false);
  CHECK_EQ(wrap->GetObject().IsEmpty(), false);

  HandleScope handle_scope(env->isolate());
  Context::Scope context_scope(env->context());

  Local<Object> req_wrap_obj = req_wrap->object();
  Local<Value> argv[3] = {
    Integer::New(env->isolate(), status),
    wrap->GetObject(),
    req_wrap_obj
  };

  req_wrap->MakeCallback(env->oncomplete_string(), ARRAY_SIZE(argv), argv);

  delete req_wrap;
}


int StreamBase::Writev(const v8::FunctionCallbackInfo<v8::Value>& args) {
  CHECK(args[0]->IsObject());
  CHECK(args[1]->IsArray());
  return Writev(args[0].As<Object>(), args[1].As<Array>());
}


int StreamBase::WriteBuffer(const v8::FunctionCallbackInfo<v8::Value>& args) {
  CHECK(args[0]->IsObject());
  CHECK(Buffer::HasInstance(args[1]));
  Environment* env = Environment::GetCurrent(args);

  Local<Object> req_wrap_obj = args[0].As<Object>();
  const char* data = Buffer::Data(args[1]);
  size_t length = Buffer::Length(args[1]);

  char* storage;
  WriteWrap* req_wrap;
  uv_buf_t buf;
  buf.base = const_cast<char*>(data);
  buf.len = length;

  // Try writing immediately without allocation
  uv_buf_t* bufs = &buf;
  size_t count = 1;
  int err = TryWrite(&bufs, &count);
  if (err != 0)
    goto done;
  if (count == 0)
    goto done;
  CHECK_EQ(count, 1);

  // Allocate, or write rest
  storage = new char[sizeof(WriteWrap)];
  req_wrap = new(storage) WriteWrap(env, req_wrap_obj, this);

  err = DoWrite(req_wrap,
                bufs,
                count,
                nullptr,
                StreamBase::AfterWrite);
  req_wrap->Dispatched();
  req_wrap_obj->Set(env->async(), True(env->isolate()));

  if (err) {
    req_wrap->~WriteWrap();
    delete[] storage;
  }

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


template <enum encoding Encoding>
int StreamBase::WriteString(const v8::FunctionCallbackInfo<v8::Value>& args) {
  CHECK(args[0]->IsObject());
  CHECK(args[1]->IsString());
  Local<Object> handle;
  if (args[2]->IsObject())
    handle = args[2].As<Object>();
  return WriteString(args[0].As<Object>(),
                     args[1].As<String>(),
                     Encoding,
                     handle);
}


void StreamBase::AfterWrite(uv_write_t* req, int status) {
  WriteWrap* req_wrap = ContainerOf(&WriteWrap::req_, req);
  StreamBase* wrap = req_wrap->wrap();
  Environment* env = req_wrap->env();

  HandleScope handle_scope(env->isolate());
  Context::Scope context_scope(env->context());

  // The wrap and request objects should still be there.
  CHECK_EQ(req_wrap->persistent().IsEmpty(), false);
  CHECK_EQ(wrap->GetObject().IsEmpty(), false);

  // Unref handle property
  Local<Object> req_wrap_obj = req_wrap->object();
  req_wrap_obj->Delete(env->handle_string());
  wrap->DoAfterWrite(req_wrap);

  Local<Value> argv[] = {
    Integer::New(env->isolate(), status),
    wrap->GetObject(),
    req_wrap_obj,
    Undefined(env->isolate())
  };

  const char* msg = wrap->Error();
  if (msg != nullptr) {
    argv[3] = OneByteString(env->isolate(), msg);
    wrap->ClearError();
  }

  req_wrap->MakeCallback(env->oncomplete_string(), ARRAY_SIZE(argv), argv);

  req_wrap->~WriteWrap();
  delete[] reinterpret_cast<char*>(req_wrap);
}


int StreamBase::SetBlocking(const v8::FunctionCallbackInfo<v8::Value>& args) {
  CHECK_GT(args.Length(), 0);
  return SetBlocking(args[0]->IsTrue());
}

}  // namespace node
