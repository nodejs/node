#ifndef SRC_NODE_V8_H_
#define SRC_NODE_V8_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include <sstream>
#include <string>
#include "aliased_buffer.h"
#include "base_object.h"
#include "json_utils.h"
#include "node_errors.h"
#include "node_snapshotable.h"
#include "util.h"
#include "v8.h"

namespace node {
class Environment;
struct InternalFieldInfoBase;

namespace v8_utils {

struct HeapProfilingCleanup;

class BindingData : public SnapshotableObject {
 public:
  struct InternalFieldInfo : public node::InternalFieldInfoBase {
    AliasedBufferIndex heap_statistics_buffer;
    AliasedBufferIndex heap_space_statistics_buffer;
    AliasedBufferIndex heap_code_statistics_buffer;
  };
  BindingData(Realm* realm,
              v8::Local<v8::Object> obj,
              InternalFieldInfo* info = nullptr);
  ~BindingData() override;

  SERIALIZABLE_OBJECT_METHODS()
  SET_BINDING_ID(v8_binding_data)

  AliasedFloat64Array heap_statistics_buffer;
  AliasedFloat64Array heap_space_statistics_buffer;
  AliasedFloat64Array heap_code_statistics_buffer;

  // The AsyncLocalStorage instance behind withHeapProfileLabels, used as the
  // key under which V8 finds each sample's labels in the CPED.
  v8::Global<v8::Value> heap_profile_labels_als_key;

  // Ownership sentinel: non-null only when this binding started a labels:true
  // V8 sampling session.  labels:false sessions leave this null (matching
  // upstream).  StartHeapProfile throws ERR_HEAP_PROFILE_HAVE_BEEN_STARTED
  // when V8 returns false, so this pointer is set only after a successful
  // start.  StopHeapProfile calls SerializeHeapProfile unconditionally (which
  // stops V8's sampler); this pointer guards only the Node-side DoCleanup
  // teardown (profiling allocator, cleanup hook).  Three main-thread paths
  // can tear this down: StopHeapProfile, ~BindingData at realm teardown, and
  // the CleanupHeapProfiling env hook if neither of the first two got there
  // first.  Whichever runs first removes the hook, deletes the struct and
  // nulls this; DoCleanup() is idempotent.
  HeapProfilingCleanup* heap_profiling_cleanup_ = nullptr;

  // Monotonically increasing counter bumped on every successful
  // StartSamplingHeapProfiler call.  Each SyncHeapProfileHandle captures the
  // value at construction; a mismatch means a different session is now live
  // and the handle must not touch it.
  uint32_t heap_profile_session_generation_ = 0;

  void MemoryInfo(MemoryTracker* tracker) const override;
  SET_SELF_SIZE(BindingData)
  SET_MEMORY_INFO_NAME(BindingData)

 private:
  InternalFieldInfo* internal_field_info_ = nullptr;
};

class GCProfiler : public BaseObject {
 public:
  enum class GCProfilerState { kInitialized, kStarted, kStopped };
  GCProfiler(Environment* env, v8::Local<v8::Object> object);
  inline ~GCProfiler() override;
  static void New(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Start(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Stop(const v8::FunctionCallbackInfo<v8::Value>& args);

  JSONWriter* writer();

  std::ostringstream* out_stream();

  SET_NO_MEMORY_INFO()
  SET_MEMORY_INFO_NAME(GCProfiler)
  SET_SELF_SIZE(GCProfiler)

  uint64_t start_time;
  uint8_t current_gc_type;
  GCProfilerState state;

 private:
  std::ostringstream out_stream_;
  JSONWriter writer_;
};

}  // namespace v8_utils

}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_NODE_V8_H_
