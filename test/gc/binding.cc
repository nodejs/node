#include "node.h"
#include "uv.h"

#include <assert.h>
#include <stdlib.h>

#include <vector>

#ifdef NDEBUG
#define CHECK(x) do { if (!(x)) abort(); } while (false)
#else
#define CHECK assert
#endif

#define CHECK_EQ(a, b) CHECK((a) == (b))

namespace {

struct Callback {
  inline Callback(v8::Isolate* isolate,
                  v8::Local<v8::Object> object,
                  v8::Local<v8::Function> function)
      : object(isolate, object), function(isolate, function) {}

  v8::Persistent<v8::Object> object;
  v8::Persistent<v8::Function> function;
};

static uv_async_t async_handle;
static std::vector<Callback*> callbacks;

inline void Prime() {
  uv_ref(reinterpret_cast<uv_handle_t*>(&async_handle));
  CHECK_EQ(0, uv_async_send(&async_handle));
}

inline void AsyncCallback(uv_async_t*) {
  auto isolate = v8::Isolate::GetCurrent();
  v8::HandleScope handle_scope(isolate);
  auto context = isolate->GetCurrentContext();
  auto global = context->Global();
  while (!callbacks.empty()) {
    v8::HandleScope handle_scope(isolate);
    auto callback = callbacks.back();
    callbacks.pop_back();
    auto function = v8::Local<v8::Function>::New(isolate, callback->function);
    delete callback;
    if (node::MakeCallback(isolate, global, function, 0, nullptr).IsEmpty())
      return Prime();  // Have exception, flush remainder on next loop tick.
  }
  uv_unref(reinterpret_cast<uv_handle_t*>(&async_handle));
}

inline void OnGC(const v8::FunctionCallbackInfo<v8::Value>& info) {
  CHECK(info[0]->IsObject());
  CHECK(info[1]->IsFunction());
  auto object = info[0].As<v8::Object>();
  auto function = info[1].As<v8::Function>();
  auto callback = new Callback(info.GetIsolate(), object, function);
  auto on_callback = [] (const v8::WeakCallbackInfo<Callback>& data) {
    auto callback = data.GetParameter();
    callbacks.push_back(callback);
    callback->object.Reset();
    Prime();
  };
  callback->object.SetWeak(callback, on_callback,
                           v8::WeakCallbackType::kParameter);
}

inline void Initialize(v8::Local<v8::Object> exports,
                       v8::Local<v8::Value> module,
                       v8::Local<v8::Context> context) {
  NODE_SET_METHOD(module->ToObject(context).ToLocalChecked(), "exports", OnGC);
  CHECK_EQ(0, uv_async_init(uv_default_loop(), &async_handle, AsyncCallback));
  uv_unref(reinterpret_cast<uv_handle_t*>(&async_handle));
}

}  // anonymous namespace

NODE_MODULE_CONTEXT_AWARE(binding, Initialize)
