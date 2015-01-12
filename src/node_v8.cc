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
