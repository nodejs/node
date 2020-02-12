#include <stdlib.h>
#include <node.h>
#include <v8.h>

#include <aliased_buffer.h>
#include <util-inl.h>

void AllocateAndResizeBuffer(
  const v8::FunctionCallbackInfo<v8::Value>& args) {
    v8::Isolate* isolate = args.GetIsolate();
    int64_t length = args[0].As<v8::BigInt>()->Int64Value();

     node::AliasedBigUint64Array array{isolate, 0};

    array.reserve(length);
    assert(false);
  }

void init(v8::Local<v8::Object> exports) {
  NODE_SET_METHOD(exports,
                  "allocateAndResizeBuffer",
                  AllocateAndResizeBuffer);
}
