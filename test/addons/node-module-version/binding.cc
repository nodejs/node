#include <node_version.h>
#undef NODE_MODULE_VERSION
#define NODE_MODULE_VERSION 42
#include <node.h>

using namespace v8;
namespace {

  inline void Initialize(Local<Object> exports,
                         Local<Value> module,
                         Local<Context> context) {
  }

}

NODE_MODULE_CONTEXT_AWARE(binding, Initialize)
