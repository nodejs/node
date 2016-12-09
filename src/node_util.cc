#include "node.h"
#include "node_watchdog.h"
#include "v8.h"
#include "env.h"
#include "env-inl.h"

namespace node {
namespace util {

using v8::Array;
using v8::Context;
using v8::FunctionCallbackInfo;
using v8::Integer;
using v8::Local;
using v8::Object;
using v8::Private;
using v8::Proxy;
using v8::Value;


#define VALUE_METHOD_MAP(V)                                                   \
  V(isArrayBuffer, IsArrayBuffer)                                             \
  V(isDataView, IsDataView)                                                   \
  V(isDate, IsDate)                                                           \
  V(isMap, IsMap)                                                             \
  V(isMapIterator, IsMapIterator)                                             \
  V(isPromise, IsPromise)                                                     \
  V(isRegExp, IsRegExp)                                                       \
  V(isSet, IsSet)                                                             \
  V(isSetIterator, IsSetIterator)                                             \
  V(isSharedArrayBuffer, IsSharedArrayBuffer)                                 \
  V(isTypedArray, IsTypedArray)


#define V(_, ucname) \
  static void ucname(const FunctionCallbackInfo<Value>& args) {               \
    CHECK_EQ(1, args.Length());                                               \
    args.GetReturnValue().Set(args[0]->ucname());                             \
  }

  VALUE_METHOD_MAP(V)
#undef V

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

  if (!args[0]->IsObject())
    return env->ThrowTypeError("obj must be an object");

  if (!args[1]->IsUint32())
    return env->ThrowTypeError("index must be an uint32");

  Local<Object> obj = args[0].As<Object>();
  auto index = args[1]->Uint32Value(env->context()).FromJust();
  auto private_symbol = IndexToPrivateSymbol(env, index);
  auto maybe_value = obj->GetPrivate(env->context(), private_symbol);

  args.GetReturnValue().Set(maybe_value.ToLocalChecked());
}

static void SetHiddenValue(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  if (!args[0]->IsObject())
    return env->ThrowTypeError("obj must be an object");

  if (!args[1]->IsUint32())
    return env->ThrowTypeError("index must be an uint32");

  Local<Object> obj = args[0].As<Object>();
  auto index = args[1]->Uint32Value(env->context()).FromJust();
  auto private_symbol = IndexToPrivateSymbol(env, index);
  auto maybe_value = obj->SetPrivate(env->context(), private_symbol, args[2]);

  args.GetReturnValue().Set(maybe_value.FromJust());
}


void StartSigintWatchdog(const FunctionCallbackInfo<Value>& args) {
  int ret = SigintWatchdogHelper::GetInstance()->Start();
  if (ret != 0) {
    Environment* env = Environment::GetCurrent(args);
    env->ThrowErrnoException(ret, "StartSigintWatchdog");
  }
}


void StopSigintWatchdog(const FunctionCallbackInfo<Value>& args) {
  bool had_pending_signals = SigintWatchdogHelper::GetInstance()->Stop();
  args.GetReturnValue().Set(had_pending_signals);
}


void WatchdogHasPendingSigint(const FunctionCallbackInfo<Value>& args) {
  bool ret = SigintWatchdogHelper::GetInstance()->HasPendingSignal();
  args.GetReturnValue().Set(ret);
}


void Initialize(Local<Object> target,
                Local<Value> unused,
                Local<Context> context) {
  Environment* env = Environment::GetCurrent(context);

#define V(lcname, ucname) env->SetMethod(target, #lcname, ucname);
  VALUE_METHOD_MAP(V)
#undef V

#define V(name, _)                                                            \
  target->Set(context,                                                        \
              FIXED_ONE_BYTE_STRING(env->isolate(), #name),                   \
              Integer::NewFromUnsigned(env->isolate(), index++)).FromJust();
  {
    uint32_t index = 0;
    PER_ISOLATE_PRIVATE_SYMBOL_PROPERTIES(V)
  }
#undef V

  env->SetMethod(target, "getHiddenValue", GetHiddenValue);
  env->SetMethod(target, "setHiddenValue", SetHiddenValue);
  env->SetMethod(target, "getProxyDetails", GetProxyDetails);

  env->SetMethod(target, "startSigintWatchdog", StartSigintWatchdog);
  env->SetMethod(target, "stopSigintWatchdog", StopSigintWatchdog);
  env->SetMethod(target, "watchdogHasPendingSigint", WatchdogHasPendingSigint);
}

}  // namespace util
}  // namespace node

NODE_MODULE_CONTEXT_AWARE_BUILTIN(util, node::util::Initialize)
