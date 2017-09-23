#include <node_version.h>
#undef NODE_MODULE_VERSION
#define NODE_MODULE_VERSION 42
#include <node.h>

using v8::Value;
using v8::Local;
using v8::Object;
using v8::Context;

namespace {
  inline void Initialize(Local<Object> exports,
                         Local<Value> module,
                         Local<Context> context) {
  }
}

NODE_MODULE_CONTEXT_AWARE(binding, Initialize)
