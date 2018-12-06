#include <assert.h>
#include <node.h>
#include <stdio.h>
#include <stdlib.h>
#include <v8.h>

using v8::Context;
using v8::HandleScope;
using v8::Isolate;
using v8::Local;
using v8::Object;
using v8::Value;

size_t count = 0;

struct statically_allocated {
  statically_allocated() {
    assert(count == 0);
    printf("ctor ");
  }
  ~statically_allocated() {
    assert(count == 0);
    printf("dtor");
  }
} var;

void Dummy(void*) {
  assert(0);
}

void Cleanup(void* str) {
  printf("%s ", static_cast<const char*>(str));
}

void Initialize(Local<Object> exports,
                Local<Value> module,
                Local<Context> context) {
  node::AddEnvironmentCleanupHook(
      context->GetIsolate(),
      Cleanup,
      const_cast<void*>(static_cast<const void*>("cleanup")));
  node::AddEnvironmentCleanupHook(context->GetIsolate(), Dummy, nullptr);
  node::RemoveEnvironmentCleanupHook(context->GetIsolate(), Dummy, nullptr);
}

NODE_MODULE_CONTEXT_AWARE(NODE_GYP_MODULE_NAME, Initialize)
