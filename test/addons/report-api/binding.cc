#include <node.h>
#include <v8.h>

using v8::FunctionCallbackInfo;
using v8::Isolate;
using v8::Local;
using v8::Object;
using v8::Value;

void TriggerReport(const FunctionCallbackInfo<Value>& args) {
  Isolate* isolate = args.GetIsolate();

  node::TriggerNodeReport(
      isolate, "FooMessage", "BarTrigger", std::string(), Local<Value>());
}

void TriggerReportNoIsolate(const FunctionCallbackInfo<Value>& args) {
  node::TriggerNodeReport(static_cast<Isolate*>(nullptr),
                          "FooMessage",
                          "BarTrigger",
                          std::string(),
                          Local<Value>());
}

void TriggerReportEnv(const FunctionCallbackInfo<Value>& args) {
  Isolate* isolate = args.GetIsolate();

  node::TriggerNodeReport(
      node::GetCurrentEnvironment(isolate->GetCurrentContext()),
      "FooMessage",
      "BarTrigger",
      std::string(),
      Local<Value>());
}

void TriggerReportNoEnv(const FunctionCallbackInfo<Value>& args) {
  Isolate* isolate = args.GetIsolate();

  node::TriggerNodeReport(static_cast<node::Environment*>(nullptr),
                          "FooMessage",
                          "BarTrigger",
                          std::string(),
                          Local<Value>());
}

void init(Local<Object> exports) {
  NODE_SET_METHOD(exports, "triggerReport", TriggerReport);
  NODE_SET_METHOD(exports, "triggerReportNoIsolate", TriggerReportNoIsolate);
  NODE_SET_METHOD(exports, "triggerReportEnv", TriggerReportEnv);
  NODE_SET_METHOD(exports, "triggerReportNoEnv", TriggerReportNoEnv);
}

NODE_MODULE(NODE_GYP_MODULE_NAME, init)
