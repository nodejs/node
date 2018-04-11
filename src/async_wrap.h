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

#include "base_object.h"
#include "v8.h"

#include <stdint.h>

namespace node {

#define NODE_ASYNC_ID_OFFSET 0xA1C

#define NODE_ASYNC_NON_CRYPTO_PROVIDER_TYPES(V)                               \
  V(NONE)                                                                     \
  V(DNSCHANNEL)                                                               \
  V(FILEHANDLE)                                                               \
  V(FILEHANDLECLOSEREQ)                                                       \
  V(FSEVENTWRAP)                                                              \
  V(FSREQWRAP)                                                                \
  V(FSREQPROMISE)                                                             \
  V(GETADDRINFOREQWRAP)                                                       \
  V(GETNAMEINFOREQWRAP)                                                       \
  V(HTTP2SESSION)                                                             \
  V(HTTP2STREAM)                                                              \
  V(HTTP2PING)                                                                \
  V(HTTP2SETTINGS)                                                            \
  V(HTTPPARSER)                                                               \
  V(JSSTREAM)                                                                 \
  V(PIPECONNECTWRAP)                                                          \
  V(PIPESERVERWRAP)                                                           \
  V(PIPEWRAP)                                                                 \
  V(PROCESSWRAP)                                                              \
  V(PROMISE)                                                                  \
  V(QUERYWRAP)                                                                \
  V(SHUTDOWNWRAP)                                                             \
  V(SIGNALWRAP)                                                               \
  V(STATWATCHER)                                                              \
  V(STREAMPIPE)                                                               \
  V(TCPCONNECTWRAP)                                                           \
  V(TCPSERVERWRAP)                                                            \
  V(TCPWRAP)                                                                  \
  V(TIMERWRAP)                                                                \
  V(TTYWRAP)                                                                  \
  V(UDPSENDWRAP)                                                              \
  V(UDPWRAP)                                                                  \
  V(WRITEWRAP)                                                                \
  V(ZLIB)

#if HAVE_OPENSSL
#define NODE_ASYNC_CRYPTO_PROVIDER_TYPES(V)                                   \
  V(PBKDF2REQUEST)                                                            \
  V(RANDOMBYTESREQUEST)                                                       \
  V(TLSWRAP)
#else
#define NODE_ASYNC_CRYPTO_PROVIDER_TYPES(V)
#endif  // HAVE_OPENSSL

#if HAVE_INSPECTOR
#define NODE_ASYNC_INSPECTOR_PROVIDER_TYPES(V)                                \
  V(INSPECTORJSBINDING)
#else
#define NODE_ASYNC_INSPECTOR_PROVIDER_TYPES(V)
#endif  // HAVE_INSPECTOR

#define NODE_ASYNC_PROVIDER_TYPES(V)                                          \
  NODE_ASYNC_NON_CRYPTO_PROVIDER_TYPES(V)                                     \
  NODE_ASYNC_CRYPTO_PROVIDER_TYPES(V)                                         \
  NODE_ASYNC_INSPECTOR_PROVIDER_TYPES(V)

class Environment;
class DestroyParam;

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
            double execution_async_id = -1);

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
  static void AsyncReset(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void QueueDestroyAsyncId(
    const v8::FunctionCallbackInfo<v8::Value>& args);

  static void EmitAsyncInit(Environment* env,
                            v8::Local<v8::Object> object,
                            v8::Local<v8::String> type,
                            double async_id,
                            double trigger_async_id);

  static void EmitDestroy(Environment* env, double async_id);
  static void EmitBefore(Environment* env, double async_id);
  static void EmitAfter(Environment* env, double async_id);
  static void EmitPromiseResolve(Environment* env, double async_id);

  void EmitTraceEventBefore();
  static void EmitTraceEventAfter(ProviderType type, double async_id);
  void EmitTraceEventDestroy();


  inline ProviderType provider_type() const;

  inline double get_async_id() const;

  inline double get_trigger_async_id() const;

  void AsyncReset(double execution_async_id = -1, bool silent = false);

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

  static void WeakCallback(const v8::WeakCallbackInfo<DestroyParam> &info);

  // This is a simplified version of InternalCallbackScope that only runs
  // the `before` and `after` hooks. Only use it when not actually calling
  // back into JS; otherwise, use InternalCallbackScope.
  class AsyncScope {
   public:
    explicit inline AsyncScope(AsyncWrap* wrap);
    ~AsyncScope();

   private:
    AsyncWrap* wrap_ = nullptr;
  };

 private:
  friend class PromiseWrap;

  AsyncWrap(Environment* env,
            v8::Local<v8::Object> promise,
            ProviderType provider,
            double execution_async_id,
            bool silent);
  inline AsyncWrap();
  const ProviderType provider_type_;
  // Because the values may be Reset(), cannot be made const.
  double async_id_;
  double trigger_async_id_;
};

void LoadAsyncWrapperInfo(Environment* env);

}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_ASYNC_WRAP_H_
