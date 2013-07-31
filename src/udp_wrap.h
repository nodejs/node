// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

#ifndef SRC_UDP_WRAP_H_
#define SRC_UDP_WRAP_H_

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
  explicit UDPWrap(v8::Handle<v8::Object> object);
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

}  // namespace node

#endif  // SRC_UDP_WRAP_H_
