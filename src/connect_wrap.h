#ifndef SRC_CONNECT_WRAP_H_
#define SRC_CONNECT_WRAP_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "req_wrap-inl.h"
#include "async_wrap.h"

namespace node {

class ConnectWrap : public ReqWrap<uv_connect_t> {
 public:
  ConnectWrap(Environment* env,
              v8::Local<v8::Object> req_wrap_obj,
              AsyncWrap::ProviderType provider);

  SET_NO_MEMORY_INFO()
  SET_MEMORY_INFO_NAME(ConnectWrap)
  SET_SELF_SIZE(ConnectWrap)
};

}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_CONNECT_WRAP_H_
