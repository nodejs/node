#include "node.h"
#include "env.h"
#include "env-inl.h"
#include "util.h"
#include "util-inl.h"
#include "v8.h"

namespace node {

using v8::ArrayBuffer;
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
using v8::Uint32Array;
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

void GetHeapStatistics(const FunctionCallbackInfo<Value>& args) {
  Isolate* isolate = args.GetIsolate();
  Environment* env = Environment::GetCurrent(isolate);
  HeapStatistics s;
  isolate->GetHeapStatistics(&s);

  uint32_t* data =
      static_cast<uint32_t*>(env->heap_stats_buffer());

  CHECK_NE(data, nullptr);

#define V(i, name, _)                                                         \
  data[i] = static_cast<uint32_t>(s.name());

  HEAP_STATISTICS_PROPERTIES(V)
#undef V
}

void GetHeapStatisticsBuffer(const FunctionCallbackInfo<Value>& args) {
  Isolate* isolate = args.GetIsolate();
  Environment* env = Environment::GetCurrent(isolate);

  void* buffer = env->heap_stats_buffer();

  if (buffer == nullptr) {
    buffer = malloc(kHeapStatisticsBufferLength * sizeof(uint32_t));
    env->set_heap_stats_buffer(buffer);
  }

  Local<ArrayBuffer> ab = ArrayBuffer::New(isolate,
      buffer, kHeapStatisticsBufferLength * sizeof(uint32_t));

  Local<Uint32Array> array =
      Uint32Array::New(ab, 0, kHeapStatisticsBufferLength);

  args.GetReturnValue().Set(array);
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


void InitializeV8Bindings(Handle<Object> target,
                          Handle<Value> unused,
                          Handle<Context> context) {
  Environment* env = Environment::GetCurrent(context);
  env->SetMethod(target, "getHeapStatistics", GetHeapStatistics);
  env->SetMethod(target, "getHeapStatisticsBuffer", GetHeapStatisticsBuffer);
  env->SetMethod(target, "setFlagsFromString", SetFlagsFromString);

#define V(i, _, name)                                                         \
  target->Set(FIXED_ONE_BYTE_STRING(env->isolate(), #name),                   \
              Uint32::NewFromUnsigned(env->isolate(), i));

  HEAP_STATISTICS_PROPERTIES(V)
#undef V
}

}  // namespace node

NODE_MODULE_CONTEXT_AWARE_BUILTIN(v8, node::InitializeV8Bindings)
