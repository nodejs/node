#include <node.h>
#include <v8.h>

using v8::FunctionCallbackInfo;
using v8::Local;
using v8::MaybeLocal;
using v8::Object;
using v8::Value;

void TriggerFatalError(const FunctionCallbackInfo<Value>& args) {
  // Trigger a v8 ApiCheck failure.
  MaybeLocal<Value> value;
  value.ToLocalChecked();
}

void init(Local<Object> exports) {
  NODE_SET_METHOD(exports, "triggerFatalError", TriggerFatalError);
}

NODE_MODULE(NODE_GYP_MODULE_NAME, init)
