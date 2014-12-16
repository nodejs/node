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

#include "node.h"
#include "env.h"
#include "env-inl.h"
#include "util.h"
#include "util-inl.h"
#include "v8.h"

namespace node {

using v8::Context;
using v8::Function;
using v8::FunctionCallbackInfo;
using v8::Handle;
using v8::HeapStatistics;
using v8::Isolate;
using v8::Local;
using v8::Object;
using v8::String;
using v8::Uint32;
using v8::V8;
using v8::Value;


void GetHeapStatistics(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  Isolate* isolate = args.GetIsolate();
  HeapStatistics s;
  isolate->GetHeapStatistics(&s);
  Local<Object> info = Object::New(isolate);
  // TODO(trevnorris): Setting many object properties in C++ is a significant
  // performance hit. Redo this to pass the results to JS and create/set the
  // properties there.
#define V(name)                                                               \
  info->Set(env->name ## _string(), Uint32::NewFromUnsigned(isolate, s.name()))
  V(total_heap_size);
  V(total_heap_size_executable);
  V(total_physical_size);
  V(used_heap_size);
  V(heap_size_limit);
#undef V
  args.GetReturnValue().Set(info);
}


void SetFlagsFromString(const FunctionCallbackInfo<Value>& args) {
  String::Utf8Value flags(args[0]);
  V8::SetFlagsFromString(*flags, flags.length());
}


void InitializeV8Bindings(Handle<Object> target,
                          Handle<Value> unused,
                          Handle<Context> context) {
  Environment* env = Environment::GetCurrent(context);
  env->SetMethod(target, "getHeapStatistics", GetHeapStatistics);
  env->SetMethod(target, "setFlagsFromString", SetFlagsFromString);
}

}  // namespace node

NODE_MODULE_CONTEXT_AWARE_BUILTIN(v8, node::InitializeV8Bindings)
