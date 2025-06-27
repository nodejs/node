#ifndef SRC_JS_STREAM_H_
#define SRC_JS_STREAM_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "async_wrap.h"
#include "stream_base.h"

namespace node {

class Environment;

class JSStream : public AsyncWrap, public StreamBase {
 public:
  static void Initialize(v8::Local<v8::Object> target,
                         v8::Local<v8::Value> unused,
                         v8::Local<v8::Context> context,
                         void* priv);

  bool IsAlive() override;
  bool IsClosing() override;
  int ReadStart() override;
  int ReadStop() override;

  int DoShutdown(ShutdownWrap* req_wrap) override;
  int DoWrite(WriteWrap* w,
              uv_buf_t* bufs,
              size_t count,
              uv_stream_t* send_handle) override;

  SET_NO_MEMORY_INFO()
  SET_MEMORY_INFO_NAME(JSStream)
  SET_SELF_SIZE(JSStream)

 protected:
  JSStream(Environment* env, v8::Local<v8::Object> obj);

  AsyncWrap* GetAsyncWrap() override;

  static void New(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void ReadBuffer(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void EmitEOF(const v8::FunctionCallbackInfo<v8::Value>& args);

  template <class Wrap>
  static void Finish(const v8::FunctionCallbackInfo<v8::Value>& args);
};

}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_JS_STREAM_H_
