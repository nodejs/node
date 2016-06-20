#include <node.h>
#include <node_buffer.h>
#include <v8.h>

#include <assert.h>

static int alive;
static char buf[1024];

static void FreeCallback(char* data, void* hint) {
  alive--;
}

void Alloc(const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  alive++;

  uintptr_t alignment = args[1]->IntegerValue();
  uintptr_t offset = args[2]->IntegerValue();

  uintptr_t static_offset = reinterpret_cast<uintptr_t>(buf) % alignment;
  char* aligned = buf + (alignment - static_offset) + offset;

  args.GetReturnValue().Set(node::Buffer::New(
        isolate,
        aligned,
        args[0]->IntegerValue(),
        FreeCallback,
        nullptr).ToLocalChecked());
}

void Check(const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  isolate->RequestGarbageCollectionForTesting(
      v8::Isolate::kFullGarbageCollection);
  assert(alive > 0);
}

void init(v8::Local<v8::Object> target) {
  NODE_SET_METHOD(target, "alloc", Alloc);
  NODE_SET_METHOD(target, "check", Check);
}

NODE_MODULE(binding, init);
