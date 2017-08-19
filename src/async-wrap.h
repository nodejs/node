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

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "base-object.h"
#include "v8.h"
#include "node.h"

#include <stdint.h>

namespace node {

#define NODE_ASYNC_ID_OFFSET 0xA1C

#define NODE_ASYNC_NON_CRYPTO_PROVIDER_TYPES(V)                               \
  V(NONE)                                                                     \
  V(DNSCHANNEL)                                                               \
  V(FSEVENTWRAP)                                                              \
  V(FSREQWRAP)                                                                \
  V(GETADDRINFOREQWRAP)                                                       \
  V(GETNAMEINFOREQWRAP)                                                       \
  V(HTTP2SESSION)                                                             \
  V(HTTP2SESSIONSHUTDOWNWRAP)                                                 \
  V(HTTPPARSER)                                                               \
  V(JSSTREAM)                                                                 \
  V(PIPECONNECTWRAP)                                                          \
  V(PIPEWRAP)                                                                 \
  V(PROCESSWRAP)                                                              \
  V(PROMISE)                                                                  \
  V(QUERYWRAP)                                                                \
  V(SHUTDOWNWRAP)                                                             \
  V(SIGNALWRAP)                                                               \
  V(STATWATCHER)                                                              \
  V(TCPCONNECTWRAP)                                                           \
  V(TCPWRAP)                                                                  \
  V(TIMERWRAP)                                                                \
  V(TTYWRAP)                                                                  \
  V(UDPSENDWRAP)                                                              \
  V(UDPWRAP)                                                                  \
  V(WRITEWRAP)                                                                \
  V(ZLIB)

#if HAVE_OPENSSL
#define NODE_ASYNC_CRYPTO_PROVIDER_TYPES(V)                                   \
  V(SSLCONNECTION)                                                            \
  V(PBKDF2REQUEST)                                                            \
  V(RANDOMBYTESREQUEST)                                                       \
  V(TLSWRAP)
#else
#define NODE_ASYNC_CRYPTO_PROVIDER_TYPES(V)
#endif  // HAVE_OPENSSL

#define NODE_ASYNC_PROVIDER_TYPES(V)                                          \
  NODE_ASYNC_NON_CRYPTO_PROVIDER_TYPES(V)                                     \
  NODE_ASYNC_CRYPTO_PROVIDER_TYPES(V)

class Environment;

class AsyncWrap : public BaseObject {
 public:
  enum ProviderType {
#define V(PROVIDER)                                                           \
    PROVIDER_ ## PROVIDER,
    NODE_ASYNC_PROVIDER_TYPES(V)
#undef V
    PROVIDERS_LENGTH,
  };

  enum Flags {
    kFlagNone = 0x0,
    kFlagHasReset = 0x1
  };

  AsyncWrap(Environment* env,
            v8::Local<v8::Object> object,
            ProviderType provider,
            bool silent = false);

  virtual ~AsyncWrap();

  static void AddWrapMethods(Environment* env,
                             v8::Local<v8::FunctionTemplate> constructor,
                             int flags = kFlagNone);

  static void Initialize(v8::Local<v8::Object> target,
                         v8::Local<v8::Value> unused,
                         v8::Local<v8::Context> context);

  static void GetAsyncId(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void PushAsyncIds(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void PopAsyncIds(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void AsyncIdStackSize(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void ClearIdStack(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void AsyncReset(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void QueueDestroyId(const v8::FunctionCallbackInfo<v8::Value>& args);

  static void EmitAsyncInit(Environment* env,
                            v8::Local<v8::Object> object,
                            v8::Local<v8::String> type,
                            double id,
                            double trigger_id);

  static void EmitBefore(Environment* env, double id);
  static void EmitAfter(Environment* env, double id);

  inline ProviderType provider_type() const;

  inline double get_id() const;

  inline double get_trigger_id() const;

  void AsyncReset(bool silent = false);

  // Only call these within a valid HandleScope.
  v8::MaybeLocal<v8::Value> MakeCallback(const v8::Local<v8::Function> cb,
                                         int argc,
                                         v8::Local<v8::Value>* argv);
  inline v8::MaybeLocal<v8::Value> MakeCallback(
      const v8::Local<v8::String> symbol,
      int argc,
      v8::Local<v8::Value>* argv);
  inline v8::MaybeLocal<v8::Value> MakeCallback(uint32_t index,
                                                int argc,
                                                v8::Local<v8::Value>* argv);

  virtual size_t self_size() const = 0;

 private:
  inline AsyncWrap();
  const ProviderType provider_type_;
  // Because the values may be Reset(), cannot be made const.
  double async_id_;
  double trigger_id_;
};

void LoadAsyncWrapperInfo(Environment* env);

// Return value is an indicator whether the domain was disposed.
bool DomainEnter(Environment* env, v8::Local<v8::Object> object);
bool DomainExit(Environment* env, v8::Local<v8::Object> object);

}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_ASYNC_WRAP_H_
