#include "node_internals.h"
#include "node_watchdog.h"
#include "base_object-inl.h"

namespace node {
namespace util {

using v8::ALL_PROPERTIES;
using v8::Array;
using v8::Boolean;
using v8::Context;
using v8::FunctionCallbackInfo;
using v8::FunctionTemplate;
using v8::IndexFilter;
using v8::Integer;
using v8::Isolate;
using v8::KeyCollectionMode;
using v8::Local;
using v8::NewStringType;
using v8::Object;
using v8::ONLY_CONFIGURABLE;
using v8::ONLY_ENUMERABLE;
using v8::ONLY_WRITABLE;
using v8::Private;
using v8::Promise;
using v8::PropertyFilter;
using v8::Proxy;
using v8::ReadOnly;
using v8::SKIP_STRINGS;
using v8::SKIP_SYMBOLS;
using v8::String;
using v8::Uint32;
using v8::Value;

static void GetOwnNonIndexProperties(
    const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  Local<Context> context = env->context();

  CHECK(args[0]->IsObject());
  CHECK(args[1]->IsUint32());

  Local<Object> object = args[0].As<Object>();

  Local<Array> properties;

  PropertyFilter filter =
    static_cast<PropertyFilter>(args[1].As<Uint32>()->Value());

  if (!object->GetPropertyNames(
        context, KeyCollectionMode::kOwnOnly,
        filter,
        IndexFilter::kSkipIndices)
          .ToLocal(&properties)) {
    return;
  }
  args.GetReturnValue().Set(properties);
}

static void GetPromiseDetails(const FunctionCallbackInfo<Value>& args) {
  // Return undefined if it's not a Promise.
  if (!args[0]->IsPromise())
    return;

  auto isolate = args.GetIsolate();

  Local<Promise> promise = args[0].As<Promise>();
  Local<Array> ret = Array::New(isolate, 2);

  int state = promise->State();
  ret->Set(0, Integer::New(isolate, state));
  if (state != Promise::PromiseState::kPending)
    ret->Set(1, promise->Result());

  args.GetReturnValue().Set(ret);
}

static void GetProxyDetails(const FunctionCallbackInfo<Value>& args) {
  // Return undefined if it's not a proxy.
  if (!args[0]->IsProxy())
    return;

  Local<Proxy> proxy = args[0].As<Proxy>();

  Local<Array> ret = Array::New(args.GetIsolate(), 2);
  ret->Set(0, proxy->GetTarget());
  ret->Set(1, proxy->GetHandler());

  args.GetReturnValue().Set(ret);
}

static void PreviewEntries(const FunctionCallbackInfo<Value>& args) {
  if (!args[0]->IsObject())
    return;

  Environment* env = Environment::GetCurrent(args);
  bool is_key_value;
  Local<Array> entries;
  if (!args[0].As<Object>()->PreviewEntries(&is_key_value).ToLocal(&entries))
    return;
  // Fast path for WeakMap, WeakSet and Set iterators.
  if (args.Length() == 1)
    return args.GetReturnValue().Set(entries);
  Local<Array> ret = Array::New(env->isolate(), 2);
  ret->Set(env->context(), 0, entries).FromJust();
  ret->Set(env->context(), 1, Boolean::New(env->isolate(), is_key_value))
      .FromJust();
  return args.GetReturnValue().Set(ret);
}

// Side effect-free stringification that will never throw exceptions.
static void SafeToString(const FunctionCallbackInfo<Value>& args) {
  auto context = args.GetIsolate()->GetCurrentContext();
  args.GetReturnValue().Set(args[0]->ToDetailString(context).ToLocalChecked());
}

inline Local<Private> IndexToPrivateSymbol(Environment* env, uint32_t index) {
#define V(name, _) &Environment::name,
  static Local<Private> (Environment::*const methods[])() const = {
    PER_ISOLATE_PRIVATE_SYMBOL_PROPERTIES(V)
  };
#undef V
  CHECK_LT(index, arraysize(methods));
  return (env->*methods[index])();
}

static void GetHiddenValue(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  CHECK(args[0]->IsObject());
  CHECK(args[1]->IsUint32());

  Local<Object> obj = args[0].As<Object>();
  auto index = args[1]->Uint32Value(env->context()).FromJust();
  auto private_symbol = IndexToPrivateSymbol(env, index);
  auto maybe_value = obj->GetPrivate(env->context(), private_symbol);

  args.GetReturnValue().Set(maybe_value.ToLocalChecked());
}

static void SetHiddenValue(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  CHECK(args[0]->IsObject());
  CHECK(args[1]->IsUint32());

  Local<Object> obj = args[0].As<Object>();
  auto index = args[1]->Uint32Value(env->context()).FromJust();
  auto private_symbol = IndexToPrivateSymbol(env, index);
  auto maybe_value = obj->SetPrivate(env->context(), private_symbol, args[2]);

  args.GetReturnValue().Set(maybe_value.FromJust());
}


void StartSigintWatchdog(const FunctionCallbackInfo<Value>& args) {
  int ret = SigintWatchdogHelper::GetInstance()->Start();
  args.GetReturnValue().Set(ret == 0);
}


void StopSigintWatchdog(const FunctionCallbackInfo<Value>& args) {
  bool had_pending_signals = SigintWatchdogHelper::GetInstance()->Stop();
  args.GetReturnValue().Set(had_pending_signals);
}


void WatchdogHasPendingSigint(const FunctionCallbackInfo<Value>& args) {
  bool ret = SigintWatchdogHelper::GetInstance()->HasPendingSignal();
  args.GetReturnValue().Set(ret);
}

void SafeGetenv(const FunctionCallbackInfo<Value>& args) {
  CHECK(args[0]->IsString());
  Utf8Value strenvtag(args.GetIsolate(), args[0]);
  std::string text;
  if (!node::SafeGetenv(*strenvtag, &text)) return;
  args.GetReturnValue()
      .Set(String::NewFromUtf8(
            args.GetIsolate(), text.c_str(),
            NewStringType::kNormal).ToLocalChecked());
}

class WeakReference : public BaseObject {
 public:
  WeakReference(Environment* env, Local<Object> object, Local<Object> target)
    : BaseObject(env, object) {
    MakeWeak();
    target_.Reset(env->isolate(), target);
    target_.SetWeak();
  }

