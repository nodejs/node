#include "tracing/trace_event_perfetto.h"

#if defined(V8_USE_PERFETTO)
PERFETTO_TRACK_EVENT_STATIC_STORAGE_IN_NAMESPACE(node);
#endif
