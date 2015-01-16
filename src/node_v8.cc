#include "node.h"
#include "env.h"
#include "env-inl.h"
#include "util.h"
#include "util-inl.h"
#include "v8.h"

namespace node {

using v8::Context;
using v8::ExternalArrayType;
using v8::Function;
using v8::FunctionCallbackInfo;
using v8::Handle;
using v8::HeapStatistics;
using v8::Isolate;
using v8::Local;
using v8::Object;
using v8::String;
using v8::Uint32;
using v8::V8;
using v8::Value;

#define HEAP_STATISTICS_PROPERTIES(V)                                         \
  V(0, total_heap_size, kTotalHeapSizeIndex)                                  \
  V(1, total_heap_size_executable, kTotalHeapSizeExecutableIndex)             \
  V(2, total_physical_size, kTotalPhysicalSizeIndex)                          \
  V(3, used_heap_size, kUsedHeapSizeIndex)                                    \
  V(4, heap_size_limit, kHeapSizeLimitIndex)

#define V(a, b, c) +1
static const size_t kHeapStatisticsBufferLength = HEAP_STATISTICS_PROPERTIES(V);
#undef V

static const ExternalArrayType kHeapStatisticsBufferType =
    v8::kExternalUint32Array;

void GetHeapStatistics(const FunctionCallbackInfo<Value>& args) {
  CHECK(args.Length() == 1 && args[0]->IsObject());

  Isolate* isolate = args.GetIsolate();
  HeapStatistics s;
  isolate->GetHeapStatistics(&s);
  Local<Object> obj = args[0].As<Object>();
  uint32_t* data =
      static_cast<uint32_t*>(obj->GetIndexedPropertiesExternalArrayData());

  CHECK_NE(data, nullptr);
  ASSERT_EQ(obj->GetIndexedPropertiesExternalArrayDataType(),
            kHeapStatisticsBufferType);
  ASSERT_EQ(obj->GetIndexedPropertiesExternalArrayDataLength(),
            kHeapStatisticsBufferLength);

#define V(i, name, _)                                                         \
  data[i] = static_cast<uint32_t>(s.name());

  HEAP_STATISTICS_PROPERTIES(V)
#undef V
}


void SetFlagsFromString(const FunctionCallbackInfo<Value>& args) {
  String::Utf8Value flags(args[0]);
  V8::SetFlagsFromString(*flags, flags.length());
}


void InitializeV8Bindings(Handle<Object> target,
                          Handle<Value> unused,
                          Handle<Context> context) {
  Environment* env = Environment::GetCurrent(context);
  env->SetMethod(target, "getHeapStatistics", GetHeapStatistics);
  env->SetMethod(target, "setFlagsFromString", SetFlagsFromString);

  target->Set(FIXED_ONE_BYTE_STRING(env->isolate(),
                                    "kHeapStatisticsBufferLength"),
              Uint32::NewFromUnsigned(env->isolate(),
                                      kHeapStatisticsBufferLength));

  target->Set(FIXED_ONE_BYTE_STRING(env->isolate(),
                                    "kHeapStatisticsBufferType"),
              Uint32::NewFromUnsigned(env->isolate(),
                                      kHeapStatisticsBufferType));

#define V(i, _, name)                                                         \
  target->Set(FIXED_ONE_BYTE_STRING(env->isolate(), #name),                   \
              Uint32::NewFromUnsigned(env->isolate(), i));

  HEAP_STATISTICS_PROPERTIES(V)
#undef V
}

}  // namespace node

NODE_MODULE_CONTEXT_AWARE_BUILTIN(v8, node::InitializeV8Bindings)
