#ifndef UDP_WRAP_H_
#define UDP_WRAP_H_

#include "node.h"
#include "req_wrap.h"
#include "handle_wrap.h"

namespace node {

class UDPWrap: public HandleWrap {
 public:
  static void Initialize(v8::Handle<v8::Object> target);
  static void GetFD(v8::Local<v8::String>,
                    const v8::PropertyCallbackInfo<v8::Value>&);
  static void New(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Bind(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Send(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Bind6(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Send6(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void RecvStart(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void RecvStop(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void GetSockName(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void AddMembership(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void DropMembership(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void SetMulticastTTL(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void SetMulticastLoopback(
      const v8::FunctionCallbackInfo<v8::Value>& args);
  static void SetBroadcast(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void SetTTL(const v8::FunctionCallbackInfo<v8::Value>& args);
  static UDPWrap* Unwrap(v8::Local<v8::Object> obj);

  static v8::Local<v8::Object> Instantiate();
  uv_udp_t* UVHandle();

 private:
  UDPWrap(v8::Handle<v8::Object> object);
  virtual ~UDPWrap();

  static void DoBind(const v8::FunctionCallbackInfo<v8::Value>& args,
                     int family);
  static void DoSend(const v8::FunctionCallbackInfo<v8::Value>& args,
                     int family);
  static void SetMembership(const v8::FunctionCallbackInfo<v8::Value>& args,
                            uv_membership membership);

  static uv_buf_t OnAlloc(uv_handle_t* handle, size_t suggested_size);
  static void OnSend(uv_udp_send_t* req, int status);
  static void OnRecv(uv_udp_t* handle,
                     ssize_t nread,
                     uv_buf_t buf,
                     struct sockaddr* addr,
                     unsigned flags);

  uv_udp_t handle_;
};

} // namespace node

#endif // UDP_WRAP_H_
