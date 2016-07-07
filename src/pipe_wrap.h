#ifndef SRC_PIPE_WRAP_H_
#define SRC_PIPE_WRAP_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "async-wrap.h"
#include "env.h"
#include "stream_wrap.h"
#include "connection_wrap.h"

namespace node {

class PipeWrap : public StreamWrap, ConnectionWrap<PipeWrap, uv_pipe_t> {
 public:
  uv_pipe_t* UVHandle();

  static v8::Local<v8::Object> Instantiate(Environment* env, AsyncWrap* parent);
  static void Initialize(v8::Local<v8::Object> target,
                         v8::Local<v8::Value> unused,
                         v8::Local<v8::Context> context);

  size_t self_size() const override { return sizeof(*this); }

 private:
  friend class ConnectionWrap;  // So handle_ can be accessed
  PipeWrap(Environment* env,
           v8::Local<v8::Object> object,
           bool ipc,
           AsyncWrap* parent);

  static void New(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Bind(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Listen(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Connect(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Open(const v8::FunctionCallbackInfo<v8::Value>& args);

#ifdef _WIN32
  static void SetPendingInstances(
      const v8::FunctionCallbackInfo<v8::Value>& args);
#endif

  static void AfterConnect(uv_connect_t* req, int status);

  uv_pipe_t handle_;
};


}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_PIPE_WRAP_H_
