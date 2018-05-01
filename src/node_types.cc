#include "node_internals.h"

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

void InitializeTypes(Local<Object> target,
                     Local<Value> unused,
                     Local<Context> context) {
  Environment* env = Environment::GetCurrent(context);

#define V(type) env->SetMethod(target,     \
                               "is" #type, \
                               Is##type);
  VALUE_METHOD_MAP(V)
#undef V

  env->SetMethod(target, "isAnyArrayBuffer", IsAnyArrayBuffer);
}

}  // anonymous namespace
}  // namespace node

NODE_MODULE_CONTEXT_AWARE_INTERNAL(types, node::InitializeTypes)
