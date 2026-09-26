#include "async_context_frame.h"
#include "env-inl.h"
#include "node.h"
#include "node_internals.h"

namespace node {

using v8::Function;
using v8::Isolate;
using v8::Local;
using v8::MaybeLocal;
using v8::Object;
using v8::String;
using v8::Undefined;
using v8::Value;

AsyncResource::AsyncResource(Isolate* isolate,
                             Local<Object> resource,
                             const char* name,
                             async_id trigger_async_id)
    : AsyncResource(
          isolate, resource, std::string_view(name), trigger_async_id) {}

AsyncResource::AsyncResource(Isolate* isolate,
                             Local<Object> resource,
                             std::string_view name,
                             async_id trigger_async_id)
    : env_(Environment::GetCurrent(isolate)),
      resource_(isolate, resource),
      context_frame_(isolate, async_context_frame::current(isolate)) {
  CHECK_NOT_NULL(env_);
  async_context_ = EmitAsyncInit(isolate, resource, name, trigger_async_id);
}

AsyncResource::~AsyncResource() {
  CHECK_NOT_NULL(env_);
  EmitAsyncDestroy(env_, async_context_);
}

MaybeLocal<Value> AsyncResource::MakeCallback(Local<Function> callback,
                                              int argc,
                                              Local<Value>* argv) {
  auto isolate = env_->isolate();
  // As in Node-API: node::MakeCallback() would run it with no frame.
  return InternalMakeCallback(isolate,
                              get_resource(),
                              callback,
                              argc,
                              argv,
                              async_context_,
                              context_frame_.Get(isolate));
}

MaybeLocal<Value> AsyncResource::MakeCallback(const char* method,
                                              int argc,
                                              Local<Value>* argv) {
  Local<String> method_string;
  if (!String::NewFromUtf8(env_->isolate(), method).ToLocal(&method_string)) {
    return {};
  }
  return MakeCallback(method_string, argc, argv);
}

MaybeLocal<Value> AsyncResource::MakeCallback(Local<String> symbol,
                                              int argc,
                                              Local<Value>* argv) {
  auto isolate = env_->isolate();
  // Check can_call_into_js() first because calling Get() might do so.
  if (!env_->can_call_into_js()) return {};
  Local<Value> callback;
  if (!get_resource()
           ->Get(isolate->GetCurrentContext(), symbol)
           .ToLocal(&callback)) {
    return {};
  }
  if (!callback->IsFunction()) return Undefined(isolate);
  return MakeCallback(callback.As<Function>(), argc, argv);
}

Local<Object> AsyncResource::get_resource() {
  return resource_.Get(env_->isolate());
}

async_id AsyncResource::get_async_id() const {
  return async_context_.async_id;
}

async_id AsyncResource::get_trigger_async_id() const {
  return async_context_.trigger_async_id;
}

AsyncResource::CallbackScope::CallbackScope(AsyncResource* res)
    : node::CallbackScope(res->env_,
                          res->resource_.Get(res->env_->isolate()),
                          res->async_context_) {}

}  // namespace node
