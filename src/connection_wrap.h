#ifndef SRC_CONNECTION_WRAP_H_
#define SRC_CONNECTION_WRAP_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "env.h"
#include "stream_wrap.h"
#include "v8.h"

namespace node {

template <typename WrapType, typename UVType>
class ConnectionWrap : public StreamWrap {
 public:
  UVType* UVHandle() {
    return &handle_;
  }

  static void OnConnection(uv_stream_t* handle, int status);

 protected:
  ConnectionWrap(Environment* env,
                 v8::Local<v8::Object> object,
                 ProviderType provider,
                 AsyncWrap* parent);
  ~ConnectionWrap() {
  }

  UVType handle_;
};


}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_CONNECTION_WRAP_H_
