#ifndef SRC_ASYNC_WRAP_H_
#define SRC_ASYNC_WRAP_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "base-object.h"
#include "uv.h"
#include "v8.h"

#include <stdint.h>

namespace node {

#define NODE_ASYNC_ID_OFFSET 0xA1C

#define NODE_ASYNC_PROVIDER_TYPES(V)                                          \
  V(NONE)                                                                     \
  V(CONNECTION)                                                               \
  V(FSEVENTWRAP)                                                              \
  V(FSREQWRAP)                                                                \
  V(GETADDRINFOREQWRAP)                                                       \
  V(GETNAMEINFOREQWRAP)                                                       \
  V(HTTPPARSER)                                                               \
  V(JSSTREAM)                                                                 \
  V(PBKDF2REQUEST)                                                            \
  V(PIPECONNECTWRAP)                                                          \
  V(PIPEWRAP)                                                                 \
  V(PROCESSWRAP)                                                              \
  V(QUERYWRAP)                                                                \
  V(RANDOMBYTESREQUEST)                                                       \
  V(SENDWRAP)                                                                 \
  V(SHUTDOWNWRAP)                                                             \
  V(SIGNALWRAP)                                                               \
  V(STATWATCHER)                                                              \
  V(TCPWRAP)                                                                  \
  V(TCPCONNECTWRAP)                                                           \
  V(TIMERWRAP)                                                                \
  V(TLSWRAP)                                                                  \
  V(TTYWRAP)                                                                  \
  V(UDPWRAP)                                                                  \
  V(WRITEWRAP)                                                                \
  V(ZCTX)

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

  AsyncWrap(Environment* env,
            v8::Local<v8::Object> object,
            ProviderType provider);

  virtual ~AsyncWrap();

  static void Initialize(v8::Local<v8::Object> target,
                         v8::Local<v8::Value> unused,
                         v8::Local<v8::Context> context);

  static void GetAsyncId(const v8::FunctionCallbackInfo<v8::Value>& args);

  static void DestroyIdsCb(uv_idle_t* handle);

  static void AsyncReset(const v8::FunctionCallbackInfo<v8::Value>& args);

  static void AddIdToDestroyList(
      const v8::FunctionCallbackInfo<v8::Value>& args);

  // Counterpart to Environment::AsyncHooks::gen_id_array().
  static void GenIdArray(const v8::FunctionCallbackInfo<v8::Value>& args);

  // Counterpart to Environment::AsyncHooks::trim_id_array().
  static void TrimIdArray(const v8::FunctionCallbackInfo<v8::Value>& args);

  // Counterpart to Environment::AsyncHooks::reset_id_array().
  static void ResetIdArray(const v8::FunctionCallbackInfo<v8::Value>& args);

  inline ProviderType provider_type() const;

  inline double get_id() const;

  inline double get_trigger_id() const;

  void Reset();

  // Only call these within a valid HandleScope.
  // TODO(trevnorris): These should return a MaybeLocal.
  v8::Local<v8::Value> MakeCallback(const v8::Local<v8::Function> cb,
                                    int argc,
                                    v8::Local<v8::Value>* argv);
  inline v8::Local<v8::Value> MakeCallback(const v8::Local<v8::String> symbol,
                                           int argc,
                                           v8::Local<v8::Value>* argv);
  inline v8::Local<v8::Value> MakeCallback(uint32_t index,
                                           int argc,
                                           v8::Local<v8::Value>* argv);

  virtual size_t self_size() const = 0;

 private:
  inline AsyncWrap();
  const ProviderType provider_type_;
  // Because the values may be Reset(), cannot be made const.
  double id_;
  double trigger_id_;
};

void LoadAsyncWrapperInfo(Environment* env);

}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_ASYNC_WRAP_H_
