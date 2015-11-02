#include <node.h>
#include <node_buffer.h>
#include <util.h>
#include <v8.h>

static int alive;

static void FreeCallback(char* data, void* hint) {
  CHECK_EQ(data, nullptr);
  alive--;
}

void Run(const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  alive++;

  {
    v8::HandleScope scope(isolate);
    v8::Local<v8::Object> buf = node::Buffer::New(
          isolate,
          nullptr,
          0,
          FreeCallback,
          nullptr).ToLocalChecked();

    char* data = node::Buffer::Data(buf);
    CHECK_EQ(data, nullptr);
  }

  isolate->RequestGarbageCollectionForTesting(
      v8::Isolate::kFullGarbageCollection);

  CHECK_EQ(alive, 0);
}

void init(v8::Local<v8::Object> target) {
  NODE_SET_METHOD(target, "run", Run);
}

NODE_MODULE(binding, init);
