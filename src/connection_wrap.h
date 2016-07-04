#ifndef SRC_CONNECTION_WRAP_H_
#define SRC_CONNECTION_WRAP_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "env.h"
#include "v8.h"

namespace node {

class ConnectionWrap {
 protected:
  template<typename WrapType, typename UVType>
  static void OnConnection(uv_stream_t* handle, int status);
};


}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_CONNECTION_WRAP_H_
