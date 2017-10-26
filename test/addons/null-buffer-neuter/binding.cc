#include <node.h>
#include <node_buffer.h>
#include <v8.h>

#include <assert.h>

using v8::FunctionCallbackInfo;
using v8::HandleScope;
using v8::Isolate;
using v8::Object;
using v8::Value;
using v8::Local;

static int alive;
static void FreeCallback(char* data, void* hint) {
  assert(data == nullptr);
  alive--;
}

void Run(const FunctionCallbackInfo<Value>& args) {
  Isolate* isolate = args.GetIsolate();
  alive++;

  {
    HandleScope scope(isolate);
    Local<Object> buf = node::Buffer::New(
          isolate,
          nullptr,
          0,
          FreeCallback,
          nullptr).ToLocalChecked();

    char* data = node::Buffer::Data(buf);
    assert(data == nullptr);
  }

  isolate->RequestGarbageCollectionForTesting(
      Isolate::kFullGarbageCollection);

  assert(alive == 0);
}

void init(Local<Object> exports) {
  NODE_SET_METHOD(exports, "run", Run);
}

NODE_MODULE(NODE_GYP_MODULE_NAME, init)
