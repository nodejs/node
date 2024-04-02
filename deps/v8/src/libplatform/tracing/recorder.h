// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_LIBPLATFORM_TRACING_RECORDER_H_
#define V8_LIBPLATFORM_TRACING_RECORDER_H_

#include <stdint.h>

#include "include/libplatform/v8-tracing.h"

#if !defined(V8_ENABLE_SYSTEM_INSTRUMENTATION)
#error "only include this file if V8_ENABLE_SYSTEM_INSTRUMENTATION"
#endif

#if V8_OS_DARWIN
#include <os/signpost.h>
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunguarded-availability"
#endif

#if V8_OS_WIN
#ifndef V8_ETW_GUID
#define V8_ETW_GUID \
  0x57277741, 0x3638, 0x4A4B, 0xBD, 0xBA, 0x0A, 0xC6, 0xE4, 0x5D, 0xA5, 0x6C
#endif
#endif

namespace v8 {
namespace platform {
namespace tracing {

// This class serves as a base class for emitting events to system event
// controllers: ETW for Windows, Signposts on Mac (to be implemented). It is
// enabled by turning on both the ENABLE_SYSTEM_INSTRUMENTATION build flag and
// the --enable-system-instrumentation command line flag. When enabled, it is
// called from within SystemInstrumentationTraceWriter and replaces the
// JSONTraceWriter for event-tracing.
class V8_PLATFORM_EXPORT Recorder {
 public:
  Recorder();
  ~Recorder();

  bool IsEnabled();
  bool IsEnabled(const uint8_t level);

  void AddEvent(TraceObject* trace_event);

 private:
#if V8_OS_DARWIN
  os_log_t v8Provider;
#endif
};

}  // namespace tracing
}  // namespace platform
}  // namespace v8

#if V8_OS_DARWIN
#pragma clang diagnostic pop
#endif

#endif  // V8_LIBPLATFORM_TRACING_RECORDER_H_
