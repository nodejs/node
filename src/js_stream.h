#ifndef SRC_JS_STREAM_H_
#define SRC_JS_STREAM_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "async-wrap.h"
#include "env.h"
#include "stream_base.h"
#include "v8.h"

namespace node {

class JSStream : public AsyncWrap, public StreamBase {
 public:
  static void Initialize(v8::Local<v8::Object> target,
                         v8::Local<v8::Value> unused,
                         v8::Local<v8::Context> context);

  ~JSStream();

  void* Cast() override;
  bool IsAlive() override;
  bool IsClosing() override;
  int ReadStart() override;
  int ReadStop() override;

  int DoShutdown(ShutdownWrap* req_wrap) override;
  int DoWrite(WriteWrap* w,
              uv_buf_t* bufs,
              size_t count,
              uv_stream_t* send_handle) override;

  size_t self_size() const override { return sizeof(*this); }

 protected:
  JSStream(Environment* env, v8::Local<v8::Object> obj, AsyncWrap* parent);

  AsyncWrap* GetAsyncWrap() override;

  static void New(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void DoAlloc(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void DoRead(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void DoAfterWrite(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void ReadBuffer(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void EmitEOF(const v8::FunctionCallbackInfo<v8::Value>& args);

  template <class Wrap>
  static void Finish(const v8::FunctionCallbackInfo<v8::Value>& args);
};

}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_JS_STREAM_H_
