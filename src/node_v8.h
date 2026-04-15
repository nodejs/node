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

  // Reference to the JS AsyncLocalStorage instance used by
  // withHeapProfileLabels/setHeapProfileLabels. The V8 callback uses this
  // as the key to look up label values in the stored CPED (AsyncContextFrame
  // Map) at profile-read time.
  v8::Global<v8::Value> heap_profile_labels_als_key;

  // Typed pointer to HeapProfilingCleanup state registered with
  // AddCleanupHook when profiling is active.
  //
  // Lifetime contract — three paths may trigger cleanup, all on the
  // main thread:
  //
  //  1. StopSamplingHeapProfiler (explicit user call):
  //     Calls DoCleanup(), removes the env cleanup hook, deletes
  //     the struct, and nulls this pointer.
  //
  //  2. ~BindingData (Realm teardown — runs BEFORE env cleanup hooks):
  //     Calls DoCleanup(), removes the env cleanup hook, deletes
  //     the struct, and nulls this pointer.
  //
  //  3. CleanupHeapProfiling (env cleanup hook):
  //     Fires only if neither (1) nor (2) removed the hook first.
  //     Calls DoCleanup() and deletes the struct.
  //
  // DoCleanup() is idempotent (guarded by cleaned_up flag), so
  // multiple paths calling it is safe.  The first path that runs
  // removes the hook and deletes the struct; subsequent paths see
  // nullptr and skip.
  HeapProfilingCleanup* heap_profiling_cleanup_ = nullptr;

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
