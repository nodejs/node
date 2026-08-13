#ifndef SRC_TRACING_TRACE_EVENT_H_
#define SRC_TRACING_TRACE_EVENT_H_

#include "tracing/agent.h"

#if defined(V8_USE_PERFETTO)

#define TRACING_CATEGORY_NODE "node"
#define TRACING_CATEGORY_NODE1(one) TRACING_CATEGORY_NODE "." #one
#define TRACING_CATEGORY_NODE2(one, two) TRACING_CATEGORY_NODE "." #one "." #two

#include "tracing/trace_event_perfetto.h"

#else  // defined(V8_USE_PERFETTO)

#define TRACING_CATEGORY_NODE "node"
#define TRACING_CATEGORY_NODE1(one)                                            \
  TRACING_CATEGORY_NODE "," TRACING_CATEGORY_NODE "." #one
#define TRACING_CATEGORY_NODE2(one, two)                                       \
  TRACING_CATEGORY_NODE "," TRACING_CATEGORY_NODE "." #one                     \
                        "," TRACING_CATEGORY_NODE "." #one "." #two

#include "tracing/trace_event_legacy_inl.h"

#endif

#endif  // SRC_TRACING_TRACE_EVENT_H_
