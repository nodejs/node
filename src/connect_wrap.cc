#include "connect_wrap.h"

#include "env-inl.h"
#include "req_wrap-inl.h"
#include "util-inl.h"

namespace node {

using v8::Local;
using v8::Object;


ConnectWrap::ConnectWrap(Environment* env,
    Local<Object> req_wrap_obj,
    AsyncWrap::ProviderType provider) : ReqWrap(env, req_wrap_obj, provider) {
}

}  // namespace node
