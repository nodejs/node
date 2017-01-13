#ifndef SRC_UDP_WRAP_H_
#define SRC_UDP_WRAP_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "async-wrap.h"
#include "env.h"
#include "handle_wrap.h"
#include "req-wrap.h"
#include "req-wrap-inl.h"
#include "uv.h"
#include "v8.h"

namespace node {

class UDPWrap: public HandleWrap {
 public:
  static void Initialize(v8::Local<v8::Object> target,
                         v8::Local<v8::Value> unused,
                         v8::Local<v8::Context> context);
  static void GetFD(v8::Local<v8::String>,
                    const v8::PropertyCallbackInfo<v8::Value>&);
  static void New(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Bind(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Send(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Bind6(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Send6(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void RecvStart(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void RecvStop(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void AddMembership(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void DropMembership(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void SetMulticastTTL(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void SetMulticastLoopback(
      const v8::FunctionCallbackInfo<v8::Value>& args);
  static void SetBroadcast(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void SetTTL(const v8::FunctionCallbackInfo<v8::Value>& args);

  static v8::Local<v8::Object> Instantiate(Environment* env, AsyncWrap* parent);
  uv_udp_t* UVHandle();

  size_t self_size() const override { return sizeof(*this); }

 private:
  typedef uv_udp_t HandleType;

  template <typename T,
            int (*F)(const typename T::HandleType*, sockaddr*, int*)>
  friend void GetSockOrPeerName(const v8::FunctionCallbackInfo<v8::Value>&);

  UDPWrap(Environment* env, v8::Local<v8::Object> object, AsyncWrap* parent);

  static void DoBind(const v8::FunctionCallbackInfo<v8::Value>& args,
                     int family);
  static void DoSend(const v8::FunctionCallbackInfo<v8::Value>& args,
                     int family);
  static void SetMembership(const v8::FunctionCallbackInfo<v8::Value>& args,
                            uv_membership membership);

  static void OnAlloc(uv_handle_t* handle,
                      size_t suggested_size,
                      uv_buf_t* buf);
  static void OnSend(uv_udp_send_t* req, int status);
  static void OnRecv(uv_udp_t* handle,
                     ssize_t nread,
                     const uv_buf_t* buf,
                     const struct sockaddr* addr,
                     unsigned int flags);

  uv_udp_t handle_;
};

}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_UDP_WRAP_H_
