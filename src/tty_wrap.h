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

#ifndef TTY_WRAP_H_
#define TTY_WRAP_H_

#include "handle_wrap.h"
#include "stream_wrap.h"

namespace node {

using v8::Object;
using v8::Handle;
using v8::Local;
using v8::Value;
using v8::Arguments;


class TTYWrap : StreamWrap {
 public:
  static void Initialize(Handle<Object> target);
  static TTYWrap* Unwrap(Local<Object> obj);

  uv_tty_t* UVHandle();

 private:
  TTYWrap(Handle<Object> object, int fd, bool readable);

  static Handle<Value> GuessHandleType(const Arguments& args);
  static Handle<Value> IsTTY(const Arguments& args);
  static Handle<Value> GetWindowSize(const Arguments& args);
  static Handle<Value> SetRawMode(const Arguments& args);
  static Handle<Value> New(const Arguments& args);

  uv_tty_t handle_;
};

} // namespace node

#endif // TTY_WRAP_H_
