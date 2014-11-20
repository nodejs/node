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

#define NODE_ASYNC_PROVIDER_TYPES(V)                                          \
  V(NONE)                                                                     \
  V(CARES)                                                                    \
  V(CONNECTWRAP)                                                              \
  V(CRYPTO)                                                                   \
  V(FSEVENTWRAP)                                                              \
  V(FSREQWRAP)                                                                \
  V(GETADDRINFOREQWRAP)                                                       \
  V(GETNAMEINFOREQWRAP)                                                       \
  V(PIPEWRAP)                                                                 \
  V(PROCESSWRAP)                                                              \
  V(QUERYWRAP)                                                                \
  V(REQWRAP)                                                                  \
  V(SHUTDOWNWRAP)                                                             \
  V(SIGNALWRAP)                                                               \
  V(STATWATCHER)                                                              \
  V(TCPWRAP)                                                                  \
  V(TIMERWRAP)                                                                \
  V(TLSWRAP)                                                                  \
  V(TTYWRAP)                                                                  \
  V(UDPWRAP)                                                                  \
  V(WRITEWRAP)                                                                \
  V(ZLIB)

class AsyncWrap : public BaseObject {
 public:
  enum ProviderType {
#define V(PROVIDER)                                                           \
    PROVIDER_ ## PROVIDER,
    NODE_ASYNC_PROVIDER_TYPES(V)
#undef V
  };

  inline AsyncWrap(Environment* env,
                   v8::Handle<v8::Object> object,
                   ProviderType provider,
                   AsyncWrap* parent = NULL);

  inline ~AsyncWrap();

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

  // When the async hooks init JS function is called from the constructor it is
  // expected the context object will receive a _asyncQueue object property
  // that will be used to call pre/post in MakeCallback.
  bool has_async_queue_;
  ProviderType provider_type_;
};

}  // namespace node


#endif  // SRC_ASYNC_WRAP_H_
