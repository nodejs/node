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

#include "async-wrap.h"
#include "async-wrap-inl.h"
#include "env.h"
#include "env-inl.h"
#include "util.h"
#include "util-inl.h"

#include "v8.h"

using v8::Function;
using v8::Handle;
using v8::Local;
using v8::Object;
using v8::TryCatch;
using v8::Value;

namespace node {

Handle<Value> AsyncWrap::MakeDomainCallback(const Handle<Function> cb,
                                            int argc,
                                            Handle<Value>* argv) {
  CHECK(env()->context() == env()->isolate()->GetCurrentContext());

  Local<Object> context = object();
  Local<Object> process = env()->process_object();
  Local<Value> domain_v = context->Get(env()->domain_string());
  Local<Object> domain;

  TryCatch try_catch;
  try_catch.SetVerbose(true);

  bool has_domain = domain_v->IsObject();
  if (has_domain) {
    domain = domain_v.As<Object>();

    if (domain->Get(env()->disposed_string())->IsTrue())
      return Undefined(env()->isolate());

    Local<Function> enter =
      domain->Get(env()->enter_string()).As<Function>();
    if (enter->IsFunction()) {
      enter->Call(domain, 0, NULL);
      if (try_catch.HasCaught())
        return Undefined(env()->isolate());
    }
  }

  Local<Value> ret = cb->Call(context, argc, argv);

  if (try_catch.HasCaught()) {
    return Undefined(env()->isolate());
  }

  if (has_domain) {
    Local<Function> exit =
      domain->Get(env()->exit_string()).As<Function>();
    if (exit->IsFunction()) {
      exit->Call(domain, 0, NULL);
      if (try_catch.HasCaught())
        return Undefined(env()->isolate());
    }
  }

  Environment::TickInfo* tick_info = env()->tick_info();

  if (tick_info->in_tick()) {
    return ret;
  }

  if (tick_info->length() == 0) {
    env()->isolate()->RunMicrotasks();
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


Handle<Value> AsyncWrap::MakeCallback(const Handle<Function> cb,
                                      int argc,
                                      Handle<Value>* argv) {
  if (env()->using_domains())
    return MakeDomainCallback(cb, argc, argv);

  CHECK(env()->context() == env()->isolate()->GetCurrentContext());

  Local<Object> context = object();
  Local<Object> process = env()->process_object();

  TryCatch try_catch;
  try_catch.SetVerbose(true);

  Local<Value> ret = cb->Call(context, argc, argv);

  if (try_catch.HasCaught()) {
    return Undefined(env()->isolate());
  }

  Environment::TickInfo* tick_info = env()->tick_info();

  if (tick_info->in_tick()) {
    return ret;
  }

  if (tick_info->length() == 0) {
    env()->isolate()->RunMicrotasks();
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

}  // namespace node
