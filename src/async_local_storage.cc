#include "async_local_storage.h"

#include <unordered_set>

#include "env-inl.h"
#include "memory_tracker-inl.h"
#include "node_external_reference.h"
#include "util-inl.h"

#include "v8.h"

namespace node {

using v8::Context;
using v8::Function;
using v8::FunctionCallbackInfo;
using v8::FunctionTemplate;
using v8::Isolate;
using v8::Local;
using v8::MaybeLocal;
using v8::Object;
using v8::String;
using v8::Symbol;
using v8::Value;

AsyncLocalStorageState::AsyncLocalStorageState(Environment* env,
                                               Local<Object> wrap)
    : BaseObject(env, wrap),
      state(ContextStorageState::State()) {
  MakeWeak();
}

BaseObjectPtr<AsyncLocalStorageState> AsyncLocalStorageState::Create(
  Environment* env) {
  Local<Object> obj;

  if (!GetConstructorTemplate(env)
           ->InstanceTemplate()
           ->NewInstance(env->context())
           .ToLocal(&obj)) {
    return BaseObjectPtr<AsyncLocalStorageState>();
  }

  return MakeBaseObject<AsyncLocalStorageState>(env, obj);
}

Local<FunctionTemplate> AsyncLocalStorageState::GetConstructorTemplate(
    Environment* env) {
  Local<FunctionTemplate> tmpl = env->async_local_storage_state_ctor_template();
  if (tmpl.IsEmpty()) {
    tmpl = env->NewFunctionTemplate(nullptr);
    Local<String> classname =
        FIXED_ONE_BYTE_STRING(env->isolate(), "AsyncLocalStorageState");
    tmpl->SetClassName(classname);
    tmpl->Inherit(BaseObject::GetConstructorTemplate(env));

    tmpl->InstanceTemplate()->SetInternalFieldCount(
        AsyncLocalStorageState::kInternalFieldCount);
    env->SetProtoMethod(tmpl, "restore", AsyncLocalStorageState::Restore);
    env->SetProtoMethod(tmpl, "clear", AsyncLocalStorageState::Clear);

    env->set_async_local_storage_state_ctor_template(tmpl);
  }
  return tmpl;
}

bool AsyncLocalStorageState::HasInstance(Environment* env,
                                         const Local<Value>& object) {
  return GetConstructorTemplate(env)->HasInstance(object);
}

void AsyncLocalStorageState::RegisterExternalReferences(
    ExternalReferenceRegistry* registry) {
  registry->Register(AsyncLocalStorageState::Restore);
  registry->Register(AsyncLocalStorageState::Clear);
}

void AsyncLocalStorageState::MemoryInfo(MemoryTracker* tracker) const {
  tracker->TrackFieldWithSize("state", sizeof(state));
}

void AsyncLocalStorageState::Restore(const FunctionCallbackInfo<Value>& args) {
  AsyncLocalStorageState* state;
  ASSIGN_OR_RETURN_UNWRAP(&state, args.Holder());
  state->state->Restore();
}

void AsyncLocalStorageState::Clear(const FunctionCallbackInfo<Value>& args) {
  AsyncLocalStorageState* state;
  ASSIGN_OR_RETURN_UNWRAP(&state, args.Holder());
  state->state->Clear();
}

AsyncLocalStorage::AsyncLocalStorage(Environment* env, Local<Object> object)
    : BaseObject(env, object),
      storage_(ContextStorage::Create(env->isolate())) {}

void AsyncLocalStorage::Initialize(Local<Object> target,
                                          Local<Value> unused,
                                          Local<Context> context,
                                          void* priv) {
  Environment* env = Environment::GetCurrent(context);
  Local<FunctionTemplate> t = AsyncLocalStorage::GetConstructorTemplate(env);
  env->SetConstructorFunction(target, "AsyncLocalStorage", t);
}

Local<FunctionTemplate> AsyncLocalStorage::GetConstructorTemplate(
  Environment* env) {
  Local<FunctionTemplate> tmpl = env->async_local_storage_ctor_template();
  if (tmpl.IsEmpty()) {
    tmpl = env->NewFunctionTemplate(New);
    tmpl->SetClassName(
        FIXED_ONE_BYTE_STRING(env->isolate(), "AsyncLocalStorage"));
    tmpl->InstanceTemplate()->SetInternalFieldCount(
        BaseObject::kInternalFieldCount);
    tmpl->Inherit(BaseObject::GetConstructorTemplate(env));
    env->SetProtoMethod(tmpl, "getStore", AsyncLocalStorage::GetStore);
    env->SetProtoMethod(tmpl, "enterWith", AsyncLocalStorage::EnterWith);
    env->SetProtoMethod(tmpl, "exit", AsyncLocalStorage::Exit);
    env->SetProtoMethod(tmpl, "run", AsyncLocalStorage::Run);
    env->set_async_local_storage_ctor_template(tmpl);
  }
  return tmpl;
}

void AsyncLocalStorage::RegisterExternalReferences(
    ExternalReferenceRegistry* registry) {
  registry->Register(AsyncLocalStorage::EnterWith);
  registry->Register(AsyncLocalStorage::Exit);
  registry->Register(AsyncLocalStorage::GetStore);
  registry->Register(AsyncLocalStorage::New);
  registry->Register(AsyncLocalStorage::Run);
}

void AsyncLocalStorage::MemoryInfo(MemoryTracker* tracker) const {
  tracker->TrackFieldWithSize("storage", sizeof(storage_));
}

void AsyncLocalStorage::New(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  CHECK(args.IsConstructCall());
  new AsyncLocalStorage(env, args.Holder());
}

void AsyncLocalStorage::GetStore(const FunctionCallbackInfo<Value>& args) {
  AsyncLocalStorage* als;
  ASSIGN_OR_RETURN_UNWRAP(&als, args.Holder());
  args.GetReturnValue().Set(als->storage_->Get());
}

void AsyncLocalStorage::EnterWith(const FunctionCallbackInfo<Value>& args) {
  AsyncLocalStorage* als;
  ASSIGN_OR_RETURN_UNWRAP(&als, args.Holder());
  std::shared_ptr<ContextStorage> storage = als->storage_;
  storage->Enable();
  storage->Enter(args[0]);
}

MaybeLocal<Value> RunFn(Local<Context> context,
                        Local<Function> fn,
                        const FunctionCallbackInfo<Value>& args,
                        int offset) {
  int argc = args.Length() - offset;
  if (argc < 1) {
    return fn->Call(context, args.This(), 0, {});
  }

  MaybeStackBuffer<Local<Value>, 16> argv(argc);
  for (int i = 0; i < argc; i++) {
    argv[i] = args[i + offset];
  }

  return fn->Call(context, args.This(), argc, &argv[0]);
}

void AsyncLocalStorage::Exit(const FunctionCallbackInfo<Value>& args) {
  CHECK(args[0]->IsFunction());

  AsyncLocalStorage* storage;
  ASSIGN_OR_RETURN_UNWRAP(&storage, args.Holder());

  ContextStorageExitScope scope(storage->storage_);

  Local<Context> context = storage->env()->context();
  auto fn = args[0].As<Function>();

  args.GetReturnValue().Set(RunFn(context, fn, args, 1).ToLocalChecked());
}

void AsyncLocalStorage::Run(const FunctionCallbackInfo<Value>& args) {
  CHECK(args[1]->IsFunction());

  AsyncLocalStorage* storage;
  ASSIGN_OR_RETURN_UNWRAP(&storage, args.Holder());

  ContextStorageRunScope scope(storage->storage_, args[0]);

  Local<Context> context = storage->env()->context();
  auto fn = args[1].As<Function>();

  v8::Local<v8::Value> result;
  if (RunFn(context, fn, args, 2).ToLocal(&result)) {
    args.GetReturnValue().Set(result);
  }
}

void AsyncLocalStorage::StoreState(v8::Local<v8::Object> resource) {
  Isolate* isolate = resource->GetIsolate();
  Environment* env = Environment::GetCurrent(isolate);

  USE(resource->SetPrivate(isolate->GetCurrentContext(),
                           env->context_storage_state_symbol(),
                           AsyncLocalStorageState::Create(env)->object()));
}

std::shared_ptr<ContextStorageState> AsyncLocalStorage::GetState(
  v8::Local<v8::Object> resource) {
  Isolate* isolate = resource->GetIsolate();
  Environment* env = Environment::GetCurrent(isolate);

  auto maybe_local = resource->GetPrivate(
      isolate->GetCurrentContext(),
      env->context_storage_state_symbol());

  Local<v8::Value> value;
  if (!maybe_local.ToLocal(&value) ||
      !AsyncLocalStorageState::HasInstance(env, value)) {
    // fprintf(stderr, "Error: No AsyncLocalStorageState found\n");
    // DumpBacktrace(stderr);
    return nullptr;
  }

  AsyncLocalStorageState* state = Unwrap<AsyncLocalStorageState>(value);

  return state->state;
}

}  // namespace node

NODE_MODULE_CONTEXT_AWARE_INTERNAL(
    async_local_storage,
    node::AsyncLocalStorage::Initialize)

NODE_MODULE_EXTERNAL_REFERENCE(
    async_local_storage_state,
    node::AsyncLocalStorageState::RegisterExternalReferences)

NODE_MODULE_EXTERNAL_REFERENCE(
    async_local_storage,
    node::AsyncLocalStorage::RegisterExternalReferences)
