#include <node_version.h>
#undef NODE_MODULE_VERSION
#define NODE_MODULE_VERSION 42
#include <node.h>

namespace {

using v8::Local;
using v8::String;
using v8::Value;
using v8::Context;

inline void Initialize(Local<Object> exports,
                       Local<Value> module,
                       Local<Context> context) {
}

}  // namespace

NODE_MODULE_CONTEXT_AWARE(NODE_GYP_MODULE_NAME, Initialize)
