#include <node.h>
#include <v8.h>
#include <thread>  // NOLINT(build/c++11)

using node::Environment;
using v8::Context;
using v8::FunctionCallbackInfo;
using v8::HandleScope;
using v8::Isolate;
using v8::Local;
using v8::Maybe;
using v8::Object;
using v8::String;
using v8::Value;

static std::thread interrupt_thread;

void ScheduleInterrupt(const FunctionCallbackInfo<Value>& args) {
  Isolate* isolate = args.GetIsolate();
  HandleScope handle_scope(isolate);
  Environment* env = node::GetCurrentEnvironment(isolate->GetCurrentContext());

  interrupt_thread = std::thread([=]() {
    std::this_thread::sleep_for(std::chrono::seconds(1));
    node::RequestInterrupt(
        env,
        [](void* data) {
          // Interrupt is called from JS thread.
          interrupt_thread.join();
          exit(0);
        },
        nullptr);
  });
}

void ScheduleInterruptWithJS(const FunctionCallbackInfo<Value>& args) {
  Isolate* isolate = args.GetIsolate();
  HandleScope handle_scope(isolate);
  Environment* env = node::GetCurrentEnvironment(isolate->GetCurrentContext());

  interrupt_thread = std::thread([=]() {
    std::this_thread::sleep_for(std::chrono::seconds(1));
    node::RequestInterrupt(
        env,
        [](void* data) {
          // Interrupt is called from JS thread.
          interrupt_thread.join();
          Isolate* isolate = static_cast<Isolate*>(data);
          HandleScope handle_scope(isolate);
          Local<Context> ctx = isolate->GetCurrentContext();
          Local<String> str =
              String::NewFromUtf8(isolate, "interrupt").ToLocalChecked();
          // Calling into JS should abort immediately.
          Maybe<bool> result = ctx->Global()->Set(ctx, str, str);
          // Should not reach here.
          if (!result.IsNothing()) {
            // Called into JavaScript.
            exit(2);
          }
          // Maybe exception thrown.
          exit(1);
        },
        isolate);
  });
}

void init(Local<Object> exports) {
  NODE_SET_METHOD(exports, "scheduleInterrupt", ScheduleInterrupt);
  NODE_SET_METHOD(exports, "ScheduleInterruptWithJS", ScheduleInterruptWithJS);
}

NODE_MODULE(NODE_GYP_MODULE_NAME, init)
