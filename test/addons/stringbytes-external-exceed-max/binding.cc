#include <stdlib.h>
#include <node.h>
#include <v8.h>

void EnsureAllocation(const v8::FunctionCallbackInfo<v8::Value> &args) {
  v8::Isolate* isolate = args.GetIsolate();
  uintptr_t size = args[0]->IntegerValue();
  v8::Local<v8::Boolean> success;

  void* buffer = malloc(size);
  if (buffer) {
    success = v8::Boolean::New(isolate, true);
    free(buffer);
  } else {
    success = v8::Boolean::New(isolate, false);
  }
  args.GetReturnValue().Set(success);
}

void init(v8::Local<v8::Object> exports) {
  NODE_SET_METHOD(exports, "ensureAllocation", EnsureAllocation);
}

NODE_MODULE(binding, init);
