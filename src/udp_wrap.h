#ifndef UDP_WRAP_H_
#define UDP_WRAP_H_

#include "node.h"
#include "req_wrap.h"
#include "handle_wrap.h"

namespace node {

class UDPWrap: public HandleWrap {
 public:
  static void Initialize(v8::Handle<v8::Object> target);
  static v8::Handle<v8::Value> GetFD(v8::Local<v8::String>,
                                     const v8::AccessorInfo&);
  static v8::Handle<v8::Value> New(const v8::Arguments& args);
  static v8::Handle<v8::Value> Bind(const v8::Arguments& args);
  static v8::Handle<v8::Value> Send(const v8::Arguments& args);
  static v8::Handle<v8::Value> Bind6(const v8::Arguments& args);
  static v8::Handle<v8::Value> Send6(const v8::Arguments& args);
  static v8::Handle<v8::Value> RecvStart(const v8::Arguments& args);
  static v8::Handle<v8::Value> RecvStop(const v8::Arguments& args);
  static v8::Handle<v8::Value> GetSockName(const v8::Arguments& args);
  static v8::Handle<v8::Value> AddMembership(const v8::Arguments& args);
  static v8::Handle<v8::Value> DropMembership(const v8::Arguments& args);
  static v8::Handle<v8::Value> SetMulticastTTL(const v8::Arguments& args);
  static v8::Handle<v8::Value> SetMulticastLoopback(const v8::Arguments& args);
  static v8::Handle<v8::Value> SetBroadcast(const v8::Arguments& args);
  static v8::Handle<v8::Value> SetTTL(const v8::Arguments& args);
  static UDPWrap* Unwrap(v8::Local<v8::Object> obj);

  static v8::Local<v8::Object> Instantiate();
  uv_udp_t* UVHandle();

 private:
  UDPWrap(v8::Handle<v8::Object> object);
  virtual ~UDPWrap();

  static v8::Handle<v8::Value> DoBind(const v8::Arguments& args, int family);
  static v8::Handle<v8::Value> DoSend(const v8::Arguments& args, int family);
  static v8::Handle<v8::Value> SetMembership(const v8::Arguments& args,
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
