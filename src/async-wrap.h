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

#ifndef SRC_ASYNC_WRAP_H_
#define SRC_ASYNC_WRAP_H_

#include "base-object.h"
#include "env.h"
#include "v8.h"

namespace node {

class AsyncWrap : public BaseObject {
 public:
  enum ProviderType {
    PROVIDER_NONE               = 1 << 0,
    PROVIDER_CARES              = 1 << 1,
    PROVIDER_CONNECTWRAP        = 1 << 2,
    PROVIDER_CRYPTO             = 1 << 3,
    PROVIDER_FSEVENTWRAP        = 1 << 4,
    PROVIDER_GETADDRINFOREQWRAP = 1 << 5,
    PROVIDER_PIPEWRAP           = 1 << 6,
    PROVIDER_PROCESSWRAP        = 1 << 7,
    PROVIDER_REQWRAP            = 1 << 8,
    PROVIDER_SHUTDOWNWRAP       = 1 << 9,
    PROVIDER_SIGNALWRAP         = 1 << 10,
    PROVIDER_STATWATCHER        = 1 << 11,
    PROVIDER_TCPWRAP            = 1 << 12,
    PROVIDER_TIMERWRAP          = 1 << 13,
    PROVIDER_TLSWRAP            = 1 << 14,
    PROVIDER_TTYWRAP            = 1 << 15,
    PROVIDER_UDPWRAP            = 1 << 16,
    PROVIDER_ZLIB               = 1 << 17,
    PROVIDER_GETNAMEINFOREQWRAP = 1 << 18
  };

  inline AsyncWrap(Environment* env,
                   v8::Handle<v8::Object> object,
                   ProviderType provider);

  inline virtual ~AsyncWrap() override = default;

  inline uint32_t provider_type() const;

  // Only call these within a valid HandleScope.
  v8::Handle<v8::Value> MakeCallback(const v8::Handle<v8::Function> cb,
                                     int argc,
                                     v8::Handle<v8::Value>* argv);
  inline v8::Handle<v8::Value> MakeCallback(const v8::Handle<v8::String> symbol,
                                            int argc,
                                            v8::Handle<v8::Value>* argv);
  inline v8::Handle<v8::Value> MakeCallback(uint32_t index,
                                            int argc,
                                            v8::Handle<v8::Value>* argv);

 private:
  inline AsyncWrap();

  uint32_t provider_type_;
};

}  // namespace node


#endif  // SRC_ASYNC_WRAP_H_
