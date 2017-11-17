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

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "async_wrap.h"
#include "base_object-inl.h"
#include "node_internals.h"

namespace node {

inline AsyncWrap::ProviderType AsyncWrap::provider_type() const {
  return provider_type_;
}


inline double AsyncWrap::get_async_id() const {
  return async_id_;
}


inline double AsyncWrap::get_trigger_async_id() const {
  return trigger_async_id_;
}


inline v8::MaybeLocal<v8::Value> AsyncWrap::MakeCallback(
    const v8::Local<v8::String> symbol,
    int argc,
    v8::Local<v8::Value>* argv) {
  v8::Local<v8::Value> cb_v = object()->Get(symbol);
  CHECK(cb_v->IsFunction());
  return MakeCallback(cb_v.As<v8::Function>(), argc, argv);
}


inline v8::MaybeLocal<v8::Value> AsyncWrap::MakeCallback(
    uint32_t index,
    int argc,
    v8::Local<v8::Value>* argv) {
  v8::Local<v8::Value> cb_v = object()->Get(index);
  CHECK(cb_v->IsFunction());
  return MakeCallback(cb_v.As<v8::Function>(), argc, argv);
}

}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_ASYNC_WRAP_INL_H_
