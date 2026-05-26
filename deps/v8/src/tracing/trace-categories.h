// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_TRACING_TRACE_CATEGORIES_H_
#define V8_TRACING_TRACE_CATEGORIES_H_

#include "src/base/macros.h"
#include "src/base/platform/time.h"
#include "src/tracing/perfetto-sdk.h"

#if defined(V8_USE_PERFETTO)

// Trace category prefixes used in tests.
PERFETTO_DEFINE_TEST_CATEGORY_PREFIXES("v8-cat", "cat", "v8.Test2");

// List of categories used by built-in V8 trace events.
PERFETTO_DEFINE_CATEGORIES_IN_NAMESPACE_WITH_ATTRS(
    v8, V8_EXPORT_PRIVATE, perfetto::Category("cppgc"),
    perfetto::Category("v8"), perfetto::Category("v8.console"),
    perfetto::Category("v8.execute"),
    perfetto::Category("v8.memory")
        .SetDescription("Emits counters related to v8 memory usage."),
    perfetto::Category("v8.wasm"),
    perfetto::Category::Group("devtools.timeline,v8"),
    perfetto::Category::Group(
        "devtools.timeline," TRACE_DISABLED_BY_DEFAULT("v8.gc")),
    perfetto::Category(TRACE_DISABLED_BY_DEFAULT("cppgc")).SetTags("slow"),
    perfetto::Category(TRACE_DISABLED_BY_DEFAULT("devtools.timeline"))
        .SetTags("slow"),
    perfetto::Category(TRACE_DISABLED_BY_DEFAULT("devtools.v8-source-rundown"))
        .SetTags("slow"),
    perfetto::Category(
        TRACE_DISABLED_BY_DEFAULT("devtools.v8-source-rundown-sources"))
        .SetTags("slow"),
    perfetto::Category(TRACE_DISABLED_BY_DEFAULT("v8")).SetTags("slow"),
    perfetto::Category(TRACE_DISABLED_BY_DEFAULT("v8.compile")).SetTags("slow"),
    perfetto::Category(TRACE_DISABLED_BY_DEFAULT("v8.cpu_profiler"))
        .SetTags("slow"),
    perfetto::Category(TRACE_DISABLED_BY_DEFAULT("v8.gc")).SetTags("slow"),
    perfetto::Category(TRACE_DISABLED_BY_DEFAULT("v8.gc_stats"))
        .SetTags("slow"),
    perfetto::Category(TRACE_DISABLED_BY_DEFAULT("v8.inspector"))
        .SetTags("slow"),
    perfetto::Category(TRACE_DISABLED_BY_DEFAULT("v8.ic_stats"))
        .SetTags("slow"),
    perfetto::Category(TRACE_DISABLED_BY_DEFAULT("v8.maglev")).SetTags("slow"),
    perfetto::Category(TRACE_DISABLED_BY_DEFAULT("v8.runtime")).SetTags("slow"),
    perfetto::Category(TRACE_DISABLED_BY_DEFAULT("v8.runtime_stats"))
        .SetTags("slow"),
    perfetto::Category(TRACE_DISABLED_BY_DEFAULT("v8.runtime_stats_sampling"))
        .SetTags("slow"),
    perfetto::Category(TRACE_DISABLED_BY_DEFAULT("v8.stack_trace"))
        .SetTags("slow"),
    perfetto::Category(TRACE_DISABLED_BY_DEFAULT("v8.turbofan"))
        .SetTags("slow"),
    perfetto::Category(TRACE_DISABLED_BY_DEFAULT("v8.wasm.detailed"))
        .SetTags("slow"),
    perfetto::Category(TRACE_DISABLED_BY_DEFAULT("v8.wasm.turbofan"))
        .SetTags("slow"),
    perfetto::Category(TRACE_DISABLED_BY_DEFAULT("v8.zone_stats"))
        .SetTags("slow"),
    perfetto::Category::Group("v8,devtools.timeline"),
    perfetto::Category::Group(TRACE_DISABLED_BY_DEFAULT(
        "v8.turbofan") "," TRACE_DISABLED_BY_DEFAULT("v8.wasm.turbofan")),
    perfetto::Category::Group(TRACE_DISABLED_BY_DEFAULT(
        "v8.inspector") "," TRACE_DISABLED_BY_DEFAULT("v8.stack_trace")));

PERFETTO_USE_CATEGORIES_FROM_NAMESPACE(v8);

namespace perfetto {

template <>
struct V8_EXPORT_PRIVATE TraceTimestampTraits<::v8::base::TimeTicks> {
  static TraceTimestamp ConvertTimestampToTraceTimeNs(
      const ::v8::base::TimeTicks& ticks);
};

}  // namespace perfetto

#endif  // defined(V8_USE_PERFETTO)

#endif  // V8_TRACING_TRACE_CATEGORIES_H_
