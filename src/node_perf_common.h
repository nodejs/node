#ifndef SRC_NODE_PERF_COMMON_H_
#define SRC_NODE_PERF_COMMON_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "node.h"
#include "uv.h"
#include "v8.h"
#include "histogram-inl.h"

#include <algorithm>
#include <map>
#include <string>

namespace node {
namespace performance {

#define PERFORMANCE_NOW() uv_hrtime()

// These occur before the environment is created. Cache them
// here and add them to the milestones when the env is init'd.
extern uint64_t performance_v8_start;

#define NODE_PERFORMANCE_MILESTONES(V)                                        \
  V(ENVIRONMENT, "environment")                                               \
  V(NODE_START, "nodeStart")                                                  \
  V(V8_START, "v8Start")                                                      \
  V(LOOP_START, "loopStart")                                                  \
  V(LOOP_EXIT, "loopExit")                                                    \
  V(BOOTSTRAP_COMPLETE, "bootstrapComplete")


#define NODE_PERFORMANCE_ENTRY_TYPES(V)                                       \
  V(NODE, "node")                                                             \
  V(MARK, "mark")                                                             \
  V(MEASURE, "measure")                                                       \
  V(GC, "gc")                                                                 \
  V(FUNCTION, "function")                                                     \
  V(HTTP2, "http2")

enum PerformanceMilestone {
#define V(name, _) NODE_PERFORMANCE_MILESTONE_##name,
  NODE_PERFORMANCE_MILESTONES(V)
#undef V
  NODE_PERFORMANCE_MILESTONE_INVALID
};

enum PerformanceEntryType {
#define V(name, _) NODE_PERFORMANCE_ENTRY_TYPE_##name,
  NODE_PERFORMANCE_ENTRY_TYPES(V)
#undef V
  NODE_PERFORMANCE_ENTRY_TYPE_INVALID
};

class performance_state {
 public:
  explicit performance_state(v8::Isolate* isolate) :
    root(
      isolate,
      sizeof(performance_state_internal)),
    milestones(
      isolate,
      offsetof(performance_state_internal, milestones),
      NODE_PERFORMANCE_MILESTONE_INVALID,
      root),
    observers(
      isolate,
      offsetof(performance_state_internal, observers),
      NODE_PERFORMANCE_ENTRY_TYPE_INVALID,
      root),
    total_memory_(uv_get_total_memory()),
    rss_histogram_(1, total_memory_),
    total_heap_size_histogram_(1, total_memory_),
    used_heap_size_histogram_(1, total_memory_),
    external_memory_histogram_(1, total_memory_),
    rss_exceeds_(0),
    total_heap_size_exceeds_(0),
    used_heap_size_exceeds_(0),
    external_memory_exceeds_(0) {
    for (size_t i = 0; i < milestones.Length(); i++)
      milestones[i] = -1.;
  }

  AliasedBuffer<uint8_t, v8::Uint8Array> root;
  AliasedBuffer<double, v8::Float64Array> milestones;
  AliasedBuffer<uint32_t, v8::Uint32Array> observers;

  uint64_t performance_last_gc_start_mark = 0;

  void Mark(enum PerformanceMilestone milestone,
            uint64_t ts = PERFORMANCE_NOW());

  void RecordHeapStatistics(v8::Isolate* isolate) {
    size_t rss;
    int err = uv_resident_set_memory(&rss);
    if (!err) {
      if (!rss_histogram_.Record(rss) && rss_exceeds_ < 0xFFFFFFFF)
        rss_exceeds_++;
    }

    v8::HeapStatistics v8_heap_stats;
    isolate->GetHeapStatistics(&v8_heap_stats);

    if (!total_heap_size_histogram_.Record(v8_heap_stats.total_heap_size()) &&
        total_heap_size_exceeds_ < 0xFFFFFFFF) {
      total_heap_size_exceeds_++;
    }
    if (!used_heap_size_histogram_.Record(v8_heap_stats.used_heap_size()) &&
        used_heap_size_exceeds_ < 0xFFFFFFFF) {
      used_heap_size_exceeds_++;
    }
    if (!external_memory_histogram_.Record(v8_heap_stats.external_memory()) &&
        external_memory_exceeds_ < 0xFFFFFFFF) {
      external_memory_exceeds_++;
    }
  }

  void ResetHeapStatistics() {
    rss_histogram_.Reset();
    total_heap_size_histogram_.Reset();
    used_heap_size_histogram_.Reset();
    external_memory_histogram_.Reset();
    rss_exceeds_ = 0;
    total_heap_size_exceeds_ = 0;
    used_heap_size_exceeds_ = 0;
    external_memory_exceeds_ = 0;
  }

  Histogram* rss_histogram() {
    return &rss_histogram_;
  }
  Histogram* total_heap_size_histogram() {
    return &total_heap_size_histogram_;
  }
  Histogram* used_heap_size_histogram() {
    return &used_heap_size_histogram_;
  }
  Histogram* external_memory_histogram() {
    return &external_memory_histogram_;
  }

  int64_t rss_exceeds() { return rss_exceeds_; }
  int64_t total_heap_size_exceeds() { return total_heap_size_exceeds_; }
  int64_t used_heap_size_exceeds() { return used_heap_size_exceeds_; }
  int64_t external_memory_exceeds() { return external_memory_exceeds_; }

 private:
  struct performance_state_internal {
    // doubles first so that they are always sizeof(double)-aligned
    double milestones[NODE_PERFORMANCE_MILESTONE_INVALID];
    uint32_t observers[NODE_PERFORMANCE_ENTRY_TYPE_INVALID];
  };
  int64_t total_memory_;
  Histogram rss_histogram_;
  Histogram total_heap_size_histogram_;
  Histogram used_heap_size_histogram_;
  Histogram external_memory_histogram_;
  int64_t rss_exceeds_;
  int64_t total_heap_size_exceeds_;
  int64_t used_heap_size_exceeds_;
  int64_t external_memory_exceeds_;
};

}  // namespace performance
}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_NODE_PERF_COMMON_H_
