#ifndef SRC_CONNECTION_WRAP_H_
#define SRC_CONNECTION_WRAP_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "env.h"
#include "stream_wrap.h"
#include "v8.h"

namespace node {

template <typename WrapType, typename UVType>
class ConnectionWrap : public LibuvStreamWrap {
 public:
  UVType* UVHandle() {
    return &handle_;
  }

  static void OnConnection(uv_stream_t* handle, int status);
  static void AfterConnect(uv_connect_t* req, int status);

 protected:
  ConnectionWrap(Environment* env,
                 v8::Local<v8::Object> object,
                 ProviderType provider);
  ~ConnectionWrap() {
  }

  UVType handle_;
};


}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_CONNECTION_WRAP_H_
