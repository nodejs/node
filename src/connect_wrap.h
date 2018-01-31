#ifndef SRC_CONNECT_WRAP_H_
#define SRC_CONNECT_WRAP_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "env.h"
#include "req_wrap.h"
#include "async_wrap.h"
#include "v8.h"

namespace node {

class ConnectWrap : public ReqWrap<uv_connect_t> {
 public:
  ConnectWrap(Environment* env,
              v8::Local<v8::Object> req_wrap_obj,
              AsyncWrap::ProviderType provider);
  ~ConnectWrap();

  size_t self_size() const override { return sizeof(*this); }
};

}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_CONNECT_WRAP_H_
