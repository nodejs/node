#ifndef SRC_NODE_CPU_PROFILER_H_
#define SRC_NODE_CPU_PROFILER_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include <atomic>
#include <cstdint>
#include <memory>
#include <vector>

#include "base_object.h"
#include "v8-profiler.h"
#include "v8.h"

namespace node {

class Environment;
class ExternalReferenceRegistry;

namespace cpu_profiler {

// Holds per-profile-session state: the buffer of sampled context references
// and the counters reported back to JS.
//
// Lifetime: created by Start() under the JS thread, written into by the
// extractor running in CPU-profiler signal-handler context (also on the JS
// thread, interrupting it), drained and destroyed by Stop() under the JS
// thread after v8::CpuProfiler::Stop() has joined the sampling thread. The
// signal-handler / Stop() race is benign because both run on the same thread
// and a signal handler runs to completion before the interrupted thread
// resumes.
struct Session {
  explicit Session(size_t buffer_size);
  ~Session() = default;
  Session(const Session&) = delete;
  Session& operator=(const Session&) = delete;

  // Pre-allocated, non-wrapping. Slots are written by the extractor and read
  // by Stop() on the main thread.
  std::vector<std::shared_ptr<v8::Global<v8::Value>>> context_buffer;

  // Next slot index the extractor will write to. When >= context_buffer.size()
  // the extractor bumps `dropped` and returns nullptr instead of capturing.
  std::atomic<uint64_t> next_index{0};
  std::atomic<uint64_t> dropped{0};
};

// SampledCpuProfiler is the C++ side of the JS-facing API. It owns the
// v8::CpuProfiler, the active Session, and the ContextHolder ObjectTemplate.
// Lifecycle is driven from JS: new() creates one, start()/stop() manage the
// active Session.
class SampledCpuProfiler : public BaseObject {
 public:
  static void Initialize(v8::Local<v8::Object> target,
                         v8::Local<v8::Value> unused,
                         v8::Local<v8::Context> context,
                         void* priv);
  static void RegisterExternalReferences(ExternalReferenceRegistry* registry);

  SET_NO_MEMORY_INFO()
  SET_MEMORY_INFO_NAME(SampledCpuProfiler)
  SET_SELF_SIZE(SampledCpuProfiler)

  // Signal-safe extractor. Installed on Isolate via the SampleContextExtractor
  // hook. Walks CPED -> AsyncContextFrame -> OrderedHashMap to find the
  // current ContextHolder, captures a strong ref into the Session's context
  // buffer, and returns the slot index packed as a void*.
  static void* ExtractContext(v8::Isolate* isolate);

  // GC prologue/epilogue callbacks installed on the isolate at Start. The
  // prologue captures v8::LookupCpedMapAlignedPointer's result while the heap
  // is still consistent and stashes it on the binding; the extractor consults
  // it during GC instead of walking the heap.
  static void OnGCPrologue(v8::Isolate* isolate,
                           v8::GCType type,
                           v8::GCCallbackFlags flags,
                           void* data);
  static void OnGCEpilogue(v8::Isolate* isolate,
                           v8::GCType type,
                           v8::GCCallbackFlags flags,
                           void* data);

  // Public so BaseObject's weak callback can `delete this`. Cleans up an
  // active profile session if the JS handle was dropped without calling
  // stop()/stopAndCapture(); disposes the underlying v8::CpuProfiler.
  ~SampledCpuProfiler();

 private:
  SampledCpuProfiler(Environment* env,
                     v8::Local<v8::Object> object,
                     v8::Local<v8::ObjectTemplate> holder_template,
                     v8::Local<v8::Value> als_resource_key,
                     size_t context_buffer_size,
                     int sampling_interval_us,
                     uint32_t max_samples,
                     bool with_context);

  // JS-facing entry points.
  static void New(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Start(const v8::FunctionCallbackInfo<v8::Value>& args);
  // Stops V8's CpuProfiler, serializes the resulting profile to JSON via
  // CpuProfile::Serialize, and returns the string. This is the original
  // v8.startCpuProfile().handle.stop() behavior.
  static void Stop(const v8::FunctionCallbackInfo<v8::Value>& args);
  // Stops V8's CpuProfiler and returns a structured object tree
  // (functionName, scriptName, lineNumber, columnNumber, hitCount, contexts,
  // children).
  static void StopAndCapture(const v8::FunctionCallbackInfo<v8::Value>& args);
  // Stops V8's CpuProfiler, atomically swaps to a fresh Session, restarts the
  // CpuProfiler so sampling continues with minimal gap, and returns the tree
  // for the just-finished session. Used for continuous profiling.
  static void Snapshot(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void CreateContextHolder(
      const v8::FunctionCallbackInfo<v8::Value>& args);

  // Helper for StopJson / StopAndCapture / Snapshot. Stops the active V8
  // profile, atomically detaches the Session from the extractor's view (so
  // signal-handler invocations bail). Returns the V8 profile pointer (caller
  // owns; must Delete()) and the detached Session (caller owns; must delete).
  // If `restart` is true, also allocates a fresh Session and restarts V8.
  // Returns false if the profiler was not running.
  bool StopActiveSession(bool restart,
                         v8::CpuProfile** out_profile,
                         Session** out_session);

  // Builds and returns the structured tree for the given profile + session
  // pair. Frees both before returning.
  v8::Local<v8::Object> BuildAndFree(v8::CpuProfile* profile, Session* session);

  v8::CpuProfiler* cpu_profiler_ = nullptr;

  // Owner of the current Session. Reads from the signal handler use the
  // atomic; this struct is destroyed only after v8::CpuProfiler::Stop() has
  // joined the sampling thread, so no extractor invocation can outlive it.
  std::atomic<Session*> active_session_{nullptr};

  v8::Global<v8::ObjectTemplate> holder_template_;
  // The key under which the current ContextHolder lives in the
  // AsyncContextFrame map (typically the AsyncLocalStorage instance itself).
  v8::Global<v8::Value> als_resource_key_;

  // Pointer to the persistent-handle slot inside `als_resource_key_` that
  // holds the ALS key's tagged pointer. The slot's address is stable for the
  // Global<>'s lifetime; V8 keeps the slot's contents GC-coherently updated
  // during compaction. The signal handler dereferences this pointer at each
  // sample to obtain the current tagged address of the ALS key, then hands
  // it to v8::LookupCpedMapAlignedPointer. Set once at construction.
  uintptr_t* als_key_slot_ = nullptr;

  size_t context_buffer_size_;
  int sampling_interval_us_;
  uint32_t max_samples_;
  bool with_context_;
  v8::ProfilerId profile_id_ = 0;

  // GC-safety state. v8::LookupCpedMapAlignedPointer walks V8 heap structures
  // and is unsafe to invoke during GC. We install GC prologue/epilogue
  // callbacks at Start: the prologue calls the helper while the heap is still
  // consistent and stashes the result in `gc_cached_aligned_ptr_`; the
  // epilogue clears it. The signal-handler extractor consults `in_gc_` and
  // sources from the cache rather than calling the helper while GC is active.
  std::atomic<bool> in_gc_{false};
  std::atomic<void*> gc_cached_aligned_ptr_{nullptr};
};

}  // namespace cpu_profiler
}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_NODE_CPU_PROFILER_H_
