#ifndef PIPE_WRAP_H_
#define PIPE_WRAP_H_
#include <stream_wrap.h>

namespace node {

class PipeWrap : StreamWrap {
 public:
  uv_pipe_t* UVHandle();

  static PipeWrap* Unwrap(v8::Local<v8::Object> obj);
  static void Initialize(v8::Handle<v8::Object> target);

 private:
  PipeWrap(v8::Handle<v8::Object> object);

  static v8::Handle<v8::Value> New(const v8::Arguments& args);
  static v8::Handle<v8::Value> Bind(const v8::Arguments& args);
  static v8::Handle<v8::Value> Listen(const v8::Arguments& args);
  static v8::Handle<v8::Value> Connect(const v8::Arguments& args);
  static v8::Handle<v8::Value> Open(const v8::Arguments& args);

  static void OnConnection(uv_stream_t* handle, int status);
  static void AfterConnect(uv_connect_t* req, int status);

  uv_pipe_t handle_;
};


}  // namespace node


#endif  // PIPE_WRAP_H_
