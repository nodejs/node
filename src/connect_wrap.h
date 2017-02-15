#ifndef SRC_CONNECT_WRAP_H_
#define SRC_CONNECT_WRAP_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "env.h"
#include "req-wrap.h"
#include "async-wrap.h"
#include "v8.h"

namespace node {

class ConnectWrap : public ReqWrap<uv_connect_t> {
 public:
  ConnectWrap(Environment* env,
              v8::Local<v8::Object> req_wrap_obj,
              AsyncWrap::ProviderType provider);

  size_t self_size() const override { return sizeof(*this); }
};

}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_CONNECT_WRAP_H_
