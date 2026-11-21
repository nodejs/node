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

namespace node {

class ExternalReferenceRegistry;
class Environment;

class PipeWrap : public ConnectionWrap<PipeWrap, uv_pipe_t> {
 public:
  enum SocketType {
    SOCKET,
    SERVER,
    IPC
  };

  enum InternalFields {
    kPeerCloseCallbackField = LibuvStreamWrap::kInternalFieldCount,
    kInternalFieldCount
  };

  static v8::MaybeLocal<v8::Object> Instantiate(Environment* env,
                                                AsyncWrap* parent,
                                                SocketType type);
  static void Initialize(v8::Local<v8::Object> target,
                         v8::Local<v8::Value> unused,
                         v8::Local<v8::Context> context,
                         void* priv);

  static void RegisterExternalReferences(ExternalReferenceRegistry* registry);
  SET_NO_MEMORY_INFO()
  SET_MEMORY_INFO_NAME(PipeWrap)
  SET_SELF_SIZE(PipeWrap)

 private:
  PipeWrap(Environment* env,
           v8::Local<v8::Object> object,
           ProviderType provider,
           bool ipc);

  static void New(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Bind(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Listen(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Connect(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Open(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void WatchPeerClose(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void PeerCloseAlloc(uv_handle_t* handle,
                             size_t suggested_size,
                             uv_buf_t* buf);
  static void PeerCloseRead(uv_stream_t* stream,
                            ssize_t nread,
                            const uv_buf_t* buf);

#ifdef _WIN32
  static void SetPendingInstances(
      const v8::FunctionCallbackInfo<v8::Value>& args);
#endif
  static void Fchmod(const v8::FunctionCallbackInfo<v8::Value>& args);
};


}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_PIPE_WRAP_H_
