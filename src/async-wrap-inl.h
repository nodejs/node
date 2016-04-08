#ifndef SRC_ASYNC_WRAP_INL_H_
#define SRC_ASYNC_WRAP_INL_H_

#include "async-wrap.h"
#include "base-object.h"
#include "base-object-inl.h"
#include "env.h"
#include "env-inl.h"
#include "node_internals.h"
#include "util.h"
#include "util-inl.h"
#include "v8.h"

namespace node {

inline AsyncWrap::AsyncWrap(Environment* env,
                            v8::Local<v8::Object> object,
                            ProviderType provider,
                            AsyncWrap* parent)
    : BaseObject(env, object), bits_(static_cast<uint32_t>(provider) << 1),
      uid_(env->async_hooks()->get_next_async_wrap_uid()) {
  CHECK_NE(provider, PROVIDER_NONE);
  CHECK_GE(object->InternalFieldCount(), 1);

  // Shift provider value over to prevent id collision.
  persistent().SetWrapperClassId(NODE_ASYNC_ID_OFFSET + provider);

  if (AsyncWrap::FireAsyncInitCallbacks(env, get_uid(), object, provider, parent)) {
      bits_ |= 1;  // ran_init_callback() is true now.
  }
}


inline AsyncWrap::~AsyncWrap() {
  v8::HandleScope scope(env()->isolate());
  FireAsyncDestroyCallbacks(env(), ran_init_callback(), v8::Integer::New(env()->isolate(), get_uid()));
}


inline bool AsyncWrap::ran_init_callback() const {
  return static_cast<bool>(bits_ & 1);
}


inline AsyncWrap::ProviderType AsyncWrap::provider_type() const {
  return static_cast<ProviderType>(bits_ >> 1);
}


inline int64_t AsyncWrap::get_uid() const {
  return uid_;
}


inline v8::Local<v8::Value> AsyncWrap::MakeCallback(
    const v8::Local<v8::String> symbol,
    int argc,
    v8::Local<v8::Value>* argv) {
  v8::Local<v8::Value> cb_v = object()->Get(symbol);
  CHECK(cb_v->IsFunction());
  return MakeCallback(cb_v.As<v8::Function>(), argc, argv);
}


inline v8::Local<v8::Value> AsyncWrap::MakeCallback(
    uint32_t index,
    int argc,
    v8::Local<v8::Value>* argv) {
  v8::Local<v8::Value> cb_v = object()->Get(index);
  CHECK(cb_v->IsFunction());
  return MakeCallback(cb_v.As<v8::Function>(), argc, argv);
}

}  // namespace node

#endif  // SRC_ASYNC_WRAP_INL_H_
