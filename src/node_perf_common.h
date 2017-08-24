#ifndef SRC_NODE_PERF_COMMON_H_
#define SRC_NODE_PERF_COMMON_H_

#include "node.h"
#include "v8.h"

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
  V(FUNCTION, "function")

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

#define PERFORMANCE_MARK(env, n)                                              \
  do {                                                                        \
    node::performance::MarkPerformanceMilestone(env,                          \
                         node::performance::NODE_PERFORMANCE_MILESTONE_##n);  \
  } while (0);

struct performance_state {
  // doubles first so that they are always sizeof(double)-aligned
  double milestones[NODE_PERFORMANCE_MILESTONE_INVALID];
  uint32_t observers[NODE_PERFORMANCE_ENTRY_TYPE_INVALID];
};

}  // namespace performance
}  // namespace node

#endif  // SRC_NODE_PERF_COMMON_H_
