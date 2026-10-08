#include <node.h>
#include <uv.h>
#include <v8.h>

using v8::Context;
using v8::Function;
using v8::FunctionCallbackInfo;
using v8::Global;
using v8::HandleScope;
using v8::Isolate;
using v8::Local;
using v8::Object;
using v8::Value;

struct State {
  uv_timer_t timer;
  Isolate* isolate;
  int64_t n;
  Global<Function> fn;
  Global<Function> done;
};

static void OnTimer(uv_timer_t* handle) {
  State* state = static_cast<State*>(handle->data);
  Isolate* isolate = state->isolate;
  HandleScope handle_scope(isolate);
  Local<Function> fn = state->fn.Get(isolate);
  Local<Context> context = fn->GetCreationContextChecked(isolate);
  Context::Scope context_scope(context);
  Local<Object> recv = context->Global();
  for (int64_t i = 0; i < state->n; i++) {
    HandleScope inner_scope(isolate);
    (void)node::MakeCallback(isolate, recv, fn, 0, nullptr, {0, 0});
  }
  Local<Function> done = state->done.Get(isolate);
  (void)node::MakeCallback(isolate, recv, done, 0, nullptr, {0, 0});
  uv_close(reinterpret_cast<uv_handle_t*>(&state->timer),
           [](uv_handle_t* h) { delete static_cast<State*>(h->data); });
}

// run(n, fn, done): calls fn n times from a timer, then calls done.
static void Run(const FunctionCallbackInfo<Value>& args) {
  Isolate* isolate = args.GetIsolate();
  State* state = new State;
  state->isolate = isolate;
  state->n = args[0]->IntegerValue(isolate->GetCurrentContext()).FromJust();
  state->fn.Reset(isolate, args[1].As<Function>());
  state->done.Reset(isolate, args[2].As<Function>());
  state->timer.data = state;
  uv_timer_init(node::GetCurrentEventLoop(isolate), &state->timer);
  uv_timer_start(&state->timer, OnTimer, 0, 0);
}

static void Initialize(Local<Object> target, Local<Value> module, void* data) {
  NODE_SET_METHOD(target, "run", Run);
}

NODE_MODULE(NODE_GYP_MODULE_NAME, Initialize)
