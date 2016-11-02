#include "connect_wrap.h"

#include "env.h"
#include "env-inl.h"
#include "req-wrap.h"
#include "req-wrap-inl.h"
#include "util.h"
#include "util-inl.h"

namespace node {

using v8::Local;
using v8::Object;


ConnectWrap::ConnectWrap(Environment* env,
    Local<Object> req_wrap_obj,
    AsyncWrap::ProviderType provider) : ReqWrap(env, req_wrap_obj, provider) {
  Wrap(req_wrap_obj, this);
}


ConnectWrap::~ConnectWrap() {
  ClearWrap(object());
}

}  // namespace node
