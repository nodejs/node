#ifndef SRC_NODE_PERF_H_
#define SRC_NODE_PERF_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "node.h"
#include "node_perf_common.h"
#include "env.h"
#include "base-object-inl.h"

#include "v8.h"
#include "uv.h"

#include <string>

namespace node {
namespace performance {

using v8::FunctionCallbackInfo;
using v8::GCType;
using v8::Local;
using v8::Object;
using v8::Value;

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

NODE_EXTERN inline void MarkPerformanceMilestone(
    Environment* env,
    PerformanceMilestone milestone) {
    env->performance_state()->milestones[milestone] = PERFORMANCE_NOW();
  }

class PerformanceEntry : public BaseObject {
 public:
  // Used for temporary storage of performance entry details when the
  // object cannot be created immediately.
  class Data {
   public:
    Data(
      Environment* env,
      const char* name,
      const char* type,
      uint64_t startTime,
      uint64_t endTime,
      int data = 0) :
      env_(env),
      name_(name),
      type_(type),
      startTime_(startTime),
      endTime_(endTime),
      data_(data) {}

    Environment* env() const {
      return env_;
    }

    const std::string& name() const {
      return name_;
    }

    const std::string& type() const {
      return type_;
    }

    uint64_t startTime() const {
      return startTime_;
    }

    uint64_t endTime() const {
      return endTime_;
    }

    int data() const {
      return data_;
    }

   private:
    Environment* const env_;
    const std::string name_;
    const std::string type_;
    const uint64_t startTime_;
    const uint64_t endTime_;
    const int data_;
  };

  static void NotifyObservers(Environment* env, PerformanceEntry* entry);

  static void New(const FunctionCallbackInfo<Value>& args);

  PerformanceEntry(Environment* env,
                   Local<Object> wrap,
                   const char* name,
                   const char* type,
                   uint64_t startTime,
                   uint64_t endTime) :
                   BaseObject(env, wrap),
                   name_(name),
                   type_(type),
                   startTime_(startTime),
                   endTime_(endTime) {
    MakeWeak<PerformanceEntry>(this);
    NotifyObservers(env, this);
  }

  PerformanceEntry(Environment* env,
                   Local<Object> wrap,
                   Data* data) :
                   BaseObject(env, wrap),
                   name_(data->name()),
                   type_(data->type()),
                   startTime_(data->startTime()),
                   endTime_(data->endTime()) {
    MakeWeak<PerformanceEntry>(this);
    NotifyObservers(env, this);
  }

  ~PerformanceEntry() {}

  const std::string& name() const {
    return name_;
  }

  const std::string& type() const {
    return type_;
  }

  double startTime() const {
    return startTime_ / 1e6;
  }

  double duration() const {
    return durationNano() / 1e6;
  }

  uint64_t startTimeNano() const {
    return startTime_;
  }

  uint64_t durationNano() const {
    return endTime_ - startTime_;
  }

 private:
  const std::string name_;
  const std::string type_;
  const uint64_t startTime_;
  const uint64_t endTime_;
};

enum PerformanceGCKind {
  NODE_PERFORMANCE_GC_MAJOR = GCType::kGCTypeMarkSweepCompact,
  NODE_PERFORMANCE_GC_MINOR = GCType::kGCTypeScavenge,
  NODE_PERFORMANCE_GC_INCREMENTAL = GCType::kGCTypeIncrementalMarking,
  NODE_PERFORMANCE_GC_WEAKCB = GCType::kGCTypeProcessWeakCallbacks
};

}  // namespace performance
}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_NODE_PERF_H_
