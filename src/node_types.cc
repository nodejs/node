#include "env-inl.h"
#include "node.h"
#include "node_debug.h"
#include "node_external_reference.h"

using v8::CFunction;
using v8::Context;
using v8::FunctionCallbackInfo;
using v8::Local;
using v8::Object;
using v8::Value;

namespace node {
namespace {

#define VALUE_METHOD_MAP(V)                                                    \
  V(External)                                                                  \
  V(Date)                                                                      \
  V(ArgumentsObject)                                                           \
  V(BigIntObject)                                                              \
  V(BooleanObject)                                                             \
  V(NumberObject)                                                              \
  V(StringObject)                                                              \
  V(SymbolObject)                                                              \
  V(NativeError)                                                               \
  V(RegExp)                                                                    \
  V(AsyncFunction)                                                             \
  V(GeneratorFunction)                                                         \
  V(GeneratorObject)                                                           \
  V(Promise)                                                                   \
  V(Map)                                                                       \
  V(Set)                                                                       \
  V(MapIterator)                                                               \
  V(SetIterator)                                                               \
  V(WeakMap)                                                                   \
  V(WeakSet)                                                                   \
  V(ArrayBuffer)                                                               \
  V(DataView)                                                                  \
  V(SharedArrayBuffer)                                                         \
  V(Proxy)                                                                     \
  V(ModuleNamespaceObject)

#define V(type)                                                                \
  static void Is##type(const FunctionCallbackInfo<Value>& args) {              \
    args.GetReturnValue().Set(args[0]->Is##type());                            \
  }                                                                            \
  static bool Is##type##FastApi(Local<Value> unused, Local<Value> value) {     \
    TRACK_V8_FAST_API_CALL("types.is" #type);                                  \
    return value->Is##type();                                                  \
  }                                                                            \
  static CFunction fast_is_##type##_ = CFunction::Make(Is##type##FastApi);

VALUE_METHOD_MAP(V)
#undef V

static void IsAnyArrayBuffer(const FunctionCallbackInfo<Value>& args) {
  args.GetReturnValue().Set(
    args[0]->IsArrayBuffer() || args[0]->IsSharedArrayBuffer());
}

static bool IsAnyArrayBufferFastApi(Local<Value> unused, Local<Value> value) {
  TRACK_V8_FAST_API_CALL("types.isAnyArrayBuffer");
  return value->IsArrayBuffer() || value->IsSharedArrayBuffer();
}

static CFunction fast_is_any_array_buffer_ =
    CFunction::Make(IsAnyArrayBufferFastApi);

static void IsBoxedPrimitive(const FunctionCallbackInfo<Value>& args) {
  args.GetReturnValue().Set(
    args[0]->IsNumberObject() ||
    args[0]->IsStringObject() ||
    args[0]->IsBooleanObject() ||
    args[0]->IsBigIntObject() ||
    args[0]->IsSymbolObject());
}

static bool IsBoxedPrimitiveFastApi(Local<Value> unused, Local<Value> value) {
  TRACK_V8_FAST_API_CALL("types.isBoxedPrimitive");
  return value->IsNumberObject() || value->IsStringObject() ||
         value->IsBooleanObject() || value->IsBigIntObject() ||
         value->IsSymbolObject();
}

static CFunction fast_is_boxed_primitive_ =
    CFunction::Make(IsBoxedPrimitiveFastApi);

void InitializeTypes(Local<Object> target,
                     Local<Value> unused,
                     Local<Context> context,
                     void* priv) {
#define V(type)                                                                \
  SetFastMethodNoSideEffect(                                                   \
      context, target, "is" #type, Is##type, &fast_is_##type##_);

  VALUE_METHOD_MAP(V)
#undef V

  SetFastMethodNoSideEffect(context,
                            target,
                            "isAnyArrayBuffer",
                            IsAnyArrayBuffer,
                            &fast_is_any_array_buffer_);
  SetFastMethodNoSideEffect(context,
                            target,
                            "isBoxedPrimitive",
                            IsBoxedPrimitive,
                            &fast_is_boxed_primitive_);
}

}  // anonymous namespace

void RegisterTypesExternalReferences(ExternalReferenceRegistry* registry) {
#define V(type)                                                                \
  registry->Register(Is##type);                                                \
  registry->Register(Is##type##FastApi);                                       \
  registry->Register(fast_is_##type##_.GetTypeInfo());

  VALUE_METHOD_MAP(V)
#undef V

  registry->Register(IsAnyArrayBuffer);
  registry->Register(IsAnyArrayBufferFastApi);
  registry->Register(fast_is_any_array_buffer_.GetTypeInfo());
  registry->Register(IsBoxedPrimitive);
  registry->Register(IsBoxedPrimitiveFastApi);
  registry->Register(fast_is_boxed_primitive_.GetTypeInfo());
}
}  // namespace node

NODE_BINDING_CONTEXT_AWARE_INTERNAL(types, node::InitializeTypes)
NODE_BINDING_EXTERNAL_REFERENCE(types, node::RegisterTypesExternalReferences)
