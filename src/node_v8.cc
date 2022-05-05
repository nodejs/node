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

#include "node_v8.h"
#include "base_object-inl.h"
#include "env-inl.h"
#include "memory_tracker-inl.h"
#include "node.h"
#include "node_external_reference.h"
#include "util-inl.h"
#include "v8.h"

namespace node {
namespace v8_utils {
using v8::Array;
using v8::Context;
using v8::FunctionCallbackInfo;
using v8::HandleScope;
using v8::HeapCodeStatistics;
using v8::HeapSpaceStatistics;
using v8::HeapStatistics;
using v8::Integer;
using v8::Isolate;
using v8::Local;
using v8::Object;
using v8::ScriptCompiler;
using v8::String;
using v8::Uint32;
using v8::V8;
using v8::Value;

#define HEAP_STATISTICS_PROPERTIES(V)                                          \
  V(0, total_heap_size, kTotalHeapSizeIndex)                                   \
  V(1, total_heap_size_executable, kTotalHeapSizeExecutableIndex)              \
  V(2, total_physical_size, kTotalPhysicalSizeIndex)                           \
  V(3, total_available_size, kTotalAvailableSize)                              \
  V(4, used_heap_size, kUsedHeapSizeIndex)                                     \
  V(5, heap_size_limit, kHeapSizeLimitIndex)                                   \
  V(6, malloced_memory, kMallocedMemoryIndex)                                  \
  V(7, peak_malloced_memory, kPeakMallocedMemoryIndex)                         \
  V(8, does_zap_garbage, kDoesZapGarbageIndex)                                 \
  V(9, number_of_native_contexts, kNumberOfNativeContextsIndex)                \
  V(10, number_of_detached_contexts, kNumberOfDetachedContextsIndex)           \
  V(11, total_global_handles_size, kTotalGlobalHandlesSizeIndex)               \
  V(12, used_global_handles_size, kUsedGlobalHandlesSizeIndex)                 \
  V(13, external_memory, kExternalMemoryIndex)

#define V(a, b, c) +1
static constexpr size_t kHeapStatisticsPropertiesCount =
    HEAP_STATISTICS_PROPERTIES(V);
#undef V

#define HEAP_SPACE_STATISTICS_PROPERTIES(V)                                   \
  V(0, space_size, kSpaceSizeIndex)                                           \
  V(1, space_used_size, kSpaceUsedSizeIndex)                                  \
  V(2, space_available_size, kSpaceAvailableSizeIndex)                        \
  V(3, physical_space_size, kPhysicalSpaceSizeIndex)

#define V(a, b, c) +1
static constexpr size_t kHeapSpaceStatisticsPropertiesCount =
    HEAP_SPACE_STATISTICS_PROPERTIES(V);
#undef V

#define HEAP_CODE_STATISTICS_PROPERTIES(V)                                     \
  V(0, code_and_metadata_size, kCodeAndMetadataSizeIndex)                      \
  V(1, bytecode_and_metadata_size, kBytecodeAndMetadataSizeIndex)              \
  V(2, external_script_source_size, kExternalScriptSourceSizeIndex)            \
  V(3, cpu_profiler_metadata_size, kCPUProfilerMetaDataSizeIndex)

#define V(a, b, c) +1
static const size_t kHeapCodeStatisticsPropertiesCount =
    HEAP_CODE_STATISTICS_PROPERTIES(V);
#undef V

BindingData::BindingData(Environment* env, Local<Object> obj)
    : SnapshotableObject(env, obj, type_int),
      heap_statistics_buffer(env->isolate(), kHeapStatisticsPropertiesCount),
      heap_space_statistics_buffer(env->isolate(),
                                   kHeapSpaceStatisticsPropertiesCount),
      heap_code_statistics_buffer(env->isolate(),
                                  kHeapCodeStatisticsPropertiesCount) {
  obj->Set(env->context(),
           FIXED_ONE_BYTE_STRING(env->isolate(), "heapStatisticsBuffer"),
           heap_statistics_buffer.GetJSArray())
      .Check();
  obj->Set(env->context(),
           FIXED_ONE_BYTE_STRING(env->isolate(), "heapCodeStatisticsBuffer"),
           heap_code_statistics_buffer.GetJSArray())
      .Check();
  obj->Set(env->context(),
           FIXED_ONE_BYTE_STRING(env->isolate(), "heapSpaceStatisticsBuffer"),
           heap_space_statistics_buffer.GetJSArray())
      .Check();
}

void BindingData::PrepareForSerialization(Local<Context> context,
                                          v8::SnapshotCreator* creator) {
  // We'll just re-initialize the buffers in the constructor since their
  // contents can be thrown away once consumed in the previous call.
  heap_statistics_buffer.Release();
  heap_space_statistics_buffer.Release();
  heap_code_statistics_buffer.Release();
}

void BindingData::Deserialize(Local<Context> context,
                              Local<Object> holder,
                              int index,
                              InternalFieldInfo* info) {
  DCHECK_EQ(index, BaseObject::kSlot);
  HandleScope scope(context->GetIsolate());
  Environment* env = Environment::GetCurrent(context);
  BindingData* binding = env->AddBindingData<BindingData>(context, holder);
  CHECK_NOT_NULL(binding);
}

InternalFieldInfo* BindingData::Serialize(int index) {
  DCHECK_EQ(index, BaseObject::kSlot);
  InternalFieldInfo* info = InternalFieldInfo::New(type());
  return info;
}

void BindingData::MemoryInfo(MemoryTracker* tracker) const {
  tracker->TrackField("heap_statistics_buffer", heap_statistics_buffer);
  tracker->TrackField("heap_space_statistics_buffer",
                      heap_space_statistics_buffer);
  tracker->TrackField("heap_code_statistics_buffer",
                      heap_code_statistics_buffer);
}

void CachedDataVersionTag(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  Local<Integer> result =
      Integer::NewFromUnsigned(env->isolate(),
                               ScriptCompiler::CachedDataVersionTag());
  args.GetReturnValue().Set(result);
}

void UpdateHeapStatisticsBuffer(const FunctionCallbackInfo<Value>& args) {
  BindingData* data = Environment::GetBindingData<BindingData>(args);
  HeapStatistics s;
  args.GetIsolate()->GetHeapStatistics(&s);
  AliasedFloat64Array& buffer = data->heap_statistics_buffer;
#define V(index, name, _) buffer[index] = static_cast<double>(s.name());
  HEAP_STATISTICS_PROPERTIES(V)
#undef V
}


void UpdateHeapSpaceStatisticsBuffer(const FunctionCallbackInfo<Value>& args) {
  BindingData* data = Environment::GetBindingData<BindingData>(args);
  HeapSpaceStatistics s;
  Isolate* const isolate = args.GetIsolate();
  CHECK(args[0]->IsUint32());
  size_t space_index = static_cast<size_t>(args[0].As<v8::Uint32>()->Value());
  isolate->GetHeapSpaceStatistics(&s, space_index);

  AliasedFloat64Array& buffer = data->heap_space_statistics_buffer;

#define V(index, name, _) buffer[index] = static_cast<double>(s.name());
  HEAP_SPACE_STATISTICS_PROPERTIES(V)
#undef V
}

void UpdateHeapCodeStatisticsBuffer(const FunctionCallbackInfo<Value>& args) {
  BindingData* data = Environment::GetBindingData<BindingData>(args);
  HeapCodeStatistics s;
  args.GetIsolate()->GetHeapCodeAndMetadataStatistics(&s);
  AliasedFloat64Array& buffer = data->heap_code_statistics_buffer;

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
  BindingData* const binding_data =
      env->AddBindingData<BindingData>(context, target);
  if (binding_data == nullptr) return;

  env->SetMethodNoSideEffect(target, "cachedDataVersionTag",
                             CachedDataVersionTag);
  env->SetMethod(
      target, "updateHeapStatisticsBuffer", UpdateHeapStatisticsBuffer);

  env->SetMethod(
      target, "updateHeapCodeStatisticsBuffer", UpdateHeapCodeStatisticsBuffer);

  size_t number_of_heap_spaces = env->isolate()->NumberOfHeapSpaces();

  // Heap space names are extracted once and exposed to JavaScript to
  // avoid excessive creation of heap space name Strings.
  HeapSpaceStatistics s;
  MaybeStackBuffer<Local<Value>, 16> heap_spaces(number_of_heap_spaces);
  for (size_t i = 0; i < number_of_heap_spaces; i++) {
    env->isolate()->GetHeapSpaceStatistics(&s, i);
    heap_spaces[i] = String::NewFromUtf8(env->isolate(), s.space_name())
                                             .ToLocalChecked();
  }
  target->Set(env->context(),
              FIXED_ONE_BYTE_STRING(env->isolate(), "kHeapSpaces"),
              Array::New(env->isolate(),
                         heap_spaces.out(),
                         number_of_heap_spaces)).Check();

  env->SetMethod(target,
                 "updateHeapSpaceStatisticsBuffer",
                 UpdateHeapSpaceStatisticsBuffer);

#define V(i, _, name)                                                          \
  target                                                                       \
      ->Set(env->context(),                                                    \
            FIXED_ONE_BYTE_STRING(env->isolate(), #name),                      \
            Uint32::NewFromUnsigned(env->isolate(), i))                        \
      .Check();

  HEAP_STATISTICS_PROPERTIES(V)
  HEAP_CODE_STATISTICS_PROPERTIES(V)
  HEAP_SPACE_STATISTICS_PROPERTIES(V)
#undef V

  // Export symbols used by v8.setFlagsFromString()
  env->SetMethod(target, "setFlagsFromString", SetFlagsFromString);
}

void RegisterExternalReferences(ExternalReferenceRegistry* registry) {
  registry->Register(CachedDataVersionTag);
  registry->Register(UpdateHeapStatisticsBuffer);
  registry->Register(UpdateHeapCodeStatisticsBuffer);
  registry->Register(UpdateHeapSpaceStatisticsBuffer);
  registry->Register(SetFlagsFromString);
}

}  // namespace v8_utils
}  // namespace node

NODE_MODULE_CONTEXT_AWARE_INTERNAL(v8, node::v8_utils::Initialize)
NODE_MODULE_EXTERNAL_REFERENCE(v8, node::v8_utils::RegisterExternalReferences)
