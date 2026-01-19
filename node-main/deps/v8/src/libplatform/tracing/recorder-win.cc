// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_LIBPLATFORM_TRACING_RECORDER_WIN_H_
#define V8_LIBPLATFORM_TRACING_RECORDER_WIN_H_

#include "src/libplatform/etw/etw-provider-win.h"
#include "src/libplatform/tracing/recorder.h"

namespace v8 {
namespace platform {
namespace tracing {

V8_DECLARE_TRACELOGGING_PROVIDER(g_v8LibProvider);
V8_DEFINE_TRACELOGGING_PROVIDER(g_v8LibProvider);

Recorder::Recorder() { TraceLoggingRegister(g_v8LibProvider); }

Recorder::~Recorder() {
  if (g_v8LibProvider) {
    TraceLoggingUnregister(g_v8LibProvider);
  }
}

bool Recorder::IsEnabled() {
  return TraceLoggingProviderEnabled(g_v8LibProvider, 0, 0);
}

bool Recorder::IsEnabled(const uint8_t level) {
  return TraceLoggingProviderEnabled(g_v8LibProvider, level, 0);
}

void Recorder::AddEvent(TraceObject* trace_event) {
  // TODO(sartang@microsoft.com): Figure out how to write the conditional
  // arguments
  wchar_t wName[4096];
  MultiByteToWideChar(CP_ACP, 0, trace_event->name(), -1, wName, 4096);

#if defined(V8_USE_PERFETTO)
  const wchar_t* wCategoryGroupName = L"";
#else  // defined(V8_USE_PERFETTO)
  wchar_t wCategoryGroupName[4096];
  MultiByteToWideChar(CP_ACP, 0,
                      TracingController::GetCategoryGroupName(
                          trace_event->category_enabled_flag()),
                      -1, wCategoryGroupName, 4096);
#endif  // !defined(V8_USE_PERFETTO)

  TraceLoggingWrite(g_v8LibProvider, "", TraceLoggingValue(wName, "Event Name"),
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
