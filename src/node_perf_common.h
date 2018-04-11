#ifndef SRC_NODE_PERF_COMMON_H_
#define SRC_NODE_PERF_COMMON_H_

#include "node.h"
#include "v8.h"

#include <algorithm>
#include <map>
#include <string>

namespace node {
namespace performance {

#define PERFORMANCE_NOW() uv_hrtime()

// These occur before the environment is created. Cache them
// here and add them to the milestones when the env is init'd.
extern uint64_t performance_node_start;
extern uint64_t performance_v8_start;

#define NODE_PERFORMANCE_MILESTONES(V)                                        \
  V(ENVIRONMENT, "environment")                                               \
  V(NODE_START, "nodeStart")                                                  \
  V(V8_START, "v8Start")                                                      \
  V(LOOP_START, "loopStart")                                                  \
  V(LOOP_EXIT, "loopExit")                                                    \
  V(BOOTSTRAP_COMPLETE, "bootstrapComplete")                                  \
  V(THIRD_PARTY_MAIN_START, "thirdPartyMainStart")                            \
  V(THIRD_PARTY_MAIN_END, "thirdPartyMainEnd")                                \
  V(CLUSTER_SETUP_START, "clusterSetupStart")                                 \
  V(CLUSTER_SETUP_END, "clusterSetupEnd")                                     \
  V(MODULE_LOAD_START, "moduleLoadStart")                                     \
  V(MODULE_LOAD_END, "moduleLoadEnd")                                         \
  V(PRELOAD_MODULE_LOAD_START, "preloadModulesLoadStart")                     \
  V(PRELOAD_MODULE_LOAD_END, "preloadModulesLoadEnd")

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
      root) {
    for (size_t i = 0; i < milestones.Length(); i++)
      milestones[i] = -1.;
  }

  AliasedBuffer<uint8_t, v8::Uint8Array> root;
  AliasedBuffer<double, v8::Float64Array> milestones;
  AliasedBuffer<uint32_t, v8::Uint32Array> observers;

  void Mark(enum PerformanceMilestone milestone,
            uint64_t ts = PERFORMANCE_NOW());

 private:
  struct performance_state_internal {
    // doubles first so that they are always sizeof(double)-aligned
    double milestones[NODE_PERFORMANCE_MILESTONE_INVALID];
    uint32_t observers[NODE_PERFORMANCE_ENTRY_TYPE_INVALID];
  };
};

}  // namespace performance
}  // namespace node

#endif  // SRC_NODE_PERF_COMMON_H_
