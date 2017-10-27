#include "js_stream.h"

#include "async-wrap.h"
#include "env-inl.h"
#include "node_buffer.h"
#include "stream_base-inl.h"
#include "v8.h"

namespace node {

using v8::Array;
using v8::Context;
using v8::FunctionCallbackInfo;
using v8::FunctionTemplate;
using v8::HandleScope;
using v8::Local;
using v8::MaybeLocal;
using v8::Object;
using v8::String;
using v8::Value;


JSStream::JSStream(Environment* env, Local<Object> obj)
    : AsyncWrap(env, obj, AsyncWrap::PROVIDER_JSSTREAM),
      StreamBase(env) {
  node::Wrap(obj, this);
  MakeWeak<JSStream>(this);

  set_alloc_cb({ OnAllocImpl, this });
  set_read_cb({ OnReadImpl, this });
}


JSStream::~JSStream() {
}


void JSStream::OnAllocImpl(size_t size, uv_buf_t* buf, void* ctx) {
  buf->base = Malloc(size);
  buf->len = size;
}


void JSStream::OnReadImpl(ssize_t nread,
                          const uv_buf_t* buf,
                          uv_handle_type pending,
                          void* ctx) {
  JSStream* wrap = static_cast<JSStream*>(ctx);
  CHECK_NE(wrap, nullptr);
  Environment* env = wrap->env();
  HandleScope handle_scope(env->isolate());
  Context::Scope context_scope(env->context());

  if (nread < 0)  {
    if (buf != nullptr && buf->base != nullptr)
      free(buf->base);
    wrap->EmitData(nread, Local<Object>(), Local<Object>());
    return;
  }

  if (nread == 0) {
    if (buf->base != nullptr)
      free(buf->base);
    return;
  }

  CHECK_LE(static_cast<size_t>(nread), buf->len);
  char* base = node::Realloc(buf->base, nread);

  CHECK_EQ(pending, UV_UNKNOWN_HANDLE);

  Local<Object> obj = Buffer::New(env, base, nread).ToLocalChecked();
  wrap->EmitData(nread, obj, Local<Object>());
}


void* JSStream::Cast() {
  return static_cast<void*>(this);
}


AsyncWrap* JSStream::GetAsyncWrap() {
  return static_cast<AsyncWrap*>(this);
}


bool JSStream::IsAlive() {
  HandleScope scope(env()->isolate());
  Context::Scope context_scope(env()->context());
  v8::Local<v8::Value> fn = object()->Get(env()->isalive_string());
  if (!fn->IsFunction())
    return false;
  return MakeCallback(fn.As<v8::Function>(), 0, nullptr)
      .ToLocalChecked()->IsTrue();
}


bool JSStream::IsClosing() {
  HandleScope scope(env()->isolate());
  Context::Scope context_scope(env()->context());
  return MakeCallback(env()->isclosing_string(), 0, nullptr)
      .ToLocalChecked()->IsTrue();
}


int JSStream::ReadStart() {
  HandleScope scope(env()->isolate());
  Context::Scope context_scope(env()->context());
  return MakeCallback(env()->onreadstart_string(), 0, nullptr)
      .ToLocalChecked()->Int32Value();
}


int JSStream::ReadStop() {
  HandleScope scope(env()->isolate());
  Context::Scope context_scope(env()->context());
  return MakeCallback(env()->onreadstop_string(), 0, nullptr)
      .ToLocalChecked()->Int32Value();
}


int JSStream::DoShutdown(ShutdownWrap* req_wrap) {
  HandleScope scope(env()->isolate());
  Context::Scope context_scope(env()->context());

  Local<Value> argv[] = {
    req_wrap->object()
  };

  req_wrap->Dispatched();
  MaybeLocal<Value> res =
      MakeCallback(env()->onshutdown_string(), arraysize(argv), argv);

  return res.ToLocalChecked()->Int32Value();
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

  w->Dispatched();
  MaybeLocal<Value> res =
      MakeCallback(env()->onwrite_string(), arraysize(argv), argv);

  return res.ToLocalChecked()->Int32Value();
}


void JSStream::New(const FunctionCallbackInfo<Value>& args) {
  // This constructor should not be exposed to public javascript.
  // Therefore we assert that we are not trying to call this as a
  // normal function.
  CHECK(args.IsConstructCall());
  Environment* env = Environment::GetCurrent(args);
  new JSStream(env, args.This());
}


void JSStream::DoAfterWrite(const FunctionCallbackInfo<Value>& args) {
  JSStream* wrap;
  CHECK(args[0]->IsObject());
  WriteWrap* w;
  ASSIGN_OR_RETURN_UNWRAP(&wrap, args.Holder());
  ASSIGN_OR_RETURN_UNWRAP(&w, args[0].As<Object>());

  wrap->OnAfterWrite(w);
}


template <class Wrap>
void JSStream::Finish(const FunctionCallbackInfo<Value>& args) {
  Wrap* w;
  CHECK(args[0]->IsObject());
  ASSIGN_OR_RETURN_UNWRAP(&w, args[0].As<Object>());

  w->Done(args[1]->Int32Value());
}


void JSStream::ReadBuffer(const FunctionCallbackInfo<Value>& args) {
  JSStream* wrap;
  ASSIGN_OR_RETURN_UNWRAP(&wrap, args.Holder());

  CHECK(Buffer::HasInstance(args[0]));
  char* data = Buffer::Data(args[0]);
  int len = Buffer::Length(args[0]);

  do {
    uv_buf_t buf;
    ssize_t avail = len;
    wrap->OnAlloc(len, &buf);
    if (static_cast<ssize_t>(buf.len) < avail)
      avail = buf.len;

    memcpy(buf.base, data, avail);
    data += avail;
    len -= avail;
    wrap->OnRead(avail, &buf);
  } while (len != 0);
}


void JSStream::EmitEOF(const FunctionCallbackInfo<Value>& args) {
  JSStream* wrap;
  ASSIGN_OR_RETURN_UNWRAP(&wrap, args.Holder());

  wrap->OnRead(UV_EOF, nullptr);
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

  env->SetProtoMethod(t, "doAfterWrite", DoAfterWrite);
  env->SetProtoMethod(t, "finishWrite", Finish<WriteWrap>);
  env->SetProtoMethod(t, "finishShutdown", Finish<ShutdownWrap>);
  env->SetProtoMethod(t, "readBuffer", ReadBuffer);
  env->SetProtoMethod(t, "emitEOF", EmitEOF);

  StreamBase::AddMethods<JSStream>(env, t, StreamBase::kFlagHasWritev);
  target->Set(jsStreamString, t->GetFunction());
}

}  // namespace node

NODE_MODULE_CONTEXT_AWARE_BUILTIN(js_stream, node::JSStream::Initialize)
