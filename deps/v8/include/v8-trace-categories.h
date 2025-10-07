// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef INCLUDE_V8_TRACE_CATEGORIES_H_
#define INCLUDE_V8_TRACE_CATEGORIES_H_

#include "v8config.h"

#if defined(V8_USE_PERFETTO)

#include "perfetto/tracing/track_event.h"

namespace v8 {

// Returns the perfeto TrackEventCategoryRegistry for v8 tracing categories.
V8_EXPORT const perfetto::internal::TrackEventCategoryRegistry&
GetTrackEventCategoryRegistry();

}  // namespace v8

#endif  // defined(V8_USE_PERFETTO)

#endif  // INCLUDE_V8_TRACE_CATEGORIES_H_
