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
  V(isTypedArray, IsTypedArray)


#define V(_, ucname) \
  static void ucname(const FunctionCallbackInfo<Value>& args) {               \
    CHECK_EQ(1, args.Length());                                               \
    args.GetReturnValue().Set(args[0]->ucname());                             \
  }

  VALUE_METHOD_MAP(V)
#undef V


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

#define V(lcname, ucname) env->SetMethod(target, #lcname, ucname);
  VALUE_METHOD_MAP(V)
#undef V

  env->SetMethod(target, "getHiddenValue", GetHiddenValue);
}

}  // namespace util
}  // namespace node

NODE_MODULE_CONTEXT_AWARE_BUILTIN(util, node::util::Initialize)
