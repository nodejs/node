// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef V8_LIBPLATFORM_TRACING_RECORDER_WIN_H_
#define V8_LIBPLATFORM_TRACING_RECORDER_WIN_H_

#include <windows.h>
#include <TraceLoggingProvider.h>

#include "src/libplatform/tracing/recorder.h"

#if defined(__clang__)
#pragma clang diagnostic ignored "-Wc++98-compat-extra-semi"
#endif

#ifndef V8_ETW_GUID
#define V8_ETW_GUID \
  0x57277741, 0x3638, 0x4A4B, 0xBD, 0xBA, 0x0A, 0xC6, 0xE4, 0x5D, 0xA5, 0x6C
#endif

namespace v8 {
namespace platform {
namespace tracing {

TRACELOGGING_DECLARE_PROVIDER(g_v8Provider);

TRACELOGGING_DEFINE_PROVIDER(g_v8Provider, "V8.js", (V8_ETW_GUID));

Recorder::Recorder() { TraceLoggingRegister(g_v8Provider); }

Recorder::~Recorder() {
  if (g_v8Provider) {
    TraceLoggingUnregister(g_v8Provider);
  }
}

bool Recorder::IsEnabled() {
  return TraceLoggingProviderEnabled(g_v8Provider, 0, 0);
}

bool Recorder::IsEnabled(const uint8_t level) {
  return TraceLoggingProviderEnabled(g_v8Provider, level, 0);
}

void Recorder::AddEvent(TraceObject* trace_event) {
  // TODO(sartang@microsoft.com): Figure out how to write the conditional
  // arguments
  wchar_t* wName = new wchar_t[4096];
  MultiByteToWideChar(CP_ACP, 0, trace_event->name(), -1, wName, 4096);

  wchar_t* wCategoryGroupName = new wchar_t[4096];
  MultiByteToWideChar(CP_ACP, 0,
                      TracingController::GetCategoryGroupName(
                          trace_event->category_enabled_flag()),
                      -1, wCategoryGroupName, 4096);

  TraceLoggingWrite(g_v8Provider, "", TraceLoggingValue(wName, "Event Name"),
                    TraceLoggingValue(trace_event->pid(), "pid"),
                    TraceLoggingValue(trace_event->tid(), "tid"),
                    TraceLoggingValue(trace_event->ts(), "ts"),
                    TraceLoggingValue(trace_event->tts(), "tts"),
                    TraceLoggingValue(trace_event->phase(), "phase"),
                    TraceLoggingValue(wCategoryGroupName, "category"),
                    TraceLoggingValue(trace_event->duration(), "dur"),
                    TraceLoggingValue(trace_event->cpu_duration(), "tdur"));
}

}  // namespace tracing
}  // namespace platform
}  // namespace v8

#endif  // V8_LIBPLATFORM_TRACING_RECORDER_WIN_H_
