#include "env-inl.h"
#include "node.h"
#include "node_external_reference.h"

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
#define V(type) SetMethodNoSideEffect(context, target, "is" #type, Is##type);
  VALUE_METHOD_MAP(V)
#undef V

  SetMethodNoSideEffect(context, target, "isAnyArrayBuffer", IsAnyArrayBuffer);
  SetMethodNoSideEffect(context, target, "isBoxedPrimitive", IsBoxedPrimitive);
}

}  // anonymous namespace

void RegisterTypesExternalReferences(ExternalReferenceRegistry* registry) {
#define V(type) registry->Register(Is##type);
  VALUE_METHOD_MAP(V)
#undef V

  registry->Register(IsAnyArrayBuffer);
  registry->Register(IsBoxedPrimitive);
}
}  // namespace node

NODE_BINDING_CONTEXT_AWARE_INTERNAL(types, node::InitializeTypes)
NODE_BINDING_EXTERNAL_REFERENCE(types, node::RegisterTypesExternalReferences)
