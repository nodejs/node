#include <node.h>
#include <assert.h>
#include <unistd.h>

namespace {

inline void Fork(const v8::FunctionCallbackInfo<v8::Value>& info) {
  auto isolate = info.GetIsolate();
  auto context = isolate->GetCurrentContext();

  pid_t pid = fork();

  if (pid == 0) {
    auto env = node::GetCurrentEnvironment(context);
    node::Fork(env);
  }

  auto result = v8::Number::New(info.GetIsolate(), pid);

  info.GetReturnValue().Set(result);
}

inline void Initialize(v8::Local<v8::Object> exports,
                       v8::Local<v8::Value> module,
                       v8::Local<v8::Context> context) {
  auto isolate = context->GetIsolate();
  auto key = v8::String::NewFromUtf8(
      isolate, "fork", v8::NewStringType::kNormal).ToLocalChecked();
  auto value = v8::FunctionTemplate::New(isolate, Fork)
                   ->GetFunction(context)
                   .ToLocalChecked();
  assert(exports->Set(context, key, value).IsJust());
}

}  // anonymous namespace

NODE_MODULE_CONTEXT_AWARE(NODE_GYP_MODULE_NAME, Initialize)
