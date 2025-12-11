// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_LIBPLATFORM_TRACING_RECORDER_MAC_H_
#define V8_LIBPLATFORM_TRACING_RECORDER_MAC_H_

#include "src/libplatform/tracing/recorder.h"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunguarded-availability"

namespace v8 {
namespace platform {
namespace tracing {

Recorder::Recorder() { v8Provider = os_log_create("v8", ""); }
Recorder::~Recorder() {}

bool Recorder::IsEnabled() {
  return os_log_type_enabled(v8Provider, OS_LOG_TYPE_DEFAULT);
}
bool Recorder::IsEnabled(const uint8_t level) {
  if (level == OS_LOG_TYPE_DEFAULT || level == OS_LOG_TYPE_INFO ||
      level == OS_LOG_TYPE_DEBUG || level == OS_LOG_TYPE_ERROR ||
      level == OS_LOG_TYPE_FAULT) {
    return os_log_type_enabled(v8Provider, static_cast<os_log_type_t>(level));
  }
  return false;
}

void Recorder::AddEvent(TraceObject* trace_event) {
  os_signpost_event_emit(v8Provider, OS_SIGNPOST_ID_EXCLUSIVE, "",
                         "%s, cpu_duration: %d", trace_event->name(),
                         static_cast<int>(trace_event->cpu_duration()));
}

}  // namespace tracing
}  // namespace platform
}  // namespace v8

#pragma clang diagnostic pop

#endif  // V8_LIBPLATFORM_TRACING_RECORDER_MAC_H_
