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
                            v8::Handle<v8::Object> object,
                            ProviderType provider,
                            AsyncWrap* parent)
    : BaseObject(env, object), bits_(static_cast<uint32_t>(provider) << 1) {
  // Check user controlled flag to see if the init callback should run.
  if (!env->using_asyncwrap())
    return;
  if (!env->call_async_init_hook() && parent == nullptr)
    return;

  // TODO(trevnorris): Until it's verified all passed object's are not weak,
  // add a HandleScope to make sure there's no leak.
  v8::HandleScope scope(env->isolate());

  v8::Local<v8::Object> parent_obj;

  v8::TryCatch try_catch;

  // If a parent value was sent then call its pre/post functions to let it know
  // a conceptual "child" is being instantiated (e.g. that a server has
  // received a connection).
  if (parent != nullptr) {
    parent_obj = parent->object();
    env->async_hooks_pre_function()->Call(parent_obj, 0, nullptr);
    if (try_catch.HasCaught())
      FatalError("node::AsyncWrap::AsyncWrap", "parent pre hook threw");
  }

  env->async_hooks_init_function()->Call(object, 0, nullptr);

  if (try_catch.HasCaught())
    FatalError("node::AsyncWrap::AsyncWrap", "init hook threw");

  bits_ |= 1;  // has_async_queue() is true now.

  if (parent != nullptr) {
    env->async_hooks_post_function()->Call(parent_obj, 0, nullptr);
    if (try_catch.HasCaught())
      FatalError("node::AsyncWrap::AsyncWrap", "parent post hook threw");
  }
}


inline bool AsyncWrap::has_async_queue() const {
  return static_cast<bool>(bits_ & 1);
}


inline AsyncWrap::ProviderType AsyncWrap::provider_type() const {
  return static_cast<ProviderType>(bits_ >> 1);
}


inline v8::Handle<v8::Value> AsyncWrap::MakeCallback(
    const v8::Handle<v8::String> symbol,
    int argc,
    v8::Handle<v8::Value>* argv) {
  v8::Local<v8::Value> cb_v = object()->Get(symbol);
  CHECK(cb_v->IsFunction());
  return MakeCallback(cb_v.As<v8::Function>(), argc, argv);
}


inline v8::Handle<v8::Value> AsyncWrap::MakeCallback(
    uint32_t index,
    int argc,
    v8::Handle<v8::Value>* argv) {
  v8::Local<v8::Value> cb_v = object()->Get(index);
  CHECK(cb_v->IsFunction());
  return MakeCallback(cb_v.As<v8::Function>(), argc, argv);
}

}  // namespace node

#endif  // SRC_ASYNC_WRAP_INL_H_
