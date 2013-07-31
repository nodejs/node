// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

#include "node.h"
#include "node_buffer.h"
#include "req_wrap.h"
#include "handle_wrap.h"
#include "stream_wrap.h"
#include "tty_wrap.h"
#include "node_wrap.h"

namespace node {

using v8::Array;
using v8::Function;
using v8::FunctionCallbackInfo;
using v8::FunctionTemplate;
using v8::Handle;
using v8::HandleScope;
using v8::Integer;
using v8::Local;
using v8::Object;
using v8::PropertyAttribute;
using v8::String;
using v8::Value;


void TTYWrap::Initialize(Handle<Object> target) {
  StreamWrap::Initialize(target);

  HandleScope scope(node_isolate);

  Local<FunctionTemplate> t = FunctionTemplate::New(New);
  t->SetClassName(String::NewSymbol("TTY"));

  t->InstanceTemplate()->SetInternalFieldCount(1);

  enum PropertyAttribute attributes =
      static_cast<PropertyAttribute>(v8::ReadOnly | v8::DontDelete);
  t->InstanceTemplate()->SetAccessor(String::New("fd"),
                                     StreamWrap::GetFD,
                                     NULL,
                                     Handle<Value>(),
                                     v8::DEFAULT,
                                     attributes);

  NODE_SET_PROTOTYPE_METHOD(t, "close", HandleWrap::Close);
  NODE_SET_PROTOTYPE_METHOD(t, "unref", HandleWrap::Unref);

  NODE_SET_PROTOTYPE_METHOD(t, "readStart", StreamWrap::ReadStart);
  NODE_SET_PROTOTYPE_METHOD(t, "readStop", StreamWrap::ReadStop);

  NODE_SET_PROTOTYPE_METHOD(t, "writeBuffer", StreamWrap::WriteBuffer);
  NODE_SET_PROTOTYPE_METHOD(t,
                            "writeAsciiString",
                            StreamWrap::WriteAsciiString);
  NODE_SET_PROTOTYPE_METHOD(t, "writeUtf8String", StreamWrap::WriteUtf8String);
  NODE_SET_PROTOTYPE_METHOD(t, "writeUcs2String", StreamWrap::WriteUcs2String);

  NODE_SET_PROTOTYPE_METHOD(t, "getWindowSize", TTYWrap::GetWindowSize);
  NODE_SET_PROTOTYPE_METHOD(t, "setRawMode", SetRawMode);

  NODE_SET_METHOD(target, "isTTY", IsTTY);
  NODE_SET_METHOD(target, "guessHandleType", GuessHandleType);

  ttyConstructorTmpl.Reset(node_isolate, t);
  target->Set(String::NewSymbol("TTY"), t->GetFunction());
}


TTYWrap* TTYWrap::Unwrap(Local<Object> obj) {
  assert(!obj.IsEmpty());
  assert(obj->InternalFieldCount() > 0);
  return static_cast<TTYWrap*>(obj->GetAlignedPointerFromInternalField(0));
}


uv_tty_t* TTYWrap::UVHandle() {
  return &handle_;
}


void TTYWrap::GuessHandleType(const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(node_isolate);
  int fd = args[0]->Int32Value();
  assert(fd >= 0);

  uv_handle_type t = uv_guess_handle(fd);
  const char* type = NULL;

  switch (t) {
  case UV_TCP: type = "TCP"; break;
  case UV_TTY: type = "TTY"; break;
  case UV_UDP: type = "UDP"; break;
  case UV_FILE: type = "FILE"; break;
  case UV_NAMED_PIPE: type = "PIPE"; break;
  case UV_UNKNOWN_HANDLE: type = "UNKNOWN"; break;
  default:
    abort();
  }

  args.GetReturnValue().Set(String::New(type));
}


void TTYWrap::IsTTY(const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(node_isolate);
  int fd = args[0]->Int32Value();
  assert(fd >= 0);
  bool rc = uv_guess_handle(fd) == UV_TTY;
  args.GetReturnValue().Set(rc);
}


void TTYWrap::GetWindowSize(const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(node_isolate);

  UNWRAP(TTYWrap)
  assert(args[0]->IsArray());

  int width, height;
  int err = uv_tty_get_winsize(&wrap->handle_, &width, &height);

  if (err == 0) {
    Local<v8::Array> a = args[0].As<Array>();
    a->Set(0, Integer::New(width, node_isolate));
    a->Set(1, Integer::New(height, node_isolate));
  }

  args.GetReturnValue().Set(err);
}


void TTYWrap::SetRawMode(const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(node_isolate);

  UNWRAP(TTYWrap)

  int err = uv_tty_set_mode(&wrap->handle_, args[0]->IsTrue());
  args.GetReturnValue().Set(err);
}


void TTYWrap::New(const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(node_isolate);

  // This constructor should not be exposed to public javascript.
  // Therefore we assert that we are not trying to call this as a
  // normal function.
  assert(args.IsConstructCall());

  int fd = args[0]->Int32Value();
  assert(fd >= 0);

  TTYWrap* wrap = new TTYWrap(args.This(), fd, args[1]->IsTrue());
  wrap->UpdateWriteQueueSize();
}


TTYWrap::TTYWrap(Handle<Object> object, int fd, bool readable)
    : StreamWrap(object, reinterpret_cast<uv_stream_t*>(&handle_)) {
  uv_tty_init(uv_default_loop(), &handle_, fd, readable);
}

}  // namespace node

NODE_MODULE(node_tty_wrap, node::TTYWrap::Initialize)
