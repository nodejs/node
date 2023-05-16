#include <node_version.h>
#undef NODE_MODULE_VERSION
#define NODE_MODULE_VERSION 42
#include <node.h>

namespace {

inline void Initialize(v8::Local<v8::Object> exports,
                       v8::Local<v8::Value> module,
                       v8::Local<v8::Context> context) {
}

}

NODE_MODULE_CONTEXT_AWARE(NODE_GYP_MODULE_NAME, Initialize)
