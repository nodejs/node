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

#ifndef SRC_NODE_CONTEXTIFY_H_
#define SRC_NODE_CONTEXTIFY_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "v8.h"
#include "base-object.h"
#include "env.h"

namespace node {

namespace contextify {

class ContextifyContext {
 protected:
  // V8 reserves the first field in context objects for the debugger. We use the
  // second field to hold a reference to the sandbox object.
  enum { kSandboxObjectIndex = 1 };

  Environment* const env_;
  v8::Persistent<v8::Context> context_;

 public:
  ContextifyContext(Environment* env, v8::Local<v8::Object> sandbox_obj);


  ~ContextifyContext();


  inline Environment* env() const {
    return env_;
  }


  inline v8::Local<v8::Context> context() const {
    return PersistentToLocal(env()->isolate(), context_);
  }


  inline v8::Local<v8::Object> global_proxy() const {
    return context()->Global();
  }


  inline v8::Local<v8::Object> sandbox() const {
    return v8::Local<v8::Object>::Cast(
        context()->GetEmbedderData(kSandboxObjectIndex));
  }

  void CopyProperties();

  static ContextifyContext* ContextFromContextifiedSandbox(
      Environment* env,
      const v8::Local<v8::Object>& sandbox);

  static void Init(Environment* env, v8::Local<v8::Object> target);

 private:
  v8::Local<v8::Value> CreateDataWrapper(Environment* env);
  v8::Local<v8::Context> CreateV8Context(Environment* env, v8::Local<v8::Object> sandbox_obj);

  static void RunInDebugContext(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void MakeContext(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void IsContext(const v8::FunctionCallbackInfo<v8::Value>& args);

  static void WeakCallback(const v8::WeakCallbackInfo<ContextifyContext>& data);
  static void GlobalPropertyGetterCallback(
      v8::Local<v8::Name> property,
      const v8::PropertyCallbackInfo<v8::Value>& args);
  static void GlobalPropertySetterCallback(
      v8::Local<v8::Name> property,
      v8::Local<v8::Value> value,
      const v8::PropertyCallbackInfo<v8::Value>& args);
  static void GlobalPropertyQueryCallback(
      v8::Local<v8::Name> property,
      const v8::PropertyCallbackInfo<v8::Integer>& args);
  static void GlobalPropertyDeleterCallback(
      v8::Local<v8::Name> property,
      const v8::PropertyCallbackInfo<v8::Boolean>& args);
  static void GlobalPropertyEnumeratorCallback(
      const v8::PropertyCallbackInfo<v8::Array>& args);
};

}  // namespace contextify

}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_NODE_CONTEXTIFY_H_
