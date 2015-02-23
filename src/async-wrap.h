#ifndef SRC_ASYNC_WRAP_H_
#define SRC_ASYNC_WRAP_H_

#include "base-object.h"
#include "v8.h"

#include <stdint.h>

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
  V(JSSTREAM)                                                                 \
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

class Environment;

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
                   AsyncWrap* parent = nullptr);

  inline virtual ~AsyncWrap() override = default;

  inline ProviderType provider_type() const;

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
  inline bool has_async_queue() const;

  // When the async hooks init JS function is called from the constructor it is
  // expected the context object will receive a _asyncQueue object property
  // that will be used to call pre/post in MakeCallback.
  uint32_t bits_;
};

}  // namespace node


#endif  // SRC_ASYNC_WRAP_H_
