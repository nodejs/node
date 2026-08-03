#ifndef SRC_CONNECTION_WRAP_H_
#define SRC_CONNECTION_WRAP_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "stream_wrap.h"

namespace node {

class Environment;

template <typename WrapType, typename UVType>
class ConnectionWrap : public LibuvStreamWrap {
 public:
  static void OnConnection(uv_stream_t* handle, int status);
  static void AfterConnect(uv_connect_t* req, int status);

 protected:
  ConnectionWrap(Environment* env,
                 v8::Local<v8::Object> object,
                 ProviderType provider);

  UVType handle_;
};

}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_CONNECTION_WRAP_H_
