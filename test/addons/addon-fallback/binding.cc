#include "node.h"

void Init(v8::Local<v8::Object> exports,
          v8::Local<v8::Value> module,
          v8::Local<v8::Context> context) {
  const char *error = nullptr;

  v8::Isolate *isolate = context->GetIsolate();
  v8::Local<v8::Object> result = v8::Object::New(isolate);
  v8::Local<v8::String> prop_name;
  auto answer = v8::Number::New(isolate, 42.0).As<v8::Value>();

  prop_name = v8::String::NewFromUtf8(isolate, "answer",
      v8::NewStringType::kNormal).ToLocalChecked();
  if (result->Set(context, prop_name, answer).FromJust()) {
    prop_name = v8::String::NewFromUtf8(isolate, "exports",
        v8::NewStringType::kNormal).ToLocalChecked();
    if (module.As<v8::Object>()->Set(context, prop_name, result).FromJust()) {
      return;
    } else {
      error = "Failed to set exports";
    }
  } else {
    error = "Failed to set property";
  }

  isolate->ThrowException(
      v8::String::NewFromUtf8(isolate, error,
          v8::NewStringType::kNormal).ToLocalChecked());
}

NODE_MODULE_CONTEXT_AWARE(NODE_GYP_MODULE_NAME, Init)
NODE_MODULE_FALLBACK(NODE_GYP_MODULE_NAME)
