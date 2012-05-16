#ifndef UDP_WRAP_H_
#define UDP_WRAP_H_

#include "node.h"
#include "req_wrap.h"
#include "handle_wrap.h"

namespace node {

using v8::Object;
using v8::Handle;
using v8::Local;
using v8::Value;
using v8::String;
using v8::Arguments;

class UDPWrap: public HandleWrap {
 public:
  static void Initialize(Handle<Object> target);
  static Handle<Value> New(const Arguments& args);
  static Handle<Value> Bind(const Arguments& args);
  static Handle<Value> Send(const Arguments& args);
  static Handle<Value> Bind6(const Arguments& args);
  static Handle<Value> Send6(const Arguments& args);
  static Handle<Value> RecvStart(const Arguments& args);
  static Handle<Value> RecvStop(const Arguments& args);
  static Handle<Value> GetSockName(const Arguments& args);
  static Handle<Value> AddMembership(const Arguments& args);
  static Handle<Value> DropMembership(const Arguments& args);
  static Handle<Value> SetMulticastTTL(const Arguments& args);
  static Handle<Value> SetMulticastLoopback(const Arguments& args);
  static Handle<Value> SetBroadcast(const Arguments& args);
  static Handle<Value> SetTTL(const Arguments& args);
  static UDPWrap* Unwrap(Local<Object> obj);

  uv_udp_t* UVHandle();

 private:
  UDPWrap(Handle<Object> object);
  virtual ~UDPWrap();

  static Handle<Value> DoBind(const Arguments& args, int family);
  static Handle<Value> DoSend(const Arguments& args, int family);
  static Handle<Value> SetMembership(const Arguments& args,
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
