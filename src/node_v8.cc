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
#include <unordered_map>
#include "aliased_buffer-inl.h"
#include "base_object-inl.h"
#include "env-inl.h"
#include "memory_tracker-inl.h"
#include "node.h"
#include "node_internals.h"
#include "node_external_reference.h"
#include "node_profiling.h"
#include "permission/permission.h"
#include "util-inl.h"
#include "v8-container.h"
#include "v8-profiler.h"
#include "v8.h"

namespace node {
namespace v8_utils {

using v8::AllocationProfile;
using v8::Array;
using v8::BigInt;
using v8::CFunction;
using v8::Context;
using v8::CpuProfile;
using v8::CpuProfilingResult;
using v8::CpuProfilingStatus;
using v8::DictionaryTemplate;
using v8::FunctionCallbackInfo;
using v8::FunctionTemplate;
using v8::HandleScope;
using v8::HeapCodeStatistics;
using v8::HeapProfiler;
using v8::HeapSpaceStatistics;
using v8::HeapStatistics;
using v8::Integer;
using v8::Isolate;
using v8::Local;
using v8::LocalVector;
using v8::MaybeLocal;
using v8::Number;
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
  V(13, external_memory, kExternalMemoryIndex)                                 \
  V(14, total_allocated_bytes, kTotalAllocatedBytes)

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

// Forward declaration for the env cleanup hook (used by ~BindingData).
static void CleanupHeapProfiling(void* data);

BindingData::BindingData(Realm* realm,
                         Local<Object> obj,
                         InternalFieldInfo* info)
    : SnapshotableObject(realm, obj, type_int),
      heap_statistics_buffer(realm->isolate(),
                             kHeapStatisticsPropertiesCount,
                             MAYBE_FIELD_PTR(info, heap_statistics_buffer)),
      heap_space_statistics_buffer(
          realm->isolate(),
          kHeapSpaceStatisticsPropertiesCount,
          MAYBE_FIELD_PTR(info, heap_space_statistics_buffer)),
      heap_code_statistics_buffer(
          realm->isolate(),
          kHeapCodeStatisticsPropertiesCount,
          MAYBE_FIELD_PTR(info, heap_code_statistics_buffer)) {
  Local<Context> context = realm->context();
  if (info == nullptr) {
    obj->Set(context,
             FIXED_ONE_BYTE_STRING(realm->isolate(), "heapStatisticsBuffer"),
             heap_statistics_buffer.GetJSArray())
        .Check();
    obj->Set(
           context,
           FIXED_ONE_BYTE_STRING(realm->isolate(), "heapCodeStatisticsBuffer"),
           heap_code_statistics_buffer.GetJSArray())
        .Check();
    obj->Set(
           context,
           FIXED_ONE_BYTE_STRING(realm->isolate(), "heapSpaceStatisticsBuffer"),
           heap_space_statistics_buffer.GetJSArray())
        .Check();
  } else {
    heap_statistics_buffer.Deserialize(realm->context());
    heap_code_statistics_buffer.Deserialize(realm->context());
    heap_space_statistics_buffer.Deserialize(realm->context());
  }
  heap_statistics_buffer.MakeWeak();
  heap_space_statistics_buffer.MakeWeak();
  heap_code_statistics_buffer.MakeWeak();
}

// Tears down profiler state if the Environment goes away while profiling is
// still active, as on worker termination. The raw pointers are safe because
// cleanup hooks run inside Isolate::Scope, before the isolate is disposed.
// Deliberately not a BindingData*: Realm::RunCleanup() destroys BindingData
// before the env cleanup queue is drained, so that would dangle.
struct HeapProfilingCleanup {
  Isolate* isolate;
  NodeArrayBufferAllocator* node_allocator;
  ProfilingArrayBufferAllocator* profiling_allocator;
  bool is_labels_session = false;
  bool cleaned_up = false;

