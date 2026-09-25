// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/tracing/trace-categories.h"

#include "include/v8-trace-categories.h"

#if defined(V8_USE_PERFETTO)
PERFETTO_TRACK_EVENT_STATIC_STORAGE_IN_NAMESPACE_WITH_ATTRS(v8,
                                                            V8_EXPORT_PRIVATE);

namespace v8 {

const perfetto::internal::TrackEventCategoryRegistry&
GetTrackEventCategoryRegistry() {
  return v8::perfetto_track_event::internal::kCategoryRegistry;
}

}  // namespace v8

namespace perfetto {

TraceTimestamp
TraceTimestampTraits<::v8::base::TimeTicks>::ConvertTimestampToTraceTimeNs(
    const ::v8::base::TimeTicks& ticks) {
  return {static_cast<uint32_t>(::v8::TrackEvent::GetTraceClockId()),
          static_cast<uint64_t>(ticks.since_origin().InNanoseconds())};
}

}  // namespace perfetto

#endif
