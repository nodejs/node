#include <stdlib.h>
#include <node.h>
#include <v8.h>

using v8::FunctionCallbackInfo;
using v8::Value;
using v8::Isolate;
using v8::Local;
using v8::Object;
using v8::Boolean;

void EnsureAllocation(const FunctionCallbackInfo<Value> &args) {
  Isolate* isolate = args.GetIsolate();
  uintptr_t size = args[0]->IntegerValue();
  Local<Boolean> success;

  void* buffer = malloc(size);
  if (buffer) {
    success = Boolean::New(isolate, true);
    free(buffer);
  } else {
    success = Boolean::New(isolate, false);
  }
  args.GetReturnValue().Set(success);
}

void init(Local<Object> exports) {
  NODE_SET_METHOD(exports, "ensureAllocation", EnsureAllocation);
}

NODE_MODULE(NODE_GYP_MODULE_NAME, init)
