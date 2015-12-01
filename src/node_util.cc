#include "node.h"
#include "v8.h"
#include "env.h"
#include "env-inl.h"

namespace node {
namespace util {

using v8::Context;
using v8::FunctionCallbackInfo;
using v8::Local;
using v8::Object;
using v8::String;
using v8::Value;


static void IsRegExp(const FunctionCallbackInfo<Value>& args) {
  CHECK_EQ(1, args.Length());
  args.GetReturnValue().Set(args[0]->IsRegExp());
}


static void IsDate(const FunctionCallbackInfo<Value>& args) {
  CHECK_EQ(1, args.Length());
  args.GetReturnValue().Set(args[0]->IsDate());
}


static void IsMap(const FunctionCallbackInfo<Value>& args) {
  CHECK_EQ(1, args.Length());
  args.GetReturnValue().Set(args[0]->IsMap());
}


static void IsMapIterator(const FunctionCallbackInfo<Value>& args) {
  CHECK_EQ(1, args.Length());
  args.GetReturnValue().Set(args[0]->IsMapIterator());
}


static void IsSet(const FunctionCallbackInfo<Value>& args) {
  CHECK_EQ(1, args.Length());
  args.GetReturnValue().Set(args[0]->IsSet());
}


static void IsSetIterator(const FunctionCallbackInfo<Value>& args) {
  CHECK_EQ(1, args.Length());
  args.GetReturnValue().Set(args[0]->IsSetIterator());
}

static void IsPromise(const FunctionCallbackInfo<Value>& args) {
  CHECK_EQ(1, args.Length());
  args.GetReturnValue().Set(args[0]->IsPromise());
}


static void GetHiddenValue(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  if (!args[0]->IsObject())
    return env->ThrowTypeError("obj must be an object");

  if (!args[1]->IsString())
    return env->ThrowTypeError("name must be a string");

  Local<Object> obj = args[0].As<Object>();
  Local<String> name = args[1].As<String>();

  args.GetReturnValue().Set(obj->GetHiddenValue(name));
}


void Initialize(Local<Object> target,
                Local<Value> unused,
                Local<Context> context) {
  Environment* env = Environment::GetCurrent(context);
  env->SetMethod(target, "isRegExp", IsRegExp);
  env->SetMethod(target, "isDate", IsDate);
  env->SetMethod(target, "isMap", IsMap);
  env->SetMethod(target, "isMapIterator", IsMapIterator);
  env->SetMethod(target, "isSet", IsSet);
  env->SetMethod(target, "isSetIterator", IsSetIterator);
  env->SetMethod(target, "isPromise", IsPromise);
  env->SetMethod(target, "getHiddenValue", GetHiddenValue);
}

}  // namespace util
}  // namespace node

NODE_MODULE_CONTEXT_AWARE_BUILTIN(util, node::util::Initialize)
