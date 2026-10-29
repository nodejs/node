#include <node.h>
#include <node_buffer.h>
#include <v8.h>

#include <assert.h>

static int alive;

static void FreeCallback(char* data, void* hint) {
  assert(data == nullptr);
  alive--;
}

void IsAlive(const v8::FunctionCallbackInfo<v8::Value>& args) {
  args.GetReturnValue().Set(alive);
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
    assert(data == nullptr);
  }
}

void init(v8::Local<v8::Object> exports) {
  NODE_SET_METHOD(exports, "run", Run);
  NODE_SET_METHOD(exports, "isAlive", IsAlive);
}

NODE_MODULE(NODE_GYP_MODULE_NAME, init)