  static void New(const FunctionCallbackInfo<Value>& args) {
    Environment* env = Environment::GetCurrent(args);
    CHECK(args.IsConstructCall());
    CHECK(args[0]->IsObject());
    new WeakReference(env, args.This(), args[0].As<Object>());
  }

  static void Get(const FunctionCallbackInfo<Value>& args) {
    WeakReference* weak_ref = Unwrap<WeakReference>(args.Holder());
    Isolate* isolate = args.GetIsolate();
    if (!weak_ref->target_.IsEmpty())
      args.GetReturnValue().Set(weak_ref->target_.Get(isolate));
  }

  SET_MEMORY_INFO_NAME(WeakReference)
  SET_SELF_SIZE(WeakReference)
  SET_NO_MEMORY_INFO()

 private:
  Persistent<Object> target_;
};

void Initialize(Local<Object> target,
                Local<Value> unused,
                Local<Context> context) {
  Environment* env = Environment::GetCurrent(context);

#define V(name, _)                                                            \
  target->Set(context,                                                        \
              FIXED_ONE_BYTE_STRING(env->isolate(), #name),                   \
              Integer::NewFromUnsigned(env->isolate(), index++)).FromJust();
  {
    uint32_t index = 0;
    PER_ISOLATE_PRIVATE_SYMBOL_PROPERTIES(V)
  }
#undef V

  target->DefineOwnProperty(
    env->context(),
    OneByteString(env->isolate(), "pushValToArrayMax"),
    Integer::NewFromUnsigned(env->isolate(), NODE_PUSH_VAL_TO_ARRAY_MAX),
    ReadOnly).FromJust();

#define V(name)                                                               \
  target->Set(context,                                                        \
              FIXED_ONE_BYTE_STRING(env->isolate(), #name),                   \
              Integer::New(env->isolate(), Promise::PromiseState::name))      \
    .FromJust()
  V(kPending);
  V(kFulfilled);
  V(kRejected);
#undef V

  env->SetMethodNoSideEffect(target, "getHiddenValue", GetHiddenValue);
  env->SetMethod(target, "setHiddenValue", SetHiddenValue);
  env->SetMethodNoSideEffect(target, "getPromiseDetails", GetPromiseDetails);
  env->SetMethodNoSideEffect(target, "getProxyDetails", GetProxyDetails);
  env->SetMethodNoSideEffect(target, "safeToString", SafeToString);
  env->SetMethodNoSideEffect(target, "previewEntries", PreviewEntries);
  env->SetMethodNoSideEffect(target, "getOwnNonIndexProperties",
                                     GetOwnNonIndexProperties);

  env->SetMethod(target, "startSigintWatchdog", StartSigintWatchdog);
  env->SetMethod(target, "stopSigintWatchdog", StopSigintWatchdog);
  env->SetMethodNoSideEffect(target, "watchdogHasPendingSigint",
                             WatchdogHasPendingSigint);

  env->SetMethod(target, "safeGetenv", SafeGetenv);

  Local<Object> constants = Object::New(env->isolate());
  NODE_DEFINE_CONSTANT(constants, ALL_PROPERTIES);
  NODE_DEFINE_CONSTANT(constants, ONLY_WRITABLE);
  NODE_DEFINE_CONSTANT(constants, ONLY_ENUMERABLE);
  NODE_DEFINE_CONSTANT(constants, ONLY_CONFIGURABLE);
  NODE_DEFINE_CONSTANT(constants, SKIP_STRINGS);
  NODE_DEFINE_CONSTANT(constants, SKIP_SYMBOLS);
  target->Set(context,
              FIXED_ONE_BYTE_STRING(env->isolate(), "propertyFilter"),
              constants).FromJust();

  Local<String> weak_ref_string =
      FIXED_ONE_BYTE_STRING(env->isolate(), "WeakReference");
  Local<FunctionTemplate> weak_ref =
      env->NewFunctionTemplate(WeakReference::New);
  weak_ref->InstanceTemplate()->SetInternalFieldCount(1);
  weak_ref->SetClassName(weak_ref_string);
  env->SetProtoMethod(weak_ref, "get", WeakReference::Get);
  target->Set(context, weak_ref_string,
              weak_ref->GetFunction(context).ToLocalChecked()).FromJust();
}

}  // namespace util
}  // namespace node

NODE_BUILTIN_MODULE_CONTEXT_AWARE(util, node::util::Initialize)
