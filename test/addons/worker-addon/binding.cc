#include <node.h>
#include <v8.h>
#include <uv.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

using v8::Context;
using v8::Function;
using v8::HandleScope;
using v8::Isolate;
using v8::Local;
using v8::MaybeLocal;
using v8::Object;
using v8::String;
using v8::Value;

size_t count = 0;

struct statically_allocated {
  statically_allocated() {
    assert(count == 0);
    printf("ctor ");
  }
  ~statically_allocated() {
    assert(count == 0);
    printf("dtor ");
  }
} var;

void Dummy(void*) {
  assert(0);
}

void Cleanup(void* str) {
  printf("%s ", static_cast<const char*>(str));

  // Check that calling into JS fails.
  Isolate* isolate = Isolate::GetCurrent();
  HandleScope handle_scope(isolate);
  assert(isolate->InContext());
  Local<Context> context = isolate->GetCurrentContext();
  MaybeLocal<Value> call_result =
      context->Global()->Get(
          context, String::NewFromUtf8Literal(isolate, "Object"))
              .ToLocalChecked().As<Function>()->Call(
                  context, v8::Null(isolate), 0, nullptr);
  assert(call_result.IsEmpty());
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

  if (getenv("addExtraItemToEventLoop") != nullptr) {
    // Add an item to the event loop that we do not clean up in order to make
    // sure that for the main thread, this addon's memory persists even after
    // the Environment instance has been destroyed.
    static uv_async_t extra_async;
    uv_loop_t* loop = node::GetCurrentEventLoop(context->GetIsolate());
    int err = uv_async_init(loop, &extra_async, [](uv_async_t*) {});
    assert(err == 0);
    uv_unref(reinterpret_cast<uv_handle_t*>(&extra_async));
  }
}

NODE_MODULE_CONTEXT_AWARE(NODE_GYP_MODULE_NAME, Initialize)
