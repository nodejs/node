// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef V8_LIBPLATFORM_TRACING_RECORDER_DEFAULT_H_
#define V8_LIBPLATFORM_TRACING_RECORDER_DEFAULT_H_

#include "src/libplatform/tracing/recorder.h"

namespace v8 {
namespace platform {
namespace tracing {

Recorder::Recorder() {}
Recorder::~Recorder() {}

bool Recorder::IsEnabled() { return false; }
bool Recorder::IsEnabled(const uint8_t level) { return false; }

void Recorder::AddEvent(TraceObject* trace_event) {}

}  // namespace tracing
}  // namespace platform
}  // namespace v8

#endif  // V8_LIBPLATFORM_TRACING_RECORDER_DEFAULT_H_
