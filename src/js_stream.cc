#include "js_stream.h"

#include "async_wrap.h"
#include "env-inl.h"
#include "node_buffer.h"
#include "stream_base-inl.h"
#include "node_errors.h"
#include "v8.h"

namespace node {

using errors::NodeTryCatch;

using v8::Array;
using v8::Context;
using v8::FunctionCallbackInfo;
using v8::FunctionTemplate;
using v8::HandleScope;
using v8::Local;
using v8::Object;
using v8::String;
using v8::Value;

JSStream::JSStream(Environment* env, Local<Object> obj)
    : AsyncWrap(env, obj, AsyncWrap::PROVIDER_JSSTREAM),
      StreamBase(env) {
  node::Wrap(obj, this);
  MakeWeak<JSStream>(this);
}


JSStream::~JSStream() {
}


AsyncWrap* JSStream::GetAsyncWrap() {
  return static_cast<AsyncWrap*>(this);
}


bool JSStream::IsAlive() {
  return true;
}


bool JSStream::IsClosing() {
  HandleScope scope(env()->isolate());
  Context::Scope context_scope(env()->context());
  NodeTryCatch try_catch(env());
  Local<Value> value;
  if (!MakeCallback(env()->isclosing_string(), 0, nullptr).ToLocal(&value)) {
    FatalException(env()->isolate(), try_catch);
    return true;
  }
  return value->IsTrue();
}


int JSStream::ReadStart() {
  HandleScope scope(env()->isolate());
  Context::Scope context_scope(env()->context());
  NodeTryCatch try_catch(env());
  Local<Value> value;
  int value_int = UV_EPROTO;
  if (!MakeCallback(env()->onreadstart_string(), 0, nullptr).ToLocal(&value) ||
      !value->Int32Value(env()->context()).To(&value_int)) {
    FatalException(env()->isolate(), try_catch);
  }
  return value_int;
}


int JSStream::ReadStop() {
  HandleScope scope(env()->isolate());
  Context::Scope context_scope(env()->context());
  NodeTryCatch try_catch(env());
  Local<Value> value;
  int value_int = UV_EPROTO;
  if (!MakeCallback(env()->onreadstop_string(), 0, nullptr).ToLocal(&value) ||
      !value->Int32Value(env()->context()).To(&value_int)) {
    FatalException(env()->isolate(), try_catch);
  }
  return value_int;
}


int JSStream::DoShutdown(ShutdownWrap* req_wrap) {
  HandleScope scope(env()->isolate());
  Context::Scope context_scope(env()->context());

  Local<Value> argv[] = {
    req_wrap->object()
  };

  NodeTryCatch try_catch(env());
  Local<Value> value;
  int value_int = UV_EPROTO;
  if (!MakeCallback(env()->onshutdown_string(),
                    arraysize(argv),
                    argv).ToLocal(&value) ||
      !value->Int32Value(env()->context()).To(&value_int)) {
    FatalException(env()->isolate(), try_catch);
  }
  return value_int;
}


int JSStream::DoWrite(WriteWrap* w,
                      uv_buf_t* bufs,
                      size_t count,
                      uv_stream_t* send_handle) {
  CHECK_EQ(send_handle, nullptr);

  HandleScope scope(env()->isolate());
  Context::Scope context_scope(env()->context());

  Local<Array> bufs_arr = Array::New(env()->isolate(), count);
  Local<Object> buf;
  for (size_t i = 0; i < count; i++) {
    buf = Buffer::Copy(env(), bufs[i].base, bufs[i].len).ToLocalChecked();
    bufs_arr->Set(i, buf);
  }

  Local<Value> argv[] = {
    w->object(),
    bufs_arr
  };

  NodeTryCatch try_catch(env());
  Local<Value> value;
  int value_int = UV_EPROTO;
  if (!MakeCallback(env()->onwrite_string(),
                    arraysize(argv),
                    argv).ToLocal(&value) ||
      !value->Int32Value(env()->context()).To(&value_int)) {
    FatalException(env()->isolate(), try_catch);
  }
  return value_int;
}


void JSStream::New(const FunctionCallbackInfo<Value>& args) {
  // This constructor should not be exposed to public javascript.
  // Therefore we assert that we are not trying to call this as a
  // normal function.
  CHECK(args.IsConstructCall());
  Environment* env = Environment::GetCurrent(args);
  new JSStream(env, args.This());
}


template <class Wrap>
void JSStream::Finish(const FunctionCallbackInfo<Value>& args) {
  CHECK(args[0]->IsObject());
  Wrap* w = static_cast<Wrap*>(StreamReq::FromObject(args[0].As<Object>()));

  w->Done(args[1]->Int32Value());
}


void JSStream::ReadBuffer(const FunctionCallbackInfo<Value>& args) {
  JSStream* wrap;
  ASSIGN_OR_RETURN_UNWRAP(&wrap, args.Holder());

  CHECK(Buffer::HasInstance(args[0]));
  char* data = Buffer::Data(args[0]);
  int len = Buffer::Length(args[0]);

  // Repeatedly ask the stream's owner for memory, copy the data that we
  // just read from JS into those buffers and emit them as reads.
  while (len != 0) {
    uv_buf_t buf = wrap->EmitAlloc(len);
    ssize_t avail = len;
    if (static_cast<ssize_t>(buf.len) < avail)
      avail = buf.len;

    memcpy(buf.base, data, avail);
    data += avail;
    len -= avail;
    wrap->EmitRead(avail, buf);
  }
}


void JSStream::EmitEOF(const FunctionCallbackInfo<Value>& args) {
  JSStream* wrap;
  ASSIGN_OR_RETURN_UNWRAP(&wrap, args.Holder());

  wrap->EmitRead(UV_EOF);
}


void JSStream::Initialize(Local<Object> target,
                          Local<Value> unused,
                          Local<Context> context) {
  Environment* env = Environment::GetCurrent(context);

  Local<FunctionTemplate> t = env->NewFunctionTemplate(New);
  Local<String> jsStreamString =
      FIXED_ONE_BYTE_STRING(env->isolate(), "JSStream");
  t->SetClassName(jsStreamString);
  t->InstanceTemplate()->SetInternalFieldCount(1);

  AsyncWrap::AddWrapMethods(env, t);

  env->SetProtoMethod(t, "finishWrite", Finish<WriteWrap>);
  env->SetProtoMethod(t, "finishShutdown", Finish<ShutdownWrap>);
  env->SetProtoMethod(t, "readBuffer", ReadBuffer);
  env->SetProtoMethod(t, "emitEOF", EmitEOF);

  StreamBase::AddMethods<JSStream>(env, t, StreamBase::kFlagHasWritev);
  target->Set(jsStreamString, t->GetFunction());
}

}  // namespace node

NODE_BUILTIN_MODULE_CONTEXT_AWARE(js_stream, node::JSStream::Initialize)
