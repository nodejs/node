#include <node.h>
#include <node_buffer.h>
#include <util.h>
#include <v8.h>

static int alive;
static char buf[1024];

static void FreeCallback(char* data, void* hint) {
  alive--;
}

void Alloc(const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  alive++;
  args.GetReturnValue().Set(node::Buffer::New(
        isolate,
        buf,
        args[0]->IntegerValue(),
        FreeCallback,
        nullptr).ToLocalChecked());
}

void Check(const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  isolate->RequestGarbageCollectionForTesting(
      v8::Isolate::kFullGarbageCollection);
  CHECK_GT(alive, 0);
}

void init(v8::Local<v8::Object> target) {
  NODE_SET_METHOD(target, "alloc", Alloc);
  NODE_SET_METHOD(target, "check", Check);
}

NODE_MODULE(binding, init);
