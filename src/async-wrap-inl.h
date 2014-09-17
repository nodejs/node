// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

#ifndef SRC_ASYNC_WRAP_INL_H_
#define SRC_ASYNC_WRAP_INL_H_

#include "async-wrap.h"
#include "base-object.h"
#include "base-object-inl.h"
#include "env.h"
#include "env-inl.h"
#include "util.h"
#include "util-inl.h"

#include "v8.h"
#include <assert.h>

namespace node {

inline AsyncWrap::AsyncWrap(Environment* env,
                            v8::Handle<v8::Object> object,
                            ProviderType provider)
    : BaseObject(env, object),
      async_flags_(NO_OPTIONS),
      provider_type_(provider) {
  if (!env->has_async_listener())
    return;

  // TODO(trevnorris): Do we really need to TryCatch this call?
  v8::TryCatch try_catch;
  try_catch.SetVerbose(true);

  v8::Local<v8::Value> val = object.As<v8::Value>();
  env->async_listener_run_function()->Call(env->process_object(), 1, &val);

  if (!try_catch.HasCaught())
    async_flags_ |= HAS_ASYNC_LISTENER;
}


inline AsyncWrap::~AsyncWrap() {
}

inline uint32_t AsyncWrap::provider_type() const {
  return provider_type_;
}


inline bool AsyncWrap::has_async_listener() {
  return async_flags_ & HAS_ASYNC_LISTENER;
}


// I hate you domains.
inline v8::Handle<v8::Value> AsyncWrap::MakeDomainCallback(
    const v8::Handle<v8::Function> cb,
    int argc,
    v8::Handle<v8::Value>* argv) {
  assert(env()->context() == env()->isolate()->GetCurrentContext());

  v8::Local<v8::Object> context = object();
  v8::Local<v8::Object> process = env()->process_object();
  v8::Local<v8::Value> domain_v = context->Get(env()->domain_string());
  v8::Local<v8::Object> domain;

  v8::TryCatch try_catch;
  try_catch.SetVerbose(true);

  if (has_async_listener()) {
    v8::Local<v8::Value> val = context.As<v8::Value>();
    env()->async_listener_load_function()->Call(process, 1, &val);

    if (try_catch.HasCaught())
      return v8::Undefined(env()->isolate());
  }

  bool has_domain = domain_v->IsObject();
  if (has_domain) {
    domain = domain_v.As<v8::Object>();

    if (domain->Get(env()->disposed_string())->IsTrue())
      return Undefined(env()->isolate());

    v8::Local<v8::Function> enter =
      domain->Get(env()->enter_string()).As<v8::Function>();
    if (enter->IsFunction()) {
      enter->Call(domain, 0, NULL);
      if (try_catch.HasCaught())
        return Undefined(env()->isolate());
    }
  }

  v8::Local<v8::Value> ret = cb->Call(context, argc, argv);

  if (try_catch.HasCaught()) {
    return Undefined(env()->isolate());
  }

  if (has_domain) {
    v8::Local<v8::Function> exit =
      domain->Get(env()->exit_string()).As<v8::Function>();
    if (exit->IsFunction()) {
      exit->Call(domain, 0, NULL);
      if (try_catch.HasCaught())
        return Undefined(env()->isolate());
    }
  }

  if (has_async_listener()) {
    v8::Local<v8::Value> val = context.As<v8::Value>();
    env()->async_listener_unload_function()->Call(process, 1, &val);

    if (try_catch.HasCaught())
      return Undefined(env()->isolate());
  }

  Environment::TickInfo* tick_info = env()->tick_info();

  if (tick_info->in_tick()) {
    return ret;
  }

  if (tick_info->length() == 0) {
    tick_info->set_index(0);
    return ret;
  }

  tick_info->set_in_tick(true);

  env()->tick_callback_function()->Call(process, 0, NULL);

  tick_info->set_in_tick(false);

  if (try_catch.HasCaught()) {
    tick_info->set_last_threw(true);
    return Undefined(env()->isolate());
  }

  return ret;
}


inline v8::Handle<v8::Value> AsyncWrap::MakeCallback(
    const v8::Handle<v8::Function> cb,
    int argc,
    v8::Handle<v8::Value>* argv) {
  if (env()->using_domains())
    return MakeDomainCallback(cb, argc, argv);

  assert(env()->context() == env()->isolate()->GetCurrentContext());

  v8::Local<v8::Object> context = object();
  v8::Local<v8::Object> process = env()->process_object();

  v8::TryCatch try_catch;
  try_catch.SetVerbose(true);

  if (has_async_listener()) {
    v8::Local<v8::Value> val = context.As<v8::Value>();
    env()->async_listener_load_function()->Call(process, 1, &val);

    if (try_catch.HasCaught())
      return v8::Undefined(env()->isolate());
  }

  v8::Local<v8::Value> ret = cb->Call(context, argc, argv);

  if (try_catch.HasCaught()) {
    return Undefined(env()->isolate());
  }

  if (has_async_listener()) {
    v8::Local<v8::Value> val = context.As<v8::Value>();
    env()->async_listener_unload_function()->Call(process, 1, &val);

    if (try_catch.HasCaught())
      return v8::Undefined(env()->isolate());
  }

  Environment::TickInfo* tick_info = env()->tick_info();

  if (tick_info->in_tick()) {
    return ret;
  }

  if (tick_info->length() == 0) {
    tick_info->set_index(0);
    return ret;
  }

  tick_info->set_in_tick(true);

  env()->tick_callback_function()->Call(process, 0, NULL);

  tick_info->set_in_tick(false);

  if (try_catch.HasCaught()) {
    tick_info->set_last_threw(true);
    return Undefined(env()->isolate());
  }

  return ret;
}


inline v8::Handle<v8::Value> AsyncWrap::MakeCallback(
    const v8::Handle<v8::String> symbol,
    int argc,
    v8::Handle<v8::Value>* argv) {
  v8::Local<v8::Value> cb_v = object()->Get(symbol);
  v8::Local<v8::Function> cb = cb_v.As<v8::Function>();
  assert(cb->IsFunction());

  return MakeCallback(cb, argc, argv);
}


inline v8::Handle<v8::Value> AsyncWrap::MakeCallback(
    uint32_t index,
    int argc,
    v8::Handle<v8::Value>* argv) {
  v8::Local<v8::Value> cb_v = object()->Get(index);
  v8::Local<v8::Function> cb = cb_v.As<v8::Function>();
  assert(cb->IsFunction());

  return MakeCallback(cb, argc, argv);
}

}  // namespace node

#endif  // SRC_ASYNC_WRAP_INL_H_
