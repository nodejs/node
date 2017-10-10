#ifndef SRC_REQ_WRAP_INL_H_
#define SRC_REQ_WRAP_INL_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "req_wrap.h"
#include "async_wrap-inl.h"
#include "env-inl.h"
#include "util-inl.h"

namespace node {

template <typename T>
ReqWrap<T>::ReqWrap(Environment* env,
                    v8::Local<v8::Object> object,
                    AsyncWrap::ProviderType provider)
    : AsyncWrap(env, object, provider) {

  // FIXME(bnoordhuis) The fact that a reinterpret_cast is needed is
  // arguably a good indicator that there should be more than one queue.
  env->req_wrap_queue()->PushBack(reinterpret_cast<ReqWrap<uv_req_t>*>(this));
}

template <typename T>
ReqWrap<T>::~ReqWrap() {
  CHECK_EQ(req_.data, this);  // Assert that someone has called Dispatched().
  CHECK_EQ(false, persistent().IsEmpty());
  persistent().Reset();
}

template <typename T>
void ReqWrap<T>::Dispatched() {
  req_.data = this;
}

}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_REQ_WRAP_INL_H_
