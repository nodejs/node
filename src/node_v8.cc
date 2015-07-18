#include "node.h"
#include "env.h"
#include "env-inl.h"
#include "util.h"
#include "util-inl.h"
#include "v8.h"

namespace node {

using v8::ArrayBuffer;
using v8::Context;
using v8::Function;
using v8::FunctionCallbackInfo;
using v8::HeapStatistics;
using v8::Isolate;
using v8::Local;
using v8::Object;
using v8::String;
using v8::Uint32;
using v8::Uint32Array;
using v8::V8;
using v8::Value;

#define HEAP_STATISTICS_PROPERTIES(V)                                         \
  V(0, total_heap_size, kTotalHeapSizeIndex)                                  \
  V(1, total_heap_size_executable, kTotalHeapSizeExecutableIndex)             \
  V(2, total_physical_size, kTotalPhysicalSizeIndex)                          \
  V(3, total_available_size, kTotalAvailableSize)                             \
  V(4, used_heap_size, kUsedHeapSizeIndex)                                    \
  V(5, heap_size_limit, kHeapSizeLimitIndex)

#define V(a, b, c) +1
static const size_t kHeapStatisticsPropertiesCount =
    HEAP_STATISTICS_PROPERTIES(V);
#undef V

void UpdateHeapStatisticsArrayBuffer(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  HeapStatistics s;
  env->isolate()->GetHeapStatistics(&s);
  uint32_t* const buffer = env->heap_statistics_buffer();
#define V(index, name, _) buffer[index] = static_cast<uint32_t>(s.name());
  HEAP_STATISTICS_PROPERTIES(V)
#undef V
}


void SetFlagsFromString(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  if (args.Length() < 1)
    return env->ThrowTypeError("v8 flag is required");
  if (!args[0]->IsString())
    return env->ThrowTypeError("v8 flag must be a string");

  String::Utf8Value flags(args[0]);
  V8::SetFlagsFromString(*flags, flags.length());
}


void InitializeV8Bindings(Local<Object> target,
                          Local<Value> unused,
                          Local<Context> context) {
  Environment* env = Environment::GetCurrent(context);
  env->SetMethod(target,
                 "updateHeapStatisticsArrayBuffer",
                 UpdateHeapStatisticsArrayBuffer);
  env->SetMethod(target, "setFlagsFromString", SetFlagsFromString);

  env->set_heap_statistics_buffer(new uint32_t[kHeapStatisticsPropertiesCount]);

  const size_t heap_statistics_buffer_byte_length =
      sizeof(*env->heap_statistics_buffer()) * kHeapStatisticsPropertiesCount;

  target->Set(FIXED_ONE_BYTE_STRING(env->isolate(),
                                    "heapStatisticsArrayBuffer"),
              ArrayBuffer::New(env->isolate(),
                               env->heap_statistics_buffer(),
                               heap_statistics_buffer_byte_length));

#define V(i, _, name)                                                         \
  target->Set(FIXED_ONE_BYTE_STRING(env->isolate(), #name),                   \
              Uint32::NewFromUnsigned(env->isolate(), i));

  HEAP_STATISTICS_PROPERTIES(V)
#undef V
}

}  // namespace node

NODE_MODULE_CONTEXT_AWARE_BUILTIN(v8, node::InitializeV8Bindings)
