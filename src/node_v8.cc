// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

#include "node.h"
#include "env-inl.h"
#include "util-inl.h"
#include "v8.h"

namespace node {

using v8::Array;
using v8::ArrayBuffer;
using v8::BackingStore;
using v8::Context;
using v8::FunctionCallbackInfo;
using v8::HeapCodeStatistics;
using v8::HeapSpaceStatistics;
using v8::HeapStatistics;
using v8::Integer;
using v8::Isolate;
using v8::Local;
using v8::NewStringType;
using v8::Object;
using v8::ScriptCompiler;
using v8::String;
using v8::Uint32;
using v8::V8;
using v8::Value;


#define HEAP_STATISTICS_PROPERTIES(V)                                         \
  V(0, total_heap_size, kTotalHeapSizeIndex)                                  \
  V(1, total_heap_size_executable, kTotalHeapSizeExecutableIndex)             \
  V(2, total_physical_size, kTotalPhysicalSizeIndex)                          \
  V(3, total_available_size, kTotalAvailableSize)                             \
  V(4, used_heap_size, kUsedHeapSizeIndex)                                    \
  V(5, heap_size_limit, kHeapSizeLimitIndex)                                  \
  V(6, malloced_memory, kMallocedMemoryIndex)                                 \
  V(7, peak_malloced_memory, kPeakMallocedMemoryIndex)                        \
  V(8, does_zap_garbage, kDoesZapGarbageIndex)                                \
  V(9, number_of_native_contexts, kNumberOfNativeContextsIndex)               \
  V(10, number_of_detached_contexts, kNumberOfDetachedContextsIndex)

#define V(a, b, c) +1
static const size_t kHeapStatisticsPropertiesCount =
    HEAP_STATISTICS_PROPERTIES(V);
#undef V


#define HEAP_SPACE_STATISTICS_PROPERTIES(V)                                   \
  V(0, space_size, kSpaceSizeIndex)                                           \
  V(1, space_used_size, kSpaceUsedSizeIndex)                                  \
  V(2, space_available_size, kSpaceAvailableSizeIndex)                        \
  V(3, physical_space_size, kPhysicalSpaceSizeIndex)

#define V(a, b, c) +1
static const size_t kHeapSpaceStatisticsPropertiesCount =
    HEAP_SPACE_STATISTICS_PROPERTIES(V);
#undef V


#define HEAP_CODE_STATISTICS_PROPERTIES(V)                                    \
  V(0, code_and_metadata_size, kCodeAndMetadataSizeIndex)                    \
  V(1, bytecode_and_metadata_size, kBytecodeAndMetadataSizeIndex)             \
  V(2, external_script_source_size, kExternalScriptSourceSizeIndex)

#define V(a, b, c) +1
static const size_t kHeapCodeStatisticsPropertiesCount =
    HEAP_CODE_STATISTICS_PROPERTIES(V);
#undef V

void CachedDataVersionTag(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  Local<Integer> result =
      Integer::NewFromUnsigned(env->isolate(),
                               ScriptCompiler::CachedDataVersionTag());
  args.GetReturnValue().Set(result);
}


void UpdateHeapStatisticsArrayBuffer(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  HeapStatistics s;
  env->isolate()->GetHeapStatistics(&s);
  double* const buffer = env->heap_statistics_buffer();
#define V(index, name, _) buffer[index] = static_cast<double>(s.name());
  HEAP_STATISTICS_PROPERTIES(V)
#undef V
}


void UpdateHeapSpaceStatisticsBuffer(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  HeapSpaceStatistics s;
  Isolate* const isolate = env->isolate();
  double* buffer = env->heap_space_statistics_buffer();
  size_t number_of_heap_spaces = env->isolate()->NumberOfHeapSpaces();

  for (size_t i = 0; i < number_of_heap_spaces; i++) {
    isolate->GetHeapSpaceStatistics(&s, i);
    size_t const property_offset = i * kHeapSpaceStatisticsPropertiesCount;
#define V(index, name, _) buffer[property_offset + index] = \
                              static_cast<double>(s.name());
      HEAP_SPACE_STATISTICS_PROPERTIES(V)
#undef V
  }
}


void UpdateHeapCodeStatisticsArrayBuffer(
    const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  HeapCodeStatistics s;
  env->isolate()->GetHeapCodeAndMetadataStatistics(&s);
  double* const buffer = env->heap_code_statistics_buffer();
#define V(index, name, _) buffer[index] = static_cast<double>(s.name());
  HEAP_CODE_STATISTICS_PROPERTIES(V)
#undef V
}


void SetFlagsFromString(const FunctionCallbackInfo<Value>& args) {
  CHECK(args[0]->IsString());
  String::Utf8Value flags(args.GetIsolate(), args[0]);
  V8::SetFlagsFromString(*flags, static_cast<size_t>(flags.length()));
}


void Initialize(Local<Object> target,
                Local<Value> unused,
                Local<Context> context,
                void* priv) {
  Environment* env = Environment::GetCurrent(context);

  env->SetMethodNoSideEffect(target, "cachedDataVersionTag",
                             CachedDataVersionTag);

  // Export symbols used by v8.getHeapStatistics()
  env->SetMethod(target,
                 "updateHeapStatisticsArrayBuffer",
                 UpdateHeapStatisticsArrayBuffer);

  const size_t heap_statistics_buffer_byte_length =
      sizeof(*env->heap_statistics_buffer()) * kHeapStatisticsPropertiesCount;

  Local<ArrayBuffer> heap_statistics_ab =
      ArrayBuffer::New(env->isolate(), heap_statistics_buffer_byte_length);
  env->set_heap_statistics_buffer(heap_statistics_ab->GetBackingStore());
  target->Set(env->context(),
              FIXED_ONE_BYTE_STRING(env->isolate(),
                                    "heapStatisticsArrayBuffer"),
              heap_statistics_ab).Check();

#define V(i, _, name)                                                         \
  target->Set(env->context(),                                                 \
              FIXED_ONE_BYTE_STRING(env->isolate(), #name),                   \
              Uint32::NewFromUnsigned(env->isolate(), i)).Check();

  HEAP_STATISTICS_PROPERTIES(V)
#undef V

  // Export symbols used by v8.getHeapCodeStatistics()
  env->SetMethod(target,
                 "updateHeapCodeStatisticsArrayBuffer",
                 UpdateHeapCodeStatisticsArrayBuffer);

  const size_t heap_code_statistics_buffer_byte_length =
      sizeof(*env->heap_code_statistics_buffer())
      * kHeapCodeStatisticsPropertiesCount;

  Local<ArrayBuffer> heap_code_statistics_ab =
      ArrayBuffer::New(env->isolate(),
                       heap_code_statistics_buffer_byte_length);
  env->set_heap_code_statistics_buffer(
      heap_code_statistics_ab->GetBackingStore());
  target->Set(env->context(),
              FIXED_ONE_BYTE_STRING(env->isolate(),
                                    "heapCodeStatisticsArrayBuffer"),
              heap_code_statistics_ab)
  .Check();

#define V(i, _, name)                                                         \
  target->Set(env->context(),                                                 \
              FIXED_ONE_BYTE_STRING(env->isolate(), #name),                   \
              Uint32::NewFromUnsigned(env->isolate(), i)).Check();

  HEAP_CODE_STATISTICS_PROPERTIES(V)
#undef V

  // Export symbols used by v8.getHeapSpaceStatistics()
  target->Set(env->context(),
              FIXED_ONE_BYTE_STRING(env->isolate(),
                                    "kHeapSpaceStatisticsPropertiesCount"),
              Uint32::NewFromUnsigned(env->isolate(),
                                      kHeapSpaceStatisticsPropertiesCount))
              .Check();

  size_t number_of_heap_spaces = env->isolate()->NumberOfHeapSpaces();

  // Heap space names are extracted once and exposed to JavaScript to
  // avoid excessive creation of heap space name Strings.
  HeapSpaceStatistics s;
  const Local<Array> heap_spaces = Array::New(env->isolate(),
                                              number_of_heap_spaces);
  for (size_t i = 0; i < number_of_heap_spaces; i++) {
    env->isolate()->GetHeapSpaceStatistics(&s, i);
    Local<String> heap_space_name = String::NewFromUtf8(env->isolate(),
                                                        s.space_name(),
                                                        NewStringType::kNormal)
                                        .ToLocalChecked();
    heap_spaces->Set(env->context(), i, heap_space_name).Check();
  }
  target->Set(env->context(),
              FIXED_ONE_BYTE_STRING(env->isolate(), "kHeapSpaces"),
              heap_spaces).Check();

  env->SetMethod(target,
                 "updateHeapSpaceStatisticsArrayBuffer",
                 UpdateHeapSpaceStatisticsBuffer);

  const size_t heap_space_statistics_buffer_byte_length =
      sizeof(*env->heap_space_statistics_buffer()) *
      kHeapSpaceStatisticsPropertiesCount *
      number_of_heap_spaces;

  Local<ArrayBuffer> heap_space_statistics_ab =
      ArrayBuffer::New(env->isolate(),
                       heap_space_statistics_buffer_byte_length);
  env->set_heap_space_statistics_buffer(
      heap_space_statistics_ab->GetBackingStore());
  target->Set(env->context(),
              FIXED_ONE_BYTE_STRING(env->isolate(),
                                    "heapSpaceStatisticsArrayBuffer"),
              heap_space_statistics_ab)
              .Check();

#define V(i, _, name)                                                         \
  target->Set(env->context(),                                                 \
              FIXED_ONE_BYTE_STRING(env->isolate(), #name),                   \
              Uint32::NewFromUnsigned(env->isolate(), i)).Check();

  HEAP_SPACE_STATISTICS_PROPERTIES(V)
#undef V

  // Export symbols used by v8.setFlagsFromString()
  env->SetMethod(target, "setFlagsFromString", SetFlagsFromString);
}

}  // namespace node

NODE_MODULE_CONTEXT_AWARE_INTERNAL(v8, node::Initialize)
