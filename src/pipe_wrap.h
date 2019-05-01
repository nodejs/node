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

#ifndef SRC_PIPE_WRAP_H_
#define SRC_PIPE_WRAP_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "async_wrap.h"
#include "connection_wrap.h"
#include "env.h"

using v8::Context;
using v8::Isolate;
using v8::Local;
using v8::MaybeLocal;
using v8::Object;
using v8::FunctionCallbackInfo;
using v8::Value;

namespace node {

class PipeWrap : public ConnectionWrap<PipeWrap, uv_pipe_t> {
 public:
  enum SocketType {
    SOCKET,
    SERVER,
    IPC
  };

  static MaybeLocal<Object> Instantiate(Environment* env,
                                        AsyncWrap* parent,
                                        SocketType type);
  static void Initialize(Local<Object> target,
                         Local<Value> unused,
                         Local<Context> context,
                         void* priv);

  SET_NO_MEMORY_INFO()
  SET_MEMORY_INFO_NAME(PipeWrap)
  SET_SELF_SIZE(PipeWrap)

 private:
  PipeWrap(Environment* env,
           Local<Object> object,
           ProviderType provider,
           bool ipc);

  static void New(const FunctionCallbackInfo<Value>& args);
  static void Bind(const FunctionCallbackInfo<Value>& args);
  static void Listen(const FunctionCallbackInfo<Value>& args);
  static void Connect(const FunctionCallbackInfo<Value>& args);
  static void Open(const FunctionCallbackInfo<Value>& args);

#ifdef _WIN32
  static void SetPendingInstances(const FunctionCallbackInfo<Value>& args);
#endif
  static void Fchmod(const FunctionCallbackInfo<Value>& args);
};

}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_PIPE_WRAP_H_
