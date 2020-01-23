#include "env-inl.h"
#include "node_internals.h"
#include "node_process.h"
#include "async_wrap.h"

namespace node {

using v8::Context;
using v8::HandleScope;
using v8::Integer;
using v8::Isolate;
using v8::Local;
using v8::Object;
using v8::String;
using v8::Value;
using v8::NewStringType;

void RunAtExit(Environment* env) {
  env->RunAtExitCallbacks();
}

void AtExit(void (*cb)(void* arg), void* arg) {
  auto env = Environment::GetThreadLocalEnv();
  AtExit(env, cb, arg);
}

void AtExit(Environment* env, void (*cb)(void* arg), void* arg) {
  CHECK_NOT_NULL(env);
  env->AtExit(cb, arg);
}

void EmitBeforeExit(Environment* env) {
  env->RunBeforeExitCallbacks();

  HandleScope handle_scope(env->isolate());
  Context::Scope context_scope(env->context());
  Local<Value> exit_code = env->process_object()
                               ->Get(env->context(), env->exit_code_string())
                               .ToLocalChecked()
                               ->ToInteger(env->context())
                               .ToLocalChecked();
  ProcessEmit(env, "beforeExit", exit_code).ToLocalChecked();
}

int EmitExit(Environment* env) {
  // process.emit('exit')
  HandleScope handle_scope(env->isolate());
  Context::Scope context_scope(env->context());
  Local<Object> process_object = env->process_object();
  process_object
      ->Set(env->context(),
            FIXED_ONE_BYTE_STRING(env->isolate(), "_exiting"),
            True(env->isolate()))
      .Check();

  Local<String> exit_code = env->exit_code_string();
  int code = process_object->Get(env->context(), exit_code)
                 .ToLocalChecked()
                 ->Int32Value(env->context())
                 .ToChecked();
  ProcessEmit(env, "exit", Integer::New(env->isolate(), code));

  // Reload exit code, it may be changed by `emit('exit')`
  return process_object->Get(env->context(), exit_code)
      .ToLocalChecked()
      ->Int32Value(env->context())
      .ToChecked();
}

void AddEnvironmentCleanupHook(Isolate* isolate,
                               void (*fun)(void* arg),
                               void* arg) {
  Environment* env = Environment::GetCurrent(isolate);
  CHECK_NOT_NULL(env);
  env->AddCleanupHook(fun, arg);
}

void RemoveEnvironmentCleanupHook(Isolate* isolate,
                                  void (*fun)(void* arg),
                                  void* arg) {
  Environment* env = Environment::GetCurrent(isolate);
  CHECK_NOT_NULL(env);
  env->RemoveCleanupHook(fun, arg);
}

async_id AsyncHooksGetExecutionAsyncId(Isolate* isolate) {
  Environment* env = Environment::GetCurrent(isolate);
  if (env == nullptr) return -1;
  return env->execution_async_id();
}

async_id AsyncHooksGetTriggerAsyncId(Isolate* isolate) {
  Environment* env = Environment::GetCurrent(isolate);
  if (env == nullptr) return -1;
  return env->trigger_async_id();
}


async_context EmitAsyncInit(Isolate* isolate,
                            Local<Object> resource,
                            const char* name,
                            async_id trigger_async_id) {
  HandleScope handle_scope(isolate);
  Local<String> type =
      String::NewFromUtf8(isolate, name, NewStringType::kInternalized)
          .ToLocalChecked();
  return EmitAsyncInit(isolate, resource, type, trigger_async_id);
}

async_context EmitAsyncInit(Isolate* isolate,
                            Local<Object> resource,
                            Local<String> name,
                            async_id trigger_async_id) {
  DebugSealHandleScope handle_scope(isolate);
  Environment* env = Environment::GetCurrent(isolate);
  CHECK_NOT_NULL(env);

  // Initialize async context struct
  if (trigger_async_id == -1)
    trigger_async_id = env->get_default_trigger_async_id();

  async_context context = {
    env->new_async_id(),  // async_id_
    trigger_async_id  // trigger_async_id_
  };

  // Run init hooks
  AsyncWrap::EmitAsyncInit(env, resource, name, context.async_id,
                           context.trigger_async_id);

  return context;
}

void EmitAsyncDestroy(Isolate* isolate, async_context asyncContext) {
  EmitAsyncDestroy(Environment::GetCurrent(isolate), asyncContext);
}

void EmitAsyncDestroy(Environment* env, async_context asyncContext) {
  AsyncWrap::EmitDestroy(env, asyncContext.async_id);
}

}  // namespace node
