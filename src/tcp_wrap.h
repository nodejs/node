#ifndef TCP_WRAP_H_
#define TCP_WRAP_H_
#include <stream_wrap.h>

namespace node {

class TCPWrap : public StreamWrap {
 public:
  static v8::Local<v8::Object> Instantiate();
  static TCPWrap* Unwrap(v8::Local<v8::Object> obj);
  static void Initialize(v8::Handle<v8::Object> target);

 private:
  TCPWrap(v8::Handle<v8::Object> object);
  ~TCPWrap();

  static v8::Handle<v8::Value> New(const v8::Arguments& args);
  static v8::Handle<v8::Value> GetSockName(const v8::Arguments& args);
  static v8::Handle<v8::Value> GetPeerName(const v8::Arguments& args);
  static v8::Handle<v8::Value> Bind(const v8::Arguments& args);
  static v8::Handle<v8::Value> Bind6(const v8::Arguments& args);
  static v8::Handle<v8::Value> Listen(const v8::Arguments& args);
  static v8::Handle<v8::Value> Connect(const v8::Arguments& args);
  static v8::Handle<v8::Value> Connect6(const v8::Arguments& args);
  static v8::Handle<v8::Value> Open(const v8::Arguments& args);

  static void OnConnection(uv_stream_t* handle, int status);
  static void AfterConnect(uv_connect_t* req, int status);

  uv_tcp_t handle_;
};


}  // namespace node


#endif  // TCP_WRAP_H_
