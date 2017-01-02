#ifndef SRC_TCP_WRAP_H_
#define SRC_TCP_WRAP_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "async-wrap.h"
#include "env.h"
#include "connection_wrap.h"

namespace node {

class TCPWrap : public ConnectionWrap<TCPWrap, uv_tcp_t> {
 public:
  static v8::Local<v8::Object> Instantiate(Environment* env, AsyncWrap* parent);
  static void Initialize(v8::Local<v8::Object> target,
                         v8::Local<v8::Value> unused,
                         v8::Local<v8::Context> context);

  size_t self_size() const override { return sizeof(*this); }

 private:
  typedef uv_tcp_t HandleType;

  template <typename T,
            int (*F)(const typename T::HandleType*, sockaddr*, int*)>
  friend void GetSockOrPeerName(const v8::FunctionCallbackInfo<v8::Value>&);

  TCPWrap(Environment* env, v8::Local<v8::Object> object, AsyncWrap* parent);
  ~TCPWrap();

  static void New(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void SetNoDelay(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void SetKeepAlive(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Bind(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Bind6(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Listen(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Connect(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Connect6(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Open(const v8::FunctionCallbackInfo<v8::Value>& args);

#ifdef _WIN32
  static void SetSimultaneousAccepts(
      const v8::FunctionCallbackInfo<v8::Value>& args);
#endif
};


}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_TCP_WRAP_H_
