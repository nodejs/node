#ifndef SRC_NODE_PERF_H_
#define SRC_NODE_PERF_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "node.h"
#include "node_perf_common.h"
#include "base_object-inl.h"
#include "histogram-inl.h"

#include "v8.h"
#include "uv.h"

#include <string>

namespace node {

class Environment;

namespace performance {

extern const uint64_t timeOrigin;

static inline const char* GetPerformanceMilestoneName(
    enum PerformanceMilestone milestone) {
  switch (milestone) {
#define V(name, label) case NODE_PERFORMANCE_MILESTONE_##name: return label;
  NODE_PERFORMANCE_MILESTONES(V)
#undef V
    default:
      UNREACHABLE();
  }
}

static inline PerformanceMilestone ToPerformanceMilestoneEnum(const char* str) {
#define V(name, label)                                                        \
  if (strcmp(str, label) == 0) return NODE_PERFORMANCE_MILESTONE_##name;
  NODE_PERFORMANCE_MILESTONES(V)
#undef V
  return NODE_PERFORMANCE_MILESTONE_INVALID;
}

static inline PerformanceEntryType ToPerformanceEntryTypeEnum(
    const char* type) {
#define V(name, label)                                                        \
  if (strcmp(type, label) == 0) return NODE_PERFORMANCE_ENTRY_TYPE_##name;
  NODE_PERFORMANCE_ENTRY_TYPES(V)
#undef V
  return NODE_PERFORMANCE_ENTRY_TYPE_INVALID;
}

class PerformanceEntry {
 public:
  static void Notify(Environment* env,
                     PerformanceEntryType type,
                     v8::Local<v8::Value> object);

  static void New(const v8::FunctionCallbackInfo<v8::Value>& args);

  PerformanceEntry(Environment* env,
                   const char* name,
                   const char* type,
                   uint64_t startTime,
                   uint64_t endTime) : env_(env),
                                       name_(name),
                                       type_(type),
                                       startTime_(startTime),
                                       endTime_(endTime) { }

  virtual ~PerformanceEntry() = default;

  virtual v8::MaybeLocal<v8::Object> ToObject() const;

  Environment* env() const { return env_; }

  const std::string& name() const { return name_; }

  const std::string& type() const { return type_; }

  PerformanceEntryType kind() {
    return ToPerformanceEntryTypeEnum(type().c_str());
  }

  double startTime() const { return startTimeNano() / 1e6; }

  double duration() const { return durationNano() / 1e6; }

  uint64_t startTimeNano() const { return startTime_ - timeOrigin; }

  uint64_t durationNano() const { return endTime_ - startTime_; }

 private:
  Environment* env_;
  const std::string name_;
  const std::string type_;
  const uint64_t startTime_;
  const uint64_t endTime_;
};

enum PerformanceGCKind {
  NODE_PERFORMANCE_GC_MAJOR = v8::GCType::kGCTypeMarkSweepCompact,
  NODE_PERFORMANCE_GC_MINOR = v8::GCType::kGCTypeScavenge,
  NODE_PERFORMANCE_GC_INCREMENTAL = v8::GCType::kGCTypeIncrementalMarking,
  NODE_PERFORMANCE_GC_WEAKCB = v8::GCType::kGCTypeProcessWeakCallbacks
};

enum PerformanceGCFlags {
  NODE_PERFORMANCE_GC_FLAGS_NO =
    v8::GCCallbackFlags::kNoGCCallbackFlags,
  NODE_PERFORMANCE_GC_FLAGS_CONSTRUCT_RETAINED =
    v8::GCCallbackFlags::kGCCallbackFlagConstructRetainedObjectInfos,
  NODE_PERFORMANCE_GC_FLAGS_FORCED =
    v8::GCCallbackFlags::kGCCallbackFlagForced,
  NODE_PERFORMANCE_GC_FLAGS_SYNCHRONOUS_PHANTOM_PROCESSING =
    v8::GCCallbackFlags::kGCCallbackFlagSynchronousPhantomCallbackProcessing,
  NODE_PERFORMANCE_GC_FLAGS_ALL_AVAILABLE_GARBAGE =
    v8::GCCallbackFlags::kGCCallbackFlagCollectAllAvailableGarbage,
  NODE_PERFORMANCE_GC_FLAGS_ALL_EXTERNAL_MEMORY =
    v8::GCCallbackFlags::kGCCallbackFlagCollectAllExternalMemory,
  NODE_PERFORMANCE_GC_FLAGS_SCHEDULE_IDLE =
    v8::GCCallbackFlags::kGCCallbackScheduleIdleGarbageCollection
};

class GCPerformanceEntry : public PerformanceEntry {
 public:
  GCPerformanceEntry(Environment* env,
                     PerformanceGCKind gckind,
                     PerformanceGCFlags gcflags,
                     uint64_t startTime,
                     uint64_t endTime) :
                         PerformanceEntry(env, "gc", "gc", startTime, endTime),
                         gckind_(gckind),
                         gcflags_(gcflags) { }

  PerformanceGCKind gckind() const { return gckind_; }
  PerformanceGCFlags gcflags() const { return gcflags_; }

 private:
  PerformanceGCKind gckind_;
  PerformanceGCFlags gcflags_;
};

class ELDHistogram : public HandleWrap, public Histogram {
 public:
  ELDHistogram(Environment* env,
               v8::Local<v8::Object> wrap,
               int32_t resolution);

  bool RecordDelta();
  bool Enable();
  bool Disable();
  void ResetState() {
    Reset();
    exceeds_ = 0;
    prev_ = 0;
  }
  int64_t Exceeds() const { return exceeds_; }

  void MemoryInfo(MemoryTracker* tracker) const override {
    tracker->TrackFieldWithSize("histogram", GetMemorySize());
  }

  SET_MEMORY_INFO_NAME(ELDHistogram)
  SET_SELF_SIZE(ELDHistogram)

 private:
  static void DelayIntervalCallback(uv_timer_t* req);

  bool enabled_ = false;
  int32_t resolution_ = 0;
  int64_t exceeds_ = 0;
  uint64_t prev_ = 0;
  uv_timer_t timer_;
};

}  // namespace performance
}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_NODE_PERF_H_
