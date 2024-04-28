#include "debug_utils-inl.h"
#include "env-inl.h"
#include "node_errors.h"
#include "node_external_reference.h"
#include "v8-function-callback.h"
#include "v8-function.h"

namespace node {

using v8::Context;
using v8::Function;
using v8::FunctionCallbackInfo;
using v8::Local;
using v8::Object;
using v8::Symbol;
using v8::Undefined;
using v8::Value;

namespace async_context_frame {

namespace {
Local<Value> CopyCurrentOrCreate(Environment* env,
                                 Local<Symbol> key,
                                 Local<Value> value) {
  auto current = env->isolate()->GetContinuationPreservedEmbedderData();
  CHECK(!current.IsEmpty());
  Local<Object> frame;
  if (!current->IsObject()) {
    CHECK(current->IsUndefined());
    v8::Local<v8::Name> name = key;
    return Object::New(
        env->isolate(), v8::Null(env->isolate()), &name, &value, 1);
  }
  CHECK(current->IsObject());
  frame = current.As<Object>()->Clone();
  USE(frame->Set(env->context(), key, value));
  return frame;
}

class Scope final {
 public:
  Scope(Environment* env, Local<Value> frame)
      : env_(env),
        prev_(env->isolate()->GetContinuationPreservedEmbedderData()) {
    CHECK(!frame.IsEmpty());
    CHECK(prev_->IsUndefined() || prev_->IsObject());
    Debug(env,
          DebugCategory::ASYNC_CONTEXT,
          "Entering AsyncContextFrame::Scope\n");
    if (frame->IsObject()) {
      env_->isolate()->SetContinuationPreservedEmbedderData(frame);
      Notify(env, frame);
    } else {
      CHECK(frame->IsUndefined());
      env_->isolate()->SetContinuationPreservedEmbedderData(
          Undefined(env->isolate()));
      Notify(env, Undefined(env->isolate()));
    }
  }
  ~Scope() {
    Debug(env_,
          DebugCategory::ASYNC_CONTEXT,
          "Leaving AsyncContextFrame::Scope\n");
    env_->isolate()->SetContinuationPreservedEmbedderData(prev_);
  }

  static void Notify(Environment* env, v8::Local<v8::Value> frame) {
    if (!env->async_context_frame_scope().IsEmpty()) {
      USE(env->async_context_frame_scope()->Call(
          env->context(), Undefined(env->isolate()), 1, &frame));
    }
  }

  Scope(const Scope&) = delete;
  Scope(Scope&&) = delete;
  Scope& operator=(const Scope&) = delete;
  Scope& operator=(Scope&&) = delete;

  void* operator new(size_t) = delete;
  void* operator new[](size_t) = delete;
  void operator delete(void*) = delete;
  void operator delete[](void*) = delete;

 private:
  Environment* env_;
  Local<Value> prev_;
};
}  // namespace

// ============================================================================

void RunWithin(const FunctionCallbackInfo<Value>& args) {
  auto env = Environment::GetCurrent(args);
  if (!args[0]->IsObject() && !args[0]->IsUndefined()) {
    THROW_ERR_INVALID_ARG_TYPE(env,
                               "first argument must be an object or undefined");
    return;
  }
  if (!args[1]->IsFunction()) {
    THROW_ERR_INVALID_ARG_TYPE(env, "second argument must be a function");
    return;
  }
  Scope scope(env, args[0]);
  Local<Value> ret;
  if (args[1]
          .As<Function>()
          ->Call(env->context(), Undefined(env->isolate()), 0, nullptr)
          .ToLocal(&ret)) {
    args.GetReturnValue().Set(ret);
  }
}

void Run(const FunctionCallbackInfo<Value>& args) {
  auto env = Environment::GetCurrent(args);
  if (!args[0]->IsSymbol()) {
    THROW_ERR_INVALID_ARG_TYPE(env, "first argument must be a symbol");
  }
  if (!args[2]->IsFunction()) {
    THROW_ERR_INVALID_ARG_TYPE(env, "second argument must be a function");
    return;
  }

  auto key = args[0].As<Symbol>();
  auto function = args[2].As<Function>();
  Scope scope(env, CopyCurrentOrCreate(env, key, args[1]));
  Local<Value> ret;
  if (function->Call(env->context(), Undefined(env->isolate()), 0, nullptr)
          .ToLocal(&ret)) {
    args.GetReturnValue().Set(ret);
  }
}

void Enter(const FunctionCallbackInfo<Value>& args) {
  auto env = Environment::GetCurrent(args);
  auto frame = env->isolate()->GetContinuationPreservedEmbedderData();
  // If this is being called, then we should be in the empty frame,
  // so frame should be undefined.
  CHECK(frame->IsUndefined());
  auto obj = v8::Object::New(
      env->isolate(), v8::Null(env->isolate()), nullptr, nullptr, 0);
  env->isolate()->SetContinuationPreservedEmbedderData(obj);
  args.GetReturnValue().Set(obj);
}

void SetAsyncContextFrameScopeCallback(
    const FunctionCallbackInfo<Value>& args) {
  auto env = Environment::GetCurrent(args);
  CHECK(args[0]->IsFunction());
  CHECK(env->async_context_frame_scope().IsEmpty());
  env->set_async_context_frame_scope(args[0].As<Function>());
  Scope::Notify(env, env->isolate()->GetContinuationPreservedEmbedderData());
}

void RegisterExternalReferences(ExternalReferenceRegistry* registry) {
  registry->Register(SetAsyncContextFrameScopeCallback);
  registry->Register(RunWithin);
  registry->Register(Run);
  registry->Register(Enter);
}

void Initialize(Local<Object> target,
                Local<Value> unused,
                Local<Context> context,
                void* priv) {
  SetMethod(context,
            target,
            "setAsyncContextFrameScopeCallback",
            SetAsyncContextFrameScopeCallback);
  SetMethod(context, target, "runWithin", RunWithin);
  SetMethod(context, target, "run", Run);
  SetMethod(context, target, "enter", Enter);
}

}  // namespace async_context_frame
}  // namespace node

NODE_BINDING_CONTEXT_AWARE_INTERNAL(async_context,
                                    node::async_context_frame::Initialize)
NODE_BINDING_EXTERNAL_REFERENCE(
    async_context, node::async_context_frame::RegisterExternalReferences)
