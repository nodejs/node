#ifndef SRC_PIPE_WRAP_H_
#define SRC_PIPE_WRAP_H_

#include "async-wrap.h"
#include "env.h"
#include "stream_wrap.h"

namespace node {

class PipeWrap : public StreamWrap {
 public:
  uv_pipe_t* UVHandle();

  static v8::Local<v8::Object> Instantiate(Environment* env, AsyncWrap* parent);
  static void Initialize(v8::Handle<v8::Object> target,
                         v8::Handle<v8::Value> unused,
                         v8::Handle<v8::Context> context);

 private:
  PipeWrap(Environment* env,
           v8::Handle<v8::Object> object,
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

  static void OnConnection(uv_stream_t* handle, int status);
  static void AfterConnect(uv_connect_t* req, int status);

  uv_pipe_t handle_;
};


}  // namespace node


#endif  // SRC_PIPE_WRAP_H_