  // Idempotent: only the first call has an effect.
  void DoCleanup() {
    if (cleaned_up) return;
    cleaned_up = true;

    HeapProfiler* profiler = isolate->GetHeapProfiler();
    profiler->StopSamplingHeapProfiler();
#ifdef V8_HEAP_PROFILER_SAMPLE_LABELS
    profiler->SetHeapProfileSampleLabelsKey(Local<Value>());
#endif  // V8_HEAP_PROFILER_SAMPLE_LABELS
    if (node_allocator != nullptr) {
      node_allocator->ClearProfilingAllocator();
    }
    if (profiling_allocator != nullptr) {
      profiling_allocator->Disable();
    }
    isolate = nullptr;
    node_allocator = nullptr;
    profiling_allocator = nullptr;
  }
};

static void CleanupHeapProfiling(void* data) {
  auto* ctx = static_cast<HeapProfilingCleanup*>(data);
  ctx->DoCleanup();
  delete ctx;
}

#ifdef V8_HEAP_PROFILER_SAMPLE_LABELS
// Stores the ALS key for later use by StartHeapProfile. Does not arm the V8
// labelling key here — that happens only when a labels:true session starts —
// except when the ALS is first created while a labels session is already
// live (mid-session first use), in which case arm it immediately so that
// allocations after this call are labelled.
void SetHeapProfileLabelsStore(const FunctionCallbackInfo<Value>& args) {
  CHECK_EQ(args.Length(), 1);
  // The AsyncLocalStorage instance; only a JSReceiver can be a CPED Map key.
  CHECK(args[0]->IsObject());
  Isolate* isolate = args.GetIsolate();
  BindingData* binding_data = Realm::GetBindingData<BindingData>(args);
  binding_data->heap_profile_labels_als_key.Reset(isolate, args[0]);
  auto* cleanup = binding_data->heap_profiling_cleanup_;
  if (cleanup != nullptr && cleanup->is_labels_session) {
    isolate->GetHeapProfiler()->SetHeapProfileSampleLabelsKey(args[0]);
  }
}
#endif  // V8_HEAP_PROFILER_SAMPLE_LABELS

BindingData::~BindingData() {
  // This runs during Realm::RunCleanup(), before the env cleanup queue is
  // drained and while the isolate is still alive, so cleaning up here stops
  // V8 holding a pointer into a BindingData that is about to go away.
  if (heap_profiling_cleanup_ != nullptr) {
    heap_profiling_cleanup_->DoCleanup();
    env()->RemoveCleanupHook(CleanupHeapProfiling, heap_profiling_cleanup_);
    delete heap_profiling_cleanup_;
    heap_profiling_cleanup_ = nullptr;
  }
}

bool BindingData::PrepareForSerialization(Local<Context> context,
                                          v8::SnapshotCreator* creator) {
  DCHECK_NULL(internal_field_info_);
  internal_field_info_ = InternalFieldInfoBase::New<InternalFieldInfo>(type());
  internal_field_info_->heap_statistics_buffer =
      heap_statistics_buffer.Serialize(context, creator);
  internal_field_info_->heap_space_statistics_buffer =
      heap_space_statistics_buffer.Serialize(context, creator);
  internal_field_info_->heap_code_statistics_buffer =
      heap_code_statistics_buffer.Serialize(context, creator);
  // Return true because we need to maintain the reference to the binding from
  // JS land.
  return true;
}

void BindingData::Deserialize(Local<Context> context,
                              Local<Object> holder,
                              int index,
                              InternalFieldInfoBase* info) {
  DCHECK_IS_SNAPSHOT_SLOT(index);
  HandleScope scope(Isolate::GetCurrent());
  Realm* realm = Realm::GetCurrent(context);
  // Recreate the buffer in the constructor.
  InternalFieldInfo* casted_info = static_cast<InternalFieldInfo*>(info);
  BindingData* binding =
      realm->AddBindingData<BindingData>(holder, casted_info);
  CHECK_NOT_NULL(binding);
}

InternalFieldInfoBase* BindingData::Serialize(int index) {
  DCHECK_IS_SNAPSHOT_SLOT(index);
  InternalFieldInfo* info = internal_field_info_;
  internal_field_info_ = nullptr;
  return info;
}

void BindingData::MemoryInfo(MemoryTracker* tracker) const {
  tracker->TrackField("heap_statistics_buffer", heap_statistics_buffer);
  tracker->TrackField("heap_space_statistics_buffer",
                      heap_space_statistics_buffer);
  tracker->TrackField("heap_code_statistics_buffer",
                      heap_code_statistics_buffer);
  tracker->TrackFieldWithSize("heap_profile_labels_als_key",
                              heap_profile_labels_als_key.IsEmpty() ? 0 :
                                  sizeof(v8::Global<v8::Value>));
}

void CachedDataVersionTag(const FunctionCallbackInfo<Value>& args) {
  Local<Integer> result = Integer::NewFromUnsigned(
      args.GetIsolate(), ScriptCompiler::CachedDataVersionTag());
  args.GetReturnValue().Set(result);
}

void SetHeapSnapshotNearHeapLimit(const FunctionCallbackInfo<Value>& args) {
  CHECK(args[0]->IsUint32());
  Environment* env = Environment::GetCurrent(args);
  uint32_t limit = args[0].As<v8::Uint32>()->Value();
  CHECK_GT(limit, 0);

  std::string dir = env->options()->diagnostic_dir;
  if (dir.empty()) {
    dir = Environment::GetCwd(env->exec_path());
  }
  THROW_IF_INSUFFICIENT_PERMISSIONS(
      env, permission::PermissionScope::kFileSystemWrite, dir);

  env->AddHeapSnapshotNearHeapLimitCallback();
  env->set_heap_snapshot_near_heap_limit(limit);
}

void UpdateHeapStatisticsBuffer(const FunctionCallbackInfo<Value>& args) {
  BindingData* data = Realm::GetBindingData<BindingData>(args);
  HeapStatistics s;
  args.GetIsolate()->GetHeapStatistics(&s);
  AliasedFloat64Array& buffer = data->heap_statistics_buffer;
#define V(index, name, _) buffer[index] = static_cast<double>(s.name());
  HEAP_STATISTICS_PROPERTIES(V)
#undef V
}


void UpdateHeapSpaceStatisticsBuffer(const FunctionCallbackInfo<Value>& args) {
  BindingData* data = Realm::GetBindingData<BindingData>(args);
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
  BindingData* data = Realm::GetBindingData<BindingData>(args);
  HeapCodeStatistics s;
  args.GetIsolate()->GetHeapCodeAndMetadataStatistics(&s);
  AliasedFloat64Array& buffer = data->heap_code_statistics_buffer;

#define V(index, name, _) buffer[index] = static_cast<double>(s.name());
  HEAP_CODE_STATISTICS_PROPERTIES(V)
#undef V
}


void SetFlagsFromString(const FunctionCallbackInfo<Value>& args) {
  CHECK(args[0]->IsString());
  Utf8Value flags(args.GetIsolate(), args[0]);
  V8::SetFlagsFromString(flags.out(), flags.length());
}

void StartCpuProfile(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  Isolate* isolate = env->isolate();
  CpuProfileOptions options = ParseCpuProfileOptions(args);
  CpuProfilingResult result = env->StartCpuProfile(options);
  if (result.status == CpuProfilingStatus::kErrorTooManyProfilers) {
    return THROW_ERR_CPU_PROFILE_TOO_MANY(isolate,
                                          "There are too many CPU profiles");
  } else if (result.status == CpuProfilingStatus::kStarted) {
    args.GetReturnValue().Set(Number::New(isolate, result.id));
  }
}

void StopCpuProfile(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  Isolate* isolate = env->isolate();
  CHECK(args[0]->IsUint32());
  uint32_t profile_id = args[0]->Uint32Value(env->context()).FromJust();
  CpuProfile* profile = env->StopCpuProfile(profile_id);
  if (!profile) {
    return THROW_ERR_CPU_PROFILE_NOT_STARTED(isolate,
                                             "CPU profile not started");
  }
  auto json_out_stream = std::make_unique<node::JSONOutputStream>();
  profile->Serialize(json_out_stream.get(),
                     CpuProfile::SerializationFormat::kJSON);
  profile->Delete();
  Local<Value> ret;
  if (ToV8Value(env->context(), json_out_stream->out_stream().str(), isolate)
          .ToLocal(&ret)) {
    args.GetReturnValue().Set(ret);
  }
}

void StartHeapProfile(const FunctionCallbackInfo<Value>& args) {
  Isolate* isolate = args.GetIsolate();
  auto options = ParseHeapProfileOptions(args);

  if (!isolate->GetHeapProfiler()->StartSamplingHeapProfiler(
          options.sample_interval, options.stack_depth, options.flags)) {
    THROW_ERR_HEAP_PROFILE_HAVE_BEEN_STARTED(isolate,
                                             "Heap profile has been started");
    return;
  }

  BindingData* binding_data = Realm::GetBindingData<BindingData>(args);
  // Stamp a new generation so handles from prior sessions cannot interact
  // with this one even if their V8 session was stolen.
  const uint32_t gen = ++binding_data->heap_profile_session_generation_;
  args.GetReturnValue().Set(gen);

#ifdef V8_HEAP_PROFILER_SAMPLE_LABELS
  {
    Environment* env = Environment::GetCurrent(args);
    // Discard stale Node-side tracking without stopping V8's new session.
    // In normal usage heap_profiling_cleanup_ is null here because V8
    // returned true (no prior session running). This handles the edge case
    // where V8 ended our prior session independently (e.g. an out-of-band
    // inspector HeapProfiler.stopSampling call), which would otherwise leak
    // an env hook whose DoCleanup would later stop an unrelated session.
    // Applies unconditionally so both the labels:true and labels:false paths
    // start with a clean slate.
    if (binding_data->heap_profiling_cleanup_ != nullptr) {
      auto* old = binding_data->heap_profiling_cleanup_;
      if (old->node_allocator != nullptr)
        old->node_allocator->ClearProfilingAllocator();
      if (old->profiling_allocator != nullptr)
        old->profiling_allocator->Disable();
      env->RemoveCleanupHook(CleanupHeapProfiling, old);
      delete old;
      binding_data->heap_profiling_cleanup_ = nullptr;
    }
    // 4th arg (index 3): labels_enabled — set up allocator and labels key.
    if (args.Length() > 3 && args[3]->IsTrue()) {
      if (!binding_data->heap_profile_labels_als_key.IsEmpty()) {
        isolate->GetHeapProfiler()->SetHeapProfileSampleLabelsKey(
            binding_data->heap_profile_labels_als_key.Get(isolate));
      }
      auto* node_allocator = env->isolate_data()->node_allocator();
      ProfilingArrayBufferAllocator* profiling_allocator = nullptr;
      if (node_allocator != nullptr) {
        auto* candidate = node_allocator->CreateProfilingAllocator();
        // Only own the allocator (and record it for teardown) if Enable took
        // this isolate. If a shared allocator is already bound to another
        // isolate, Enable() is a no-op returning false; recording it here
        // would let this session's cleanup disable the other isolate's
        // tracking.
        if (candidate->Enable(isolate)) profiling_allocator = candidate;
      }
      auto* cleanup = new HeapProfilingCleanup{
          isolate,
          profiling_allocator != nullptr ? node_allocator : nullptr,
          profiling_allocator};
      cleanup->is_labels_session = true;
      env->AddCleanupHook(CleanupHeapProfiling, cleanup);
      binding_data->heap_profiling_cleanup_ = cleanup;
    } else {
      // Defensively clear any key left over from a previous labels session so
      // that a labels:false session never emits labelled samples.
      isolate->GetHeapProfiler()->SetHeapProfileSampleLabelsKey(Local<Value>());
    }
  }
#endif
}

void StopHeapProfile(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  Isolate* isolate = env->isolate();
  // If a session generation was provided and it no longer matches the current
  // one, a newer session is live.  Leave it untouched and behave as if this
  // handle was already stopped.
  if (args.Length() > 0 && args[0]->IsUint32()) {
    BindingData* bd = Realm::GetBindingData<BindingData>(args);
    if (args[0].As<Uint32>()->Value() != bd->heap_profile_session_generation_)
      return;
  }
  std::ostringstream out_stream;
  bool success = node::SerializeHeapProfile(isolate, out_stream);
  // Run Node-side teardown unconditionally whenever a session was tracked.
  // SerializeHeapProfile stops V8's profiler on success; DoCleanup's own
  // StopSamplingHeapProfiler is a safe no-op if that already happened.
  // This covers the out-of-band case where an inspector
  // HeapProfiler.stopSampling call stopped V8's sampler before we did,
  // which causes serialisation to fail — without this, the profiling
  // allocator would stay installed on the ArrayBuffer alloc/free path
  // for the rest of the process with no way to remove it.
  BindingData* binding_data = Realm::GetBindingData<BindingData>(args);
  if (binding_data->heap_profiling_cleanup_ != nullptr) {
    binding_data->heap_profiling_cleanup_->DoCleanup();
    env->RemoveCleanupHook(
        CleanupHeapProfiling, binding_data->heap_profiling_cleanup_);
    delete binding_data->heap_profiling_cleanup_;
    binding_data->heap_profiling_cleanup_ = nullptr;
  }
  if (success) {
    Local<Value> result;
    if (ToV8Value(env->context(), out_stream.str(), isolate).ToLocal(&result)) {
      args.GetReturnValue().Set(result);
    }
  } else {
    THROW_ERR_HEAP_PROFILE_NOT_STARTED(isolate, "heap profile not started");
  }
}

#ifdef V8_HEAP_PROFILER_SAMPLE_LABELS
// Test-only accessor: returns true if the NodeArrayBufferAllocator currently
// has a profiling delegate installed. Exposed via internalBinding('v8') so
// tests can verify that StopHeapProfile released the allocator.
static void GetProfilingAllocatorActive(
    const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  auto* node_alloc = env->isolate_data()->node_allocator();
  bool active = node_alloc != nullptr &&
                node_alloc->GetProfilingAllocator() != nullptr;
  args.GetReturnValue().Set(active);
}
#endif  // V8_HEAP_PROFILER_SAMPLE_LABELS

static void IsStringOneByteRepresentation(
    const FunctionCallbackInfo<Value>& args) {
  CHECK_EQ(args.Length(), 1);
  CHECK(args[0]->IsString());
  bool is_one_byte = args[0].As<String>()->IsOneByte();
  args.GetReturnValue().Set(is_one_byte);
}

static bool FastIsStringOneByteRepresentation(Local<Value> receiver,
                                              const Local<Value> target) {
  CHECK(target->IsString());
  return target.As<String>()->IsOneByte();
}

CFunction fast_is_string_one_byte_representation_(
    CFunction::Make(FastIsStringOneByteRepresentation));

void GetHashSeed(const FunctionCallbackInfo<Value>& args) {
  Isolate* isolate = args.GetIsolate();
  uint64_t hash_seed = isolate->GetHashSeed();
  args.GetReturnValue().Set(BigInt::NewFromUnsigned(isolate, hash_seed));
}

static const char* GetGCTypeName(v8::GCType gc_type) {
  switch (gc_type) {
    case v8::GCType::kGCTypeScavenge:
      return "Scavenge";
    case v8::GCType::kGCTypeMinorMarkSweep:
      return "MinorMarkSweep";
    case v8::GCType::kGCTypeMarkSweepCompact:
      return "MarkSweepCompact";
    case v8::GCType::kGCTypeIncrementalMarking:
      return "IncrementalMarking";
    case v8::GCType::kGCTypeProcessWeakCallbacks:
      return "ProcessWeakCallbacks";
    default:
      return "Unknown";
  }
}

static void SetHeapStatistics(JSONWriter* writer, Isolate* isolate) {
  HeapStatistics heap_statistics;
  isolate->GetHeapStatistics(&heap_statistics);
  writer->json_objectstart("heapStatistics");
  writer->json_keyvalue("totalHeapSize", heap_statistics.total_heap_size());
  writer->json_keyvalue("totalHeapSizeExecutable",
                        heap_statistics.total_heap_size_executable());
  writer->json_keyvalue("totalPhysicalSize",
                        heap_statistics.total_physical_size());
  writer->json_keyvalue("totalAvailableSize",
                        heap_statistics.total_available_size());
  writer->json_keyvalue("totalGlobalHandlesSize",
                        heap_statistics.total_global_handles_size());
  writer->json_keyvalue("usedGlobalHandlesSize",
                        heap_statistics.used_global_handles_size());
  writer->json_keyvalue("usedHeapSize", heap_statistics.used_heap_size());
  writer->json_keyvalue("heapSizeLimit", heap_statistics.heap_size_limit());
  writer->json_keyvalue("mallocedMemory", heap_statistics.malloced_memory());
  writer->json_keyvalue("externalMemory", heap_statistics.external_memory());
  writer->json_keyvalue("peakMallocedMemory",
                        heap_statistics.peak_malloced_memory());
  writer->json_objectend();

  int space_count = isolate->NumberOfHeapSpaces();
  writer->json_arraystart("heapSpaceStatistics");
  for (int i = 0; i < space_count; i++) {
    HeapSpaceStatistics heap_space_statistics;
    isolate->GetHeapSpaceStatistics(&heap_space_statistics, i);
    writer->json_start();
    writer->json_keyvalue("spaceName", heap_space_statistics.space_name());
    writer->json_keyvalue("spaceSize", heap_space_statistics.space_size());
    writer->json_keyvalue("spaceUsedSize",
                          heap_space_statistics.space_used_size());
    writer->json_keyvalue("spaceAvailableSize",
                          heap_space_statistics.space_available_size());
    writer->json_keyvalue("physicalSpaceSize",
                          heap_space_statistics.physical_space_size());
    writer->json_end();
  }
  writer->json_arrayend();
}

static MaybeLocal<Object> ConvertHeapStatsToJSObject(
    Isolate* isolate, const cppgc::HeapStatistics& stats) {
  Local<Context> context = isolate->GetCurrentContext();
  Environment* env = Environment::GetCurrent(isolate);
  // Space Statistics
  LocalVector<Value> space_statistics_array(isolate);
  space_statistics_array.reserve(stats.space_stats.size());

  auto object_stats_template = env->object_stats_template();
  auto page_stats_tmpl = env->page_stats_template();
  auto free_list_statistics_template = env->free_list_statistics_template();
  auto space_stats_tmpl = env->space_stats_template();
  auto heap_stats_tmpl = env->v8_heap_statistics_template();
  if (object_stats_template.IsEmpty()) {
    static constexpr std::string_view object_stats_names[] = {"allocated_bytes",
                                                              "object_count"};
    object_stats_template =
        DictionaryTemplate::New(isolate, object_stats_names);
    env->set_object_stats_template(object_stats_template);
  }
  if (page_stats_tmpl.IsEmpty()) {
    static constexpr std::string_view page_stats_names[] = {
        "committed_size_bytes",
        "resident_size_bytes",
        "used_size_bytes",
        "object_statistics"};
    page_stats_tmpl = DictionaryTemplate::New(isolate, page_stats_names);
    env->set_page_stats_template(page_stats_tmpl);
  }
  if (free_list_statistics_template.IsEmpty()) {
    std::string_view free_list_statistics_names[] = {
        "bucket_size", "free_count", "free_size"};
    free_list_statistics_template =
        DictionaryTemplate::New(isolate, free_list_statistics_names);
    env->set_free_list_statistics_template(free_list_statistics_template);
  }
  if (space_stats_tmpl.IsEmpty()) {
    static constexpr std::string_view space_stats_names[] = {
        "name",
        "committed_size_bytes",
        "resident_size_bytes",
        "used_size_bytes",
        "page_stats",
        "free_list_stats"};
    space_stats_tmpl = DictionaryTemplate::New(isolate, space_stats_names);
    env->set_space_stats_template(space_stats_tmpl);
  }
  if (heap_stats_tmpl.IsEmpty()) {
    static constexpr std::string_view heap_statistics_names[] = {
        "committed_size_bytes",
        "resident_size_bytes",
        "used_size_bytes",
        "space_statistics",
        "type_names"};
    heap_stats_tmpl = DictionaryTemplate::New(isolate, heap_statistics_names);
    env->set_v8_heap_statistics_template(heap_stats_tmpl);
  }

  for (size_t i = 0; i < stats.space_stats.size(); i++) {
    const cppgc::HeapStatistics::SpaceStatistics& space_stats =
        stats.space_stats[i];
    // Page Statistics
    LocalVector<Value> page_statistics_array(isolate);
    page_statistics_array.reserve(space_stats.page_stats.size());
    for (size_t j = 0; j < space_stats.page_stats.size(); j++) {
      const cppgc::HeapStatistics::PageStatistics& page_stats =
          space_stats.page_stats[j];
      // Object Statistics
      LocalVector<Value> object_statistics_array(isolate);
      object_statistics_array.reserve(page_stats.object_statistics.size());
      for (size_t k = 0; k < page_stats.object_statistics.size(); k++) {
        const cppgc::HeapStatistics::ObjectStatsEntry& object_stats =
            page_stats.object_statistics[k];
        MaybeLocal<Value> object_stats_values[] = {
            Uint32::NewFromUnsigned(
                isolate, static_cast<uint32_t>(object_stats.allocated_bytes)),
            Uint32::NewFromUnsigned(
                isolate, static_cast<uint32_t>(object_stats.object_count))};
        Local<Object> object_stats_object;
        if (!NewDictionaryInstanceNullProto(
                 context, object_stats_template, object_stats_values)
                 .ToLocal(&object_stats_object)) {
          return MaybeLocal<Object>();
        }
        object_statistics_array.emplace_back(object_stats_object);
      }

      // Set page statistics
      MaybeLocal<Value> page_stats_values[] = {
          Uint32::NewFromUnsigned(
              isolate, static_cast<uint32_t>(page_stats.committed_size_bytes)),
          Uint32::NewFromUnsigned(
              isolate, static_cast<uint32_t>(page_stats.resident_size_bytes)),
          Uint32::NewFromUnsigned(
              isolate, static_cast<uint32_t>(page_stats.used_size_bytes)),
          Array::New(isolate,
                     object_statistics_array.data(),
                     object_statistics_array.size())};
      Local<Object> page_stats_object;
      if (!NewDictionaryInstanceNullProto(
               context, page_stats_tmpl, page_stats_values)
               .ToLocal(&page_stats_object)) {
        return MaybeLocal<Object>();
      }
      page_statistics_array.emplace_back(page_stats_object);
    }

    // Free List Statistics
    MaybeLocal<Value> free_list_statistics_values[] = {
        ToV8ValuePrimitiveArray(
            context, space_stats.free_list_stats.bucket_size, isolate),
        ToV8ValuePrimitiveArray(
            context, space_stats.free_list_stats.free_count, isolate),
        ToV8ValuePrimitiveArray(
            context, space_stats.free_list_stats.free_size, isolate)};

    Local<Object> free_list_statistics_obj;
    if (!NewDictionaryInstanceNullProto(context,
                                        free_list_statistics_template,
                                        free_list_statistics_values)
             .ToLocal(&free_list_statistics_obj)) {
      return MaybeLocal<Object>();
    }

    // Set Space Statistics
    Local<Value> name_value;
    if (!ToV8Value(context, stats.space_stats[i].name, isolate)
             .ToLocal(&name_value)) {
      return MaybeLocal<Object>();
    }
    MaybeLocal<Value> space_stats_values[] = {
        name_value,
        Uint32::NewFromUnsigned(
            isolate,
            static_cast<uint32_t>(stats.space_stats[i].committed_size_bytes)),
        Uint32::NewFromUnsigned(
            isolate,
            static_cast<uint32_t>(stats.space_stats[i].resident_size_bytes)),
        Uint32::NewFromUnsigned(
            isolate,
            static_cast<uint32_t>(stats.space_stats[i].used_size_bytes)),
        Array::New(isolate,
                   page_statistics_array.data(),
                   page_statistics_array.size()),
        free_list_statistics_obj,
    };
    Local<Object> space_stats_object;
    if (!NewDictionaryInstanceNullProto(
             context, space_stats_tmpl, space_stats_values)
             .ToLocal(&space_stats_object)) {
      return MaybeLocal<Object>();
    }
    space_statistics_array.emplace_back(space_stats_object);
  }

  Local<Value> type_names_value;
  if (!ToV8Value(context, stats.type_names, isolate)
           .ToLocal(&type_names_value)) {
    return MaybeLocal<Object>();
  }
  MaybeLocal<Value> heap_statistics_values[] = {
      Uint32::NewFromUnsigned(
          isolate, static_cast<uint32_t>(stats.committed_size_bytes)),
      Uint32::NewFromUnsigned(isolate,
                              static_cast<uint32_t>(stats.resident_size_bytes)),
      Uint32::NewFromUnsigned(isolate,
                              static_cast<uint32_t>(stats.used_size_bytes)),
      Array::New(isolate,
                 space_statistics_array.data(),
                 space_statistics_array.size()),
      type_names_value};

  return NewDictionaryInstanceNullProto(
      context, heap_stats_tmpl, heap_statistics_values);
}

static void GetCppHeapStatistics(const FunctionCallbackInfo<Value>& args) {
  Isolate* isolate = args.GetIsolate();
  HandleScope handle_scope(isolate);

  CHECK_EQ(args.Length(), 1);
  CHECK(args[0]->IsInt32());

  cppgc::HeapStatistics stats = isolate->GetCppHeap()->CollectStatistics(
      FromV8Value<cppgc::HeapStatistics::DetailLevel>(args[0]));

  Local<Object> result;
  if (!ConvertHeapStatsToJSObject(isolate, stats).ToLocal(&result)) {
    return;
  }
  args.GetReturnValue().Set(result);
}

static void BeforeGCCallback(Isolate* isolate,
                             v8::GCType gc_type,
                             v8::GCCallbackFlags flags,
                             void* data) {
  GCProfiler* profiler = static_cast<GCProfiler*>(data);
  if (profiler->current_gc_type != 0) {
    return;
  }
  JSONWriter* writer = profiler->writer();
  writer->json_start();
  writer->json_keyvalue("gcType", GetGCTypeName(gc_type));
  writer->json_objectstart("beforeGC");
  SetHeapStatistics(writer, isolate);
  writer->json_objectend();
  profiler->current_gc_type = gc_type;
  profiler->start_time = uv_hrtime();
}

static void AfterGCCallback(Isolate* isolate,
                            v8::GCType gc_type,
                            v8::GCCallbackFlags flags,
                            void* data) {
  GCProfiler* profiler = static_cast<GCProfiler*>(data);
  if (profiler->current_gc_type != gc_type) {
    return;
  }
  JSONWriter* writer = profiler->writer();
  profiler->current_gc_type = 0;
  writer->json_keyvalue("cost", (uv_hrtime() - profiler->start_time) / 1e3);
  profiler->start_time = 0;
  writer->json_objectstart("afterGC");
  SetHeapStatistics(writer, isolate);
  writer->json_objectend();
  writer->json_end();
}

GCProfiler::GCProfiler(Environment* env, Local<Object> object)
    : BaseObject(env, object),
      start_time(0),
      current_gc_type(0),
      state(GCProfilerState::kInitialized),
      writer_(out_stream_, false) {
  MakeWeak();
}

// This function will be called when
// 1. StartGCProfile and StopGCProfile are called and
//    JS land does not keep the object anymore.
// 2. StartGCProfile is called then the env exits before
//    StopGCProfile is called.
GCProfiler::~GCProfiler() {
  if (state != GCProfiler::GCProfilerState::kInitialized) {
    env()->isolate()->RemoveGCPrologueCallback(BeforeGCCallback, this);
    env()->isolate()->RemoveGCEpilogueCallback(AfterGCCallback, this);
  }
}

JSONWriter* GCProfiler::writer() {
  return &writer_;
}

std::ostringstream* GCProfiler::out_stream() {
  return &out_stream_;
}

void GCProfiler::New(const FunctionCallbackInfo<Value>& args) {
  CHECK(args.IsConstructCall());
  Environment* env = Environment::GetCurrent(args);
  new GCProfiler(env, args.This());
}

void GCProfiler::Start(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  GCProfiler* profiler;
  ASSIGN_OR_RETURN_UNWRAP(&profiler, args.This());
  if (profiler->state != GCProfiler::GCProfilerState::kInitialized) {
    return;
  }
  profiler->writer()->json_start();
  profiler->writer()->json_keyvalue("version", 1);

  uv_timeval64_t ts;
  if (uv_gettimeofday(&ts) == 0) {
    profiler->writer()->json_keyvalue("startTime",
                                      ts.tv_sec * 1000 + ts.tv_usec / 1000);
  } else {
    profiler->writer()->json_keyvalue("startTime", 0);
  }
  profiler->writer()->json_arraystart("statistics");
  env->isolate()->AddGCPrologueCallback(BeforeGCCallback,
                                        static_cast<void*>(profiler));
  env->isolate()->AddGCEpilogueCallback(AfterGCCallback,
                                        static_cast<void*>(profiler));
  profiler->state = GCProfiler::GCProfilerState::kStarted;
}

void GCProfiler::Stop(const FunctionCallbackInfo<v8::Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  GCProfiler* profiler;
  ASSIGN_OR_RETURN_UNWRAP(&profiler, args.This());
  if (profiler->state != GCProfiler::GCProfilerState::kStarted) {
    return;
  }
  profiler->writer()->json_arrayend();
  uv_timeval64_t ts;
  if (uv_gettimeofday(&ts) == 0) {
    profiler->writer()->json_keyvalue("endTime",
                                      ts.tv_sec * 1000 + ts.tv_usec / 1000);
  } else {
    profiler->writer()->json_keyvalue("endTime", 0);
  }
  profiler->writer()->json_end();
  profiler->state = GCProfiler::GCProfilerState::kStopped;
  auto string = profiler->out_stream()->str();
  Local<Value> ret;
  if (ToV8Value(env->context(), string, env->isolate()).ToLocal(&ret)) {
    args.GetReturnValue().Set(ret);
  }
}

void GetAllocationProfile(const FunctionCallbackInfo<Value>& args) {
  Isolate* isolate = args.GetIsolate();
  // If a session generation was provided and it no longer matches the current
  // one, a newer session is live.  Return undefined without touching it.
  if (args.Length() > 0 && args[0]->IsUint32()) {
    BindingData* bd = Realm::GetBindingData<BindingData>(args);
    if (args[0].As<Uint32>()->Value() != bd->heap_profile_session_generation_)
      return;
  }
  HeapProfiler* profiler = isolate->GetHeapProfiler();
  HandleScope scope(isolate);
  Local<Context> context = isolate->GetCurrentContext();

  std::unique_ptr<AllocationProfile> profile(profiler->GetAllocationProfile());
  if (!profile) {
    return;  // Returns undefined if profiler not started
  }

  const std::vector<AllocationProfile::Sample>& samples = profile->GetSamples();
  Local<Array> js_samples = Array::New(isolate, samples.size());

#ifdef V8_HEAP_PROFILER_SAMPLE_LABELS
  // Each id is resolved once, so all samples carrying it share one frozen
  // object. The serial is its content key, used to merge externalBytes below.
  std::unordered_map<uint32_t, Local<Object>> id_to_labels;
  std::unordered_map<uint32_t, std::string> id_to_serial;
  HeapProfiler* hp = isolate->GetHeapProfiler();
  // Returns empty on a V8 error, letting the exception reach the JS caller. A
  // failed resolution is never cached.
  auto resolve_label = [&](uint32_t id) -> MaybeLocal<Object> {
    auto it = id_to_labels.find(id);
    if (it != id_to_labels.end()) return it->second;
    Local<Object> obj = Object::New(isolate);
    std::string serial;
    Local<Value> als_value;
    if (id != 0 && hp->ResolveLabelValue(id).ToLocal(&als_value) &&
        als_value->IsArray()) {
      Local<Array> flat = als_value.As<Array>();
      uint32_t len = flat->Length();
      for (uint32_t j = 0; j + 1 < len; j += 2) {
        Local<Value> k, v;
        if (!flat->Get(context, j).ToLocal(&k)) return {};
        if (!flat->Get(context, j + 1).ToLocal(&v)) return {};
        // labelsToFlat() builds this from ObjectKeys(), so a non-string key
        // cannot occur; skipping rather than failing keeps that assumption
        // from becoming a crash.
        // Values are strings too (labelsToFlat validates them); guard both so
        // the labels object and the merge serial stay in agreement, and so a
        // non-string value can never reach Utf8Value -> ToString(), which
        // could throw and leave a pending exception for later V8 calls.
        if (!k->IsString() || !v->IsString()) continue;
        // CreateDataProperty rather than Set, so that a poisoned
        // Object.prototype setter cannot run here.
        if (obj->CreateDataProperty(context, k.As<String>(), v).IsNothing())
          return {};
        node::Utf8Value ks(isolate, k), vs(isolate, v);
        if (*ks && *vs) {
          if (!serial.empty()) serial += '\0';
          serial += std::to_string(ks.length());
          serial += ':';
          serial.append(*ks, ks.length());
          serial += '\0';
          serial += std::to_string(vs.length());
          serial += ':';
          serial.append(*vs, vs.length());
        }
      }
    }
    if (obj->SetIntegrityLevel(context, v8::IntegrityLevel::kFrozen)
            .IsNothing())
      return {};
    id_to_labels.emplace(id, obj);
    id_to_serial.emplace(id, std::move(serial));
    return id_to_labels[id];
  };
#endif  // V8_HEAP_PROFILER_SAMPLE_LABELS

  for (size_t i = 0; i < samples.size(); i++) {
    const AllocationProfile::Sample& sample = samples[i];
    Local<Object> js_sample = Object::New(isolate);

    // CreateDataProperty defines own data properties directly, so a poisoned
    // Object.prototype setter for any of these key names cannot run here.
    if (js_sample->CreateDataProperty(
                       context,
                       FIXED_ONE_BYTE_STRING(isolate, "nodeId"),
                       Integer::NewFromUnsigned(isolate, sample.node_id))
            .IsNothing()) return;
    if (js_sample->CreateDataProperty(
                       context,
                       FIXED_ONE_BYTE_STRING(isolate, "size"),
                       Number::New(isolate, static_cast<double>(sample.size)))
            .IsNothing()) return;
    if (js_sample->CreateDataProperty(
                       context,
                       FIXED_ONE_BYTE_STRING(isolate, "count"),
                       Integer::NewFromUnsigned(isolate, sample.count))
            .IsNothing()) return;
    if (js_sample->CreateDataProperty(
                       context,
                       FIXED_ONE_BYTE_STRING(isolate, "sampleId"),
                       Number::New(isolate,
                                   static_cast<double>(sample.sample_id)))
            .IsNothing()) return;

#ifdef V8_HEAP_PROFILER_SAMPLE_LABELS
    // Always emitted, as a frozen empty object when no label was captured.
    Local<Object> js_labels;
    if (!resolve_label(sample.label_id).ToLocal(&js_labels)) return;
    if (js_sample->CreateDataProperty(
                       context,
                       FIXED_ONE_BYTE_STRING(isolate, "labels"),
                       js_labels).IsNothing()) return;
#endif  // V8_HEAP_PROFILER_SAMPLE_LABELS

    if (js_samples->CreateDataProperty(
            context, static_cast<uint32_t>(i), js_sample).IsNothing()) return;
  }

  Local<Object> result = Object::New(isolate);
  if (result->CreateDataProperty(context,
                  FIXED_ONE_BYTE_STRING(isolate, "samples"),
                  js_samples).IsNothing()) return;

  // Per-label external memory, as { labels, bytes } entries using the same
  // labels shape as the samples above.
  Environment* env = Environment::GetCurrent(args);
  auto* node_allocator = env->isolate_data()->node_allocator();
  auto* profiling_allocator = node_allocator != nullptr
      ? node_allocator->GetProfilingAllocator() : nullptr;
#ifdef V8_HEAP_PROFILER_SAMPLE_LABELS
  if (profiling_allocator != nullptr) {
    auto per_label = profiling_allocator->GetPerLabelBytes();
    if (!per_label.empty()) {
      // Distinct ids can carry identical label content, so merge on content.
      std::unordered_map<std::string,
                         std::pair<Local<Object>, int64_t>> by_content;
      for (const auto& [label_id, bytes] : per_label) {
        Local<Object> labels_obj;
        if (!resolve_label(label_id).ToLocal(&labels_obj)) return;
        const std::string& serial = id_to_serial[label_id];
        // An empty serial means a stale id or an ALS value with no usable
        // pairs. Such entries were dropped before labels existed, and
        // emitting them now would change the shape of the output.
        if (serial.empty()) continue;
        auto& slot = by_content[serial];
        if (slot.first.IsEmpty()) slot.first = labels_obj;
        slot.second += bytes;
      }
      std::vector<std::pair<Local<Object>, int64_t>> entries;
      entries.reserve(by_content.size());
      for (auto& [serial, cv] : by_content) {
        if (cv.second > 0) entries.emplace_back(cv.first, cv.second);
      }
      if (!entries.empty()) {
        Local<Array> js_external = Array::New(isolate, entries.size());
        for (size_t idx = 0; idx < entries.size(); idx++) {
          Local<Object> js_entry = Object::New(isolate);
          if (js_entry->CreateDataProperty(
                            context,
                            FIXED_ONE_BYTE_STRING(isolate, "labels"),
                            entries[idx].first).IsNothing()) return;
          if (js_entry->CreateDataProperty(
                  context,
                  FIXED_ONE_BYTE_STRING(isolate, "bytes"),
                  Number::New(isolate,
                              static_cast<double>(entries[idx].second)))
                  .IsNothing()) return;
          if (js_external->CreateDataProperty(
                  context, static_cast<uint32_t>(idx), js_entry)
                  .IsNothing()) return;
        }
        if (result->CreateDataProperty(context,
                        FIXED_ONE_BYTE_STRING(isolate, "externalBytes"),
                        js_external).IsNothing()) return;
      }
    }
  }
#else
  (void)profiling_allocator;
#endif  // V8_HEAP_PROFILER_SAMPLE_LABELS

  args.GetReturnValue().Set(result);
}

void Initialize(Local<Object> target,
                Local<Value> unused,
                Local<Context> context,
                void* priv) {
  Realm* realm = Realm::GetCurrent(context);
  Environment* env = realm->env();
  BindingData* const binding_data = realm->AddBindingData<BindingData>(target);
  if (binding_data == nullptr) return;

  SetMethodNoSideEffect(
      context, target, "cachedDataVersionTag", CachedDataVersionTag);
  SetMethodNoSideEffect(context,
                        target,
                        "setHeapSnapshotNearHeapLimit",
                        SetHeapSnapshotNearHeapLimit);
  SetMethod(context,
            target,
            "updateHeapStatisticsBuffer",
            UpdateHeapStatisticsBuffer);

  SetMethod(context,
            target,
            "updateHeapCodeStatisticsBuffer",
            UpdateHeapCodeStatisticsBuffer);
  SetMethodNoSideEffect(
      context, target, "getCppHeapStatistics", GetCppHeapStatistics);

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
  target
      ->Set(
          context,
          FIXED_ONE_BYTE_STRING(env->isolate(), "kHeapSpaces"),
          Array::New(env->isolate(), heap_spaces.out(), number_of_heap_spaces))
      .Check();

  SetMethod(context,
            target,
            "updateHeapSpaceStatisticsBuffer",
            UpdateHeapSpaceStatisticsBuffer);

#define V(i, _, name)                                                          \
  target                                                                       \
      ->Set(context,                                                           \
            FIXED_ONE_BYTE_STRING(env->isolate(), #name),                      \
            Uint32::NewFromUnsigned(env->isolate(), i))                        \
      .Check();

  HEAP_STATISTICS_PROPERTIES(V)
  HEAP_CODE_STATISTICS_PROPERTIES(V)
  HEAP_SPACE_STATISTICS_PROPERTIES(V)
#undef V

  // Export symbols used by v8.setFlagsFromString()
  SetMethod(context, target, "setFlagsFromString", SetFlagsFromString);

  SetMethod(context, target, "startCpuProfile", StartCpuProfile);
  SetMethod(context, target, "stopCpuProfile", StopCpuProfile);
  SetMethod(context, target, "startHeapProfile", StartHeapProfile);
  SetMethod(context, target, "stopHeapProfile", StopHeapProfile);

  {
    constexpr uint32_t kSamplingNoFlags = static_cast<uint32_t>(
        v8::HeapProfiler::SamplingFlags::kSamplingNoFlags);
    constexpr uint32_t kSamplingForceGC = static_cast<uint32_t>(
        v8::HeapProfiler::SamplingFlags::kSamplingForceGC);
    constexpr uint32_t kSamplingIncludeObjectsCollectedByMajorGC =
        static_cast<uint32_t>(v8::HeapProfiler::SamplingFlags::
                                  kSamplingIncludeObjectsCollectedByMajorGC);
    constexpr uint32_t kSamplingIncludeObjectsCollectedByMinorGC =
        static_cast<uint32_t>(v8::HeapProfiler::SamplingFlags::
                                  kSamplingIncludeObjectsCollectedByMinorGC);

    NODE_DEFINE_CONSTANT(target, kSamplingNoFlags);
    NODE_DEFINE_CONSTANT(target, kSamplingForceGC);
    NODE_DEFINE_CONSTANT(target, kSamplingIncludeObjectsCollectedByMajorGC);
    NODE_DEFINE_CONSTANT(target, kSamplingIncludeObjectsCollectedByMinorGC);
  }

  SetMethod(context, target, "getAllocationProfile",
            GetAllocationProfile);
#ifdef V8_HEAP_PROFILER_SAMPLE_LABELS
  SetMethod(context, target, "setHeapProfileLabelsStore",
            SetHeapProfileLabelsStore);
  SetMethodNoSideEffect(context, target, "getProfilingAllocatorActive",
                        GetProfilingAllocatorActive);
#endif  // V8_HEAP_PROFILER_SAMPLE_LABELS

  // Export symbols used by v8.isStringOneByteRepresentation()
  SetFastMethodNoSideEffect(context,
                            target,
                            "isStringOneByteRepresentation",
                            IsStringOneByteRepresentation,
                            &fast_is_string_one_byte_representation_);

  SetMethodNoSideEffect(context, target, "getHashSeed", GetHashSeed);

  // GCProfiler
  Local<FunctionTemplate> t =
      NewFunctionTemplate(env->isolate(), GCProfiler::New);
  t->InstanceTemplate()->SetInternalFieldCount(GCProfiler::kInternalFieldCount);
  SetProtoMethod(env->isolate(), t, "start", GCProfiler::Start);
  SetProtoMethod(env->isolate(), t, "stop", GCProfiler::Stop);
  SetConstructorFunction(context, target, "GCProfiler", t);

  {
    Isolate* isolate = env->isolate();
    Local<Object> detail_level = Object::New(isolate);
    cppgc::HeapStatistics::DetailLevel DETAILED =
        cppgc::HeapStatistics::DetailLevel::kDetailed;
    cppgc::HeapStatistics::DetailLevel BRIEF =
        cppgc::HeapStatistics::DetailLevel::kBrief;
    NODE_DEFINE_CONSTANT(detail_level, DETAILED);
    NODE_DEFINE_CONSTANT(detail_level, BRIEF);
    READONLY_PROPERTY(target, "detailLevel", detail_level);
  }
}

void RegisterExternalReferences(ExternalReferenceRegistry* registry) {
  registry->Register(CachedDataVersionTag);
  registry->Register(UpdateHeapStatisticsBuffer);
  registry->Register(UpdateHeapCodeStatisticsBuffer);
  registry->Register(UpdateHeapSpaceStatisticsBuffer);
  registry->Register(SetFlagsFromString);
  registry->Register(GetHashSeed);
  registry->Register(SetHeapSnapshotNearHeapLimit);
  registry->Register(GCProfiler::New);
  registry->Register(GCProfiler::Start);
  registry->Register(GCProfiler::Stop);
  registry->Register(GetCppHeapStatistics);
  registry->Register(IsStringOneByteRepresentation);
  registry->Register(fast_is_string_one_byte_representation_);
  registry->Register(StartCpuProfile);
  registry->Register(StopCpuProfile);
  registry->Register(StartHeapProfile);
  registry->Register(StopHeapProfile);
  registry->Register(GetAllocationProfile);
#ifdef V8_HEAP_PROFILER_SAMPLE_LABELS
  registry->Register(SetHeapProfileLabelsStore);
  registry->Register(GetProfilingAllocatorActive);
#endif  // V8_HEAP_PROFILER_SAMPLE_LABELS
}

}  // namespace v8_utils
}  // namespace node

NODE_BINDING_CONTEXT_AWARE_INTERNAL(v8, node::v8_utils::Initialize)
NODE_BINDING_EXTERNAL_REFERENCE(v8, node::v8_utils::RegisterExternalReferences)
