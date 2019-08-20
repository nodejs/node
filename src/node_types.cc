#include "env-inl.h"
#include "node.h"

using v8::Context;
using v8::FunctionCallbackInfo;
using v8::Local;
using v8::Object;
using v8::Value;

namespace node {
namespace {

#define VALUE_METHOD_MAP(V)                                                   \
  V(External)                                                                 \
  V(Date)                                                                     \
  V(ArgumentsObject)                                                          \
  V(BigIntObject)                                                             \
  V(BooleanObject)                                                            \
  V(NumberObject)                                                             \
  V(StringObject)                                                             \
  V(SymbolObject)                                                             \
  V(NativeError)                                                              \
  V(RegExp)                                                                   \
  V(AsyncFunction)                                                            \
  V(GeneratorFunction)                                                        \
  V(GeneratorObject)                                                          \
  V(Promise)                                                                  \
  V(Map)                                                                      \
  V(Set)                                                                      \
  V(MapIterator)                                                              \
  V(SetIterator)                                                              \
  V(WeakMap)                                                                  \
  V(WeakSet)                                                                  \
  V(ArrayBuffer)                                                              \
  V(DataView)                                                                 \
  V(SharedArrayBuffer)                                                        \
  V(Proxy)                                                                    \
  V(WebAssemblyCompiledModule)                                                \
  V(ModuleNamespaceObject)                                                    \


#define V(type) \
  static void Is##type(const FunctionCallbackInfo<Value>& args) {             \
    args.GetReturnValue().Set(args[0]->Is##type());                           \
  }

  VALUE_METHOD_MAP(V)
#undef V

static void IsAnyArrayBuffer(const FunctionCallbackInfo<Value>& args) {
  args.GetReturnValue().Set(
    args[0]->IsArrayBuffer() || args[0]->IsSharedArrayBuffer());
}

static void IsBoxedPrimitive(const FunctionCallbackInfo<Value>& args) {
  args.GetReturnValue().Set(
    args[0]->IsNumberObject() ||
    args[0]->IsStringObject() ||
    args[0]->IsBooleanObject() ||
    args[0]->IsBigIntObject() ||
    args[0]->IsSymbolObject());
}

void InitializeTypes(Local<Object> target,
                     Local<Value> unused,
                     Local<Context> context,
                     void* priv) {
  Environment* env = Environment::GetCurrent(context);

#define V(type) env->SetMethodNoSideEffect(target,     \
                                           "is" #type, \
                                           Is##type);
  VALUE_METHOD_MAP(V)
#undef V

  env->SetMethodNoSideEffect(target, "isAnyArrayBuffer", IsAnyArrayBuffer);
  env->SetMethodNoSideEffect(target, "isBoxedPrimitive", IsBoxedPrimitive);
}

}  // anonymous namespace
}  // namespace node

NODE_MODULE_CONTEXT_AWARE_INTERNAL(types, node::InitializeTypes)
