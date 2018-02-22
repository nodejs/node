#include <node.h>
#include <v8.h>

void Method(const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  args.GetReturnValue().Set(v8::String::NewFromUtf8(isolate, "world"));
}

#define CONCAT(a, b) CONCAT_HELPER(a, b)
#define CONCAT_HELPER(a, b) a##b
#define INITIALIZER CONCAT(node_register_module_v, NODE_MODULE_VERSION)

extern "C" NODE_MODULE_EXPORT void INITIALIZER(v8::Local<v8::Object> exports,
                                               v8::Local<v8::Value> module,
                                               v8::Local<v8::Context> context) {
  NODE_SET_METHOD(exports, "hello", Method);
}
